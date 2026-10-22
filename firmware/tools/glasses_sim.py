#!/usr/bin/env python3
"""Standalone glasses-side simulator for the smart-glasses charger.

Answers the case's AT protocol frames on a USB-TTL adapter wired to the
MCU's USART0 (PA10=RX to adapter TX, PA9=TX to adapter RX, GND; 3.3V
levels), bypassing the POGO analog chain exactly like the old HIL bed
did. For the firmware this is indistinguishable from glasses on the
pins: handshake, charge polling, LED policy, force-charge gating and OTA
all run their real code paths.

Every chunk the adapter receives is logged as a RAW line (valid frames
or not) and everything tees to --log-file (default sim_log.txt, append
mode) so a whole bench session can be handed over for analysis.
NO RAW LINES AT ALL = the adapter RX is not connected to the case TX.

Bench scenarios:
  normal presence        python glasses_sim.py --port COM5 --soc 50
  full glasses           --soc 100 --full
  seen-then-silent       run it, let a handshake succeed, then Ctrl-C and
                         re-open the lid (the only path that triggers
                         FORCE_CHARGING under option A)
  dead glasses           do not run it, or --deaf: log but never answer
  OTA serving            --ota-fw app.bin --version 1 (version mismatch
                         on the heartbeat triggers the case's OTA request)

Frame format (all multi-byte fields little-endian):
  magic(4) crc8(1) size(2) opcode(2) status(1) payload(size)
  REQ magic 23 54 41 23, RSP magic 23 50 41 23; the CRC covers ONLY the
  bytes after the CRC byte (the corrected span, matches firmware/sgc_at).

Requires pyserial only. Run with --help for options.
"""

import argparse
import struct
import sys
import time
from datetime import datetime

try:
    import serial
except ImportError:
    sys.exit("pyserial missing: pip install pyserial")

REQ_MAGIC = bytes([0x23, 0x54, 0x41, 0x23])
RSP_MAGIC = bytes([0x23, 0x50, 0x41, 0x23])
HEADER_SIZE = 10

OP_HEART = 0x3001
OP_SHUTDOWN = 0x3002
OP_PREPARE = 0x3003
OP_READ = 0x3004

ROLE_CASE = 0
ROLE_GLASS = 1

_CRC_TABLE = bytes([
    0x00, 0x31, 0x62, 0x53, 0xC4, 0xF5, 0xA6, 0x97, 0xB9, 0x88, 0xDB, 0xEA, 0x7D, 0x4C, 0x1F, 0x2E,
    0x43, 0x72, 0x21, 0x10, 0x87, 0xB6, 0xE5, 0xD4, 0xFA, 0xCB, 0x98, 0xA9, 0x3E, 0x0F, 0x5C, 0x6D,
    0x86, 0xB7, 0xE4, 0xD5, 0x42, 0x73, 0x20, 0x11, 0x3F, 0x0E, 0x5D, 0x6C, 0xFB, 0xCA, 0x99, 0xA8,
    0xC5, 0xF4, 0xA7, 0x96, 0x01, 0x30, 0x63, 0x52, 0x7C, 0x4D, 0x1E, 0x2F, 0xB8, 0x89, 0xDA, 0xEB,
    0x3D, 0x0C, 0x5F, 0x6E, 0xF9, 0xC8, 0x9B, 0xAA, 0x84, 0xB5, 0xE6, 0xD7, 0x40, 0x71, 0x22, 0x13,
    0x7E, 0x4F, 0x1C, 0x2D, 0xBA, 0x8B, 0xD8, 0xE9, 0xC7, 0xF6, 0xA5, 0x94, 0x03, 0x32, 0x61, 0x50,
    0xBB, 0x8A, 0xD9, 0xE8, 0x7F, 0x4E, 0x1D, 0x2C, 0x02, 0x33, 0x60, 0x51, 0xC6, 0xF7, 0xA4, 0x95,
    0xF8, 0xC9, 0x9A, 0xAB, 0x3C, 0x0D, 0x5E, 0x6F, 0x41, 0x70, 0x23, 0x12, 0x85, 0xB4, 0xE7, 0xD6,
    0x7A, 0x4B, 0x18, 0x29, 0xBE, 0x8F, 0xDC, 0xED, 0xC3, 0xF2, 0xA1, 0x90, 0x07, 0x36, 0x65, 0x54,
    0x39, 0x08, 0x5B, 0x6A, 0xFD, 0xCC, 0x9F, 0xAE, 0x80, 0xB1, 0xE2, 0xD3, 0x44, 0x75, 0x26, 0x17,
    0xFC, 0xCD, 0x9E, 0xAF, 0x38, 0x09, 0x5A, 0x6B, 0x45, 0x74, 0x27, 0x16, 0x81, 0xB0, 0xE3, 0xD2,
    0xBF, 0x8E, 0xDD, 0xEC, 0x7B, 0x4A, 0x19, 0x28, 0x06, 0x37, 0x64, 0x55, 0xC2, 0xF3, 0xA0, 0x91,
    0x47, 0x76, 0x25, 0x14, 0x83, 0xB2, 0xE1, 0xD0, 0xFE, 0xCF, 0x9C, 0xAD, 0x3A, 0x0B, 0x58, 0x69,
    0x04, 0x35, 0x66, 0x57, 0xC0, 0xF1, 0xA2, 0x93, 0xBD, 0x8C, 0xDF, 0xEE, 0x79, 0x48, 0x1B, 0x2A,
    0xC1, 0xF0, 0xA3, 0x92, 0x05, 0x34, 0x67, 0x56, 0x78, 0x49, 0x1A, 0x2B, 0xBC, 0x8D, 0xDE, 0xEF,
    0x82, 0xB3, 0xE0, 0xD1, 0x46, 0x77, 0x24, 0x15, 0x3B, 0x0A, 0x59, 0x68, 0xFF, 0xCE, 0x9D, 0xAC])

_LOG_FILE = None  # optional tee target, opened by main()


def emit(line: str) -> None:
    print(line, flush=True)
    if _LOG_FILE is not None:
        _LOG_FILE.write(line + "\n")
        _LOG_FILE.flush()


def crc8(data: bytes) -> int:
    crc = 0
    for b in data:
        crc = _CRC_TABLE[b ^ crc]
    return crc


def pack_rsp(opcode: int, status: int, payload: bytes) -> bytes:
    body = struct.pack("<HHB", len(payload), opcode, status) + payload
    return RSP_MAGIC + bytes([crc8(body)]) + body


def log(tag: str, data: bytes, note: str = "") -> None:
    t = time.strftime("%H:%M:%S")
    emit("[%s] %s %s%s" % (t, tag, data.hex(" ")[:72], (" " + note) if note else ""))


class Sim:
    def __init__(self, args):
        self.args = args
        self.fw = open(args.ota_fw, "rb").read() if args.ota_fw else None

    def heartbeat_rsp(self, req_payload: bytes):
        case_sta = req_payload[3] if len(req_payload) >= 4 else 0
        ota_requested = (case_sta & 0x80) != 0
        agree = ota_requested and self.fw is not None
        glass_soc = ((0x80 if self.args.full else 0) | (self.args.soc & 0x7F))
        payload = bytes([ROLE_CASE, ROLE_GLASS, glass_soc, (0x80 if agree else 0), self.args.version])
        note = "soc=%d%% full=%s ver=%d" % (self.args.soc, self.args.full, self.args.version)
        if ota_requested:
            note += " ota_req -> %s" % ("AGREE" if agree else "REFUSE")
        return pack_rsp(OP_HEART, 0x00, payload), note

    def serve(self, opcode: int, payload: bytes):
        if opcode == OP_HEART:
            return self.heartbeat_rsp(payload)
        if opcode == OP_SHUTDOWN:
            return pack_rsp(OP_SHUTDOWN, 0x00, bytes([ROLE_CASE, ROLE_GLASS])), "shutdown ACK"
        if opcode == OP_PREPARE:
            rsp = pack_rsp(OP_PREPARE, 0x00, struct.pack("<BBI", ROLE_CASE, ROLE_GLASS, len(self.fw)))
            return rsp, "prepare size=%d" % len(self.fw)
        if opcode == OP_READ:
            _, _, index, size = struct.unpack("<BBHH", payload[:6])
            chunk = self.fw[index * size:(index + 1) * size]
            is_end = (index + 1) * size >= len(self.fw)
            body = struct.pack("<BBHB", ROLE_CASE, ROLE_GLASS, index, 1 if is_end else 0) + chunk
            return pack_rsp(OP_READ, 0x00, body), "read idx=%d len=%d%s" % (index, len(chunk), " END" if is_end else "")
        return None, "unhandled opcode 0x%04X" % opcode


def run(args):
    sim = Sim(args)
    ser = serial.Serial(args.port, args.baud, timeout=0.02)
    emit("glasses sim on %s@%d: soc=%d full=%s ver=%d fw=%s deaf=%s latency=%dms -- Ctrl-C to go silent"
         % (args.port, args.baud, args.soc, args.full, args.version,
            ("%dB" % len(sim.fw)) if sim.fw else "none", args.deaf, args.latency))
    buf = b""
    while True:
        chunk = ser.read(64)
        if chunk:
            # Raw byte trace: EVERYTHING the adapter sees, valid frames or
            # not. No RAW lines at all = adapter RX not on the case TX.
            ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            emit("[%s] RAW %3d B: %s" % (ts, len(chunk), chunk.hex(" ")))
        buf += chunk
        while True:
            start = buf.find(REQ_MAGIC)
            if start < 0:
                buf = buf[-3:]  # keep a possible partial magic tail
                break
            if len(buf) < start + HEADER_SIZE:
                break
            size = struct.unpack("<H", buf[start + 5:start + 7])[0]
            total = HEADER_SIZE + size
            if len(buf) < start + total:
                break
            frame = buf[start:start + total]
            buf = buf[start + total:]
            opcode = struct.unpack("<H", frame[7:9])[0]
            ok = crc8(frame[5:total]) == frame[4]
            log("REQ ", frame, "op=0x%04X crc=%s" % (opcode, "OK" if ok else "BAD"))
            if args.deaf or not ok:
                continue
            rsp, note = sim.serve(opcode, frame[HEADER_SIZE:])
            if rsp is None:
                continue
            if args.latency:
                time.sleep(args.latency / 1000.0)
            ser.write(rsp)
            log("RSP ", rsp, note)


def selftest() -> int:
    """Validate the packer against frames captured from real hardware."""
    # Case REQ captured on the wire: heartbeat, soc=100, lid open -> crc 0x9D
    req_body = bytes([0x04, 0x00, 0x01, 0x30, 0x00, 0x01, 0x00, 0x64, 0x01])
    assert crc8(req_body) == 0x9D, "REQ crc mismatch"
    # Glasses RSP captured on the wire: glass 86%, not full, ver 0 -> crc 0xD2
    want = bytes([0x23, 0x50, 0x41, 0x23, 0xD2, 0x05, 0x00, 0x01, 0x30, 0x00, 0x00, 0x01, 0x56, 0x00, 0x00])
    got = pack_rsp(OP_HEART, 0x00, bytes([0x00, 0x01, 0x56, 0x00, 0x00]))
    assert got == want, "RSP mismatch: " + got.hex()
    print("selftest OK: CRC span and RSP packing match the real-glasses captures")
    return 0


def main():
    ap = argparse.ArgumentParser(description="glasses-side charger bench simulator")
    ap.add_argument("--port", help="serial port, e.g. COM5")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--soc", type=int, default=50, help="reported glasses SOC 0-100")
    ap.add_argument("--full", action="store_true", help="report glasses full (bit7)")
    ap.add_argument("--version", type=int, default=0, help="case_version byte mirrored to the case")
    ap.add_argument("--ota-fw", help="firmware file to serve over OTA (enables agree)")
    ap.add_argument("--deaf", action="store_true", help="log frames but never answer (dead glasses)")
    ap.add_argument("--latency", type=int, default=5, help="reply delay in ms (reply window is 100)")
    ap.add_argument("--log-file", default="sim_log.txt",
                    help="tee everything (raw bytes + frames) to this file, append mode")
    ap.add_argument("--selftest", action="store_true", help="validate packer against captures and exit")
    args = ap.parse_args()
    global _LOG_FILE
    _LOG_FILE = open(args.log_file, "a", encoding="utf-8")
    emit("=== session start: %s ===" % time.strftime("%Y-%m-%d %H:%M:%S"))
    if args.selftest:
        rc = selftest()
        _LOG_FILE.close()
        sys.exit(rc)
    if not args.port:
        ap.error("--port is required (or --selftest)")
    try:
        run(args)
    except KeyboardInterrupt:
        emit("sim gone silent (seen-then-silent scenario)")
    finally:
        if _LOG_FILE is not None:
            _LOG_FILE.close()


if __name__ == "__main__":
    main()
