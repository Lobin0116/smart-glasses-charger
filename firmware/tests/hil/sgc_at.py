"""Smart Glasses Charger AT protocol — Python mirror of firmware src/protocol/.

Covers frame format, CRC8, opcodes, status codes. Used by HIL tests to
assemble responses (PC simulating glasses) and parse requests from firmware.
"""
import os
import re
import struct
import time

MAGIC_REQ = 0x23415423
MAGIC_RSP = 0x23415023
HEADER_SIZE = 10
MAX_PAYLOAD = 255


def _parse_case_fw_version() -> int:
    """Read CASE_FW_VERSION from firmware/src/app/fw_version.h.

    Keeps the value PC reports in heartbeat responses in sync with the
    firmware's OTA version-mismatch trigger (state_machine.c:100).
    """
    fw_version_h = os.path.join(
        os.path.dirname(__file__), "..", "..", "src", "app", "fw_version.h"
    )
    with open(fw_version_h, "r", encoding="utf-8") as f:
        for line in f:
            m = re.match(
                r"\s*#define\s+CASE_FW_VERSION\s+(0x[0-9A-Fa-f]+|\d+)", line
            )
            if m:
                return int(m.group(1), 0)
    raise RuntimeError(f"CASE_FW_VERSION not found in {fw_version_h}")


CASE_FW_VERSION = _parse_case_fw_version()

OPCODE_CASE_HEART = 0x3001
OPCODE_CASE_SHUTDOWN = 0x3002
OPCODE_CASE_PACKET_PREPARE = 0x3003
OPCODE_CASE_PACKET_READ = 0x3004

# HIL test opcodes (firmware at_opcode.h AT_OPCODE_HIL_*). Same frame format
# as production opcodes; firmware update_mode_poll dispatches these.
OPCODE_HIL_RESET = 0x3010
OPCODE_HIL_OPEN = 0x3011
OPCODE_HIL_CLOSE = 0x3012
OPCODE_HIL_KEY = 0x3013
OPCODE_HIL_STATUS = 0x3014
OPCODE_HIL_SCAN = 0x3015
OPCODE_HIL_OTA = 0x3016

# HIL STATUS ACK payload (must match firmware hil_status_payload_t, 17 bytes).
# All multi-byte fields big-endian.
HIL_STATUS_FMT = ">BBBBBBBBBBBBBBBBB"

AT_SUCCESS = 0x00

ROLE_CASE = 0
ROLE_GLASS = 1
ROLE_MAGBAG = 2

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
    0x82, 0xB3, 0xE0, 0xD1, 0x46, 0x77, 0x24, 0x15, 0x3B, 0x0A, 0x59, 0x68, 0xFF, 0xCE, 0x9D, 0xAC,
])


def crc8(data: bytes, init: int = 0x00) -> int:
    crc = init
    for b in data:
        crc = _CRC_TABLE[b ^ crc]
    return crc


def frame_crc(buf: bytes) -> int:
    crc = crc8(buf[0:4], 0x00)
    crc = crc8(buf[5:], crc)
    return crc


def pack_request(opcode: int, payload: bytes = b"", reserved: int = 0x00) -> bytes:
    total_len = HEADER_SIZE + len(payload)
    buf = bytearray(total_len)
    struct.pack_into(">I", buf, 0, MAGIC_REQ)
    struct.pack_into(">H", buf, 5, total_len)
    struct.pack_into(">H", buf, 7, opcode)
    buf[9] = reserved
    buf[10:] = payload
    buf[4] = frame_crc(bytes(buf))
    return bytes(buf)


def pack_response(opcode: int, status: int = AT_SUCCESS, payload: bytes = b"") -> bytes:
    total_len = HEADER_SIZE + len(payload)
    buf = bytearray(total_len)
    struct.pack_into(">I", buf, 0, MAGIC_RSP)
    struct.pack_into(">H", buf, 5, total_len)
    struct.pack_into(">H", buf, 7, opcode)
    buf[9] = status
    buf[10:] = payload
    buf[4] = frame_crc(bytes(buf))
    return bytes(buf)


def parse_frame(buf: bytes):
    if len(buf) < HEADER_SIZE:
        raise ValueError(f"frame too short: {len(buf)} bytes")
    magic = struct.unpack_from(">I", buf, 0)[0]
    size = struct.unpack_from(">H", buf, 5)[0]
    opcode = struct.unpack_from(">H", buf, 7)[0]
    status = buf[9]
    if size != len(buf):
        raise ValueError(f"size field ({size}) != actual length ({len(buf)})")
    expected_crc = frame_crc(buf[:size])
    if buf[4] != expected_crc:
        raise ValueError(f"CRC mismatch: byte={buf[4]:#04x} computed={expected_crc:#04x}")
    payload = buf[HEADER_SIZE:size]
    return {"magic": magic, "opcode": opcode, "status": status, "payload": payload}


def pack_heartbeat_response(glass_soc: int, glass_sta: int, case_version: int = 0,
                            glass_full: bool = None, ota_agree: bool = None) -> bytes:
    if glass_full is not None:
        glass_soc = (glass_soc & 0x7F) | (0x80 if glass_full else 0x00)
    if ota_agree is not None:
        glass_sta = (glass_sta & 0x7F) | (0x80 if ota_agree else 0x00)
    payload = bytes([ROLE_GLASS, ROLE_CASE, glass_soc & 0xFF, glass_sta & 0xFF, case_version & 0xFF])
    return pack_response(OPCODE_CASE_HEART, AT_SUCCESS, payload)


def pack_prepare_response(fw_size: int) -> bytes:
    """Reply to 0x3003 PREPARE: tell the case the firmware image size in bytes."""
    payload = bytes([ROLE_GLASS, ROLE_CASE]) + struct.pack("<I", fw_size & 0xFFFFFFFF)
    return pack_response(OPCODE_CASE_PACKET_PREPARE, AT_SUCCESS, payload)


def pack_read_response(index: int, data: bytes, packet_type: int = 0) -> bytes:
    """Reply to 0x3004 READ: return one firmware block.
    packet_type: 0=MID, 1=END (see at_packet_type_e)."""
    payload = bytes([ROLE_GLASS, ROLE_CASE]) + struct.pack("<H", index & 0xFFFF) + bytes([packet_type & 0xFF]) + data
    return pack_response(OPCODE_CASE_PACKET_READ, AT_SUCCESS, payload)


# Persistent RX buffer across recv_request calls. pyserial read(n) pulls bytes
# out of the OS buffer; if a single read grabs multiple frames, the bytes past
# the first parsed frame would be lost when recv_request returns (its local buf
# drops on return). Keeping them here lets the next call resume from where the
# last one stopped.
_recv_buf = bytearray()

_MAGIC_REQ_BYTES = struct.pack(">I", MAGIC_REQ)


def recv_request(ser, timeout: float = 5.0, magic: int = MAGIC_REQ) -> bytes:
    """Read bytes from serial until a complete, CRC-valid frame is found.
    Pass magic=MAGIC_RSP to receive response frames (HIL ACKs)."""
    old_timeout = ser.timeout
    ser.timeout = 0.1  # short poll so the end_time check actually runs
    magic_bytes = struct.pack(">I", magic)
    try:
        end_time = time.time() + timeout
        while time.time() < end_time:
            idx = _recv_buf.find(magic_bytes)
            while idx >= 0:
                if len(_recv_buf) - idx < 7:
                    break  # header not complete yet — wait, do NOT drop magic
                size = struct.unpack_from(">H", _recv_buf, idx + 5)[0]
                if size < HEADER_SIZE:
                    # Bogus size field — drop the magic lead and re-hunt.
                    del _recv_buf[:idx + 4]
                    idx = _recv_buf.find(magic_bytes)
                    continue
                if len(_recv_buf) - idx < size:
                    break  # frame not complete yet — wait, do NOT drop magic
                frame = bytes(_recv_buf[idx:idx + size])
                try:
                    parse_frame(frame)
                    del _recv_buf[:idx + size]
                    return frame
                except ValueError:
                    # CRC/magic mismatch — not a real frame, drop lead and re-hunt.
                    del _recv_buf[:idx + 4]
                    idx = _recv_buf.find(magic_bytes)
                    continue
            # Read whatever has already arrived (don't block waiting for 64 bytes
            # that may never come). When the OS buffer is empty, read(1) blocks
            # on the next byte arrival bounded by the 0.1s timeout set above.
            in_wait = ser.in_waiting
            chunk = ser.read(in_wait if in_wait > 0 else 1)
            if chunk:
                _recv_buf.extend(chunk)
        return None
    finally:
        ser.timeout = old_timeout


def recv_response(ser, timeout: float = 5.0) -> bytes:
    """Alias for recv_request with magic=MAGIC_RSP (HIL ACK frames)."""
    return recv_request(ser, timeout=timeout, magic=MAGIC_RSP)


def reset_recv_buffer() -> None:
    """Drop any buffered RX bytes (e.g. between unrelated test phases)."""
    _recv_buf.clear()


def pack_hil_command(opcode: int) -> bytes:
    """Pack a HIL REQ frame. Payload is the 2-byte role prefix matching the
    production at_case_role layout (des=GLASS, src=CASE)."""
    return pack_request(opcode, bytes([ROLE_GLASS, ROLE_CASE]))


def send_hil_command(ser, opcode: int, timeout: float = 5.0, retries: int = 5):
    """Send a HIL command and wait for its ACK (same opcode, RSP magic).
    Returns the parsed ACK payload bytes, or None on timeout. Default timeout
    is 5s so a command that lands while firmware is mid-handshake (which
    blocks update_mode_poll ~1.5s per attempt) still gets its ACK during a
    handshake gap — a 2s timeout straddles the gap edge and times out."""
    for _ in range(retries):
        ser.write(pack_hil_command(opcode))
        ser.flush()
        end = time.time() + timeout
        while time.time() < end:
            f = recv_response(ser, timeout=1.0)
            if f is None:
                continue
            try:
                p = parse_frame(f)
            except ValueError:
                continue
            if p["opcode"] == opcode and p["status"] == AT_SUCCESS:
                return p["payload"]
    return None


def parse_hil_status(payload: bytes) -> dict:
    """Unpack HIL STATUS ACK payload (17 bytes packed)."""
    f = struct.unpack(HIL_STATUS_FMT, payload[:17])
    return {
        "ver": f[0], "soc_reg": f[1], "cfg": f[2],
        "sysclk_mhz": f[3], "ahb_mhz": f[4], "apb1_mhz": f[5],
        "case_soc": f[6], "state": f[7],
        "ota_index": (f[8] << 8) | f[9],
        "ota_rxlen": (f[10] << 8) | f[11],
        "ota_fails": (f[12] << 8) | f[13],
        "ota_fail_reason": f[14],
        "ota_succ_len": (f[15] << 8) | f[16],
    }


_HIL_CMD_OPCODES = {
    "RESET": OPCODE_HIL_RESET,
    "OPEN": OPCODE_HIL_OPEN,
    "CLOSE": OPCODE_HIL_CLOSE,
    "KEY": OPCODE_HIL_KEY,
    "STATUS": OPCODE_HIL_STATUS,
    "SCAN": OPCODE_HIL_SCAN,
    "OTA": OPCODE_HIL_OTA,
}


def send_command(ser, cmd: str):
    """Send a HIL command and wait for ACK. Confirms firmware's update_mode_poll
    actually processed the command (e.g. RESET → IDLE) before the caller
    continues — fire-and-forget left the caller blind to whether firmware was
    mid-force_charge_probe (1.5s blocking) and never serviced the command,
    which is how OPEN ended up landing in FORCE_CHARGING where it's a no-op."""
    opcode = _HIL_CMD_OPCODES.get(cmd.upper())
    if opcode is None:
        raise ValueError(f"unknown HIL command: {cmd!r}")
    return send_hil_command(ser, opcode)
