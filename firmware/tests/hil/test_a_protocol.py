"""A 类 — AT 协议帧格式（P0）.

验证固件发出的心跳帧的 Magic / Size / Opcode / Status / CRC / Payload 结构。
前置：人工触发开盖（磁铁碰霍尔），固件进 HANDSHAKING 后会发心跳请求帧。
"""
import struct
import time
import pytest
import sgc_at


def test_a01_request_magic(heartbeat_frame):
    magic = struct.unpack_from(">I", heartbeat_frame, 0)[0]
    assert magic == sgc_at.MAGIC_REQ, f"magic={magic:#010x}, expected {sgc_at.MAGIC_REQ:#010x}"


def test_a03_header_size_field(heartbeat_frame):
    size = struct.unpack_from(">H", heartbeat_frame, 5)[0]
    assert size == len(heartbeat_frame), f"size field={size}, actual={len(heartbeat_frame)}"
    assert size == sgc_at.HEADER_SIZE + 4, f"心跳帧总长应为 14 字节(header 10 + payload 4)，实际 {size}"


def test_a04_crc_correct(heartbeat_frame):
    expected = sgc_at.frame_crc(heartbeat_frame)
    actual = heartbeat_frame[4]
    assert actual == expected, f"CRC byte={actual:#04x}, computed={expected:#04x}"


def test_a07_opcode_heartbeat(heartbeat_frame):
    opcode = struct.unpack_from(">H", heartbeat_frame, 7)[0]
    assert opcode == sgc_at.OPCODE_CASE_HEART, f"opcode={opcode:#06x}, expected heartbeat 0x3001"


def test_a11_heartbeat_payload_structure(heartbeat_frame):
    parsed = sgc_at.parse_frame(heartbeat_frame)
    payload = parsed["payload"]
    assert len(payload) == 4, f"心跳 payload 应为 4 字节(role.des + role.src + case_soc + case_sta)，实际 {len(payload)}"
    des, src, case_soc, case_sta = payload[0], payload[1], payload[2], payload[3]
    assert des == sgc_at.ROLE_GLASS, f"des={des}, 应为 GLASS(1) — 心跳发给眼镜"
    assert src == sgc_at.ROLE_CASE, f"src={src}, 应为 CASE(0) — 盒子发出"


def test_a13_case_sta_lid_bit(heartbeat_frame):
    parsed = sgc_at.parse_frame(heartbeat_frame)
    case_sta = parsed["payload"][3]
    bit0_lid_open = (case_sta & 0x01) != 0
    assert bit0_lid_open, f"case_sta={case_sta:#04x}, bit0 应为 1（开盖触发的心跳）"


@pytest.mark.flaky(reruns=3, reruns_delay=2)
def test_a15_glass_full_enters_maintaining(serial_port):
    """A15: PC 回 glass_soc=0xE4 (眼镜满电) → 固件进 MAINTAINING (开盖心跳 ~1s).

    按矩阵 A15: glass_soc bit7=1 (0xE4=满电) → "眼镜满电"分支.
    开盖 → MAINTAINING (心跳 1s); 关盖 → SHUTTING_DOWN (C07 覆盖).

    通过心跳间隔判断状态:
      ~200ms = 还在 retry (握手没成功)
      ~1000ms = MAINTAINING ✓
      ~30000ms = CHARGING (没识别 bit7=1)
    """
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()
    sgc_at.send_command(serial_port, "RESET")
    time.sleep(0.5)
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()
    sgc_at.send_command(serial_port, "OPEN")

    # 持续发满电响应, 直到收到两个连续心跳测间隔.
    # 类似 _enter_charging 的写法: 多次发响应覆盖固件 retry 窗口.
    response = sgc_at.pack_heartbeat_response(
        glass_soc=0xE4, glass_sta=0x00, case_version=sgc_at.CASE_FW_VERSION
    )

    serial_port.timeout = 0.1
    last = time.time()
    deadline = time.time() + 8.0
    interval_ms = 0.0
    while time.time() < deadline:
        serial_port.write(response)
        f = sgc_at.recv_request(serial_port, timeout=2.0)
        if f is None:
            continue
        gap_ms = (time.time() - last) * 1000
        if gap_ms > 500:
            # 上一帧到这帧间隔 > 500ms, 说明已离开 retry (~200ms) 进稳定状态.
            interval_ms = gap_ms
            break
        last = time.time()
    serial_port.timeout = 2.0

    assert interval_ms > 0, "8s 内未观察到 > 500ms 的心跳间隔 (固件一直在 retry)"
    assert interval_ms < 2000, (
        f"间隔 {interval_ms:.0f}ms > 2000ms, 超出 MAINTAINING 1s 周期. "
        f"可能进了 CHARGING (没识别 glass_soc bit7=1 满电语义)."
    )


def test_a05_crc_range(heartbeat_frame):
    """A05: CRC 覆盖范围 = magic(4) + CRC byte 之后(5+)，不含 byte 4 自身."""
    original_crc = heartbeat_frame[4]

    # 重算 CRC 必须等于帧里的存的 CRC 字节。
    recomputed = sgc_at.frame_crc(heartbeat_frame)
    assert recomputed == original_crc, (
        f"recomputed CRC {recomputed:#04x} != frame[4] {original_crc:#04x}"
    )

    # Flip 一个 payload bit，CRC 必须变（证明 payload 在覆盖范围内）。
    flipped = bytearray(heartbeat_frame)
    flipped[10] ^= 0x01  # payload 第一字节 LSB
    flipped_crc = sgc_at.frame_crc(bytes(flipped))
    assert flipped_crc != original_crc, (
        f"flipping payload bit did not change CRC "
        f"(orig={original_crc:#04x}, flipped={flipped_crc:#04x})"
    )

    # 改 byte 4 自身 CRC 应不变（byte 4 不在 CRC 输入里）。
    byte4_modified = bytearray(heartbeat_frame)
    byte4_modified[4] ^= 0xFF
    byte4_crc = sgc_at.frame_crc(bytes(byte4_modified))
    assert byte4_crc == original_crc, (
        f"changing byte 4 altered CRC ({byte4_crc:#04x} != {original_crc:#04x}) — "
        f"byte 4 should not be part of CRC input"
    )


def test_a14_response_payload_structure():
    """A14: 心跳响应 payload = [ROLE_GLASS, ROLE_CASE, glass_soc, glass_sta, case_version]."""
    response = sgc_at.pack_heartbeat_response(
        glass_soc=0x20, glass_sta=0x00, case_version=sgc_at.CASE_FW_VERSION
    )
    # Header 10 字节 (MAGIC 4 + CRC 1 + SIZE 2 + OPCODE 2 + STATUS 1)；
    # payload 从 offset 10 开始，长度 5。
    assert len(response) == 15, f"response len={len(response)}, expected 15 (header 10 + payload 5)"
    payload = response[10:15]
    expected = bytes([
        sgc_at.ROLE_GLASS,  # des=1
        sgc_at.ROLE_CASE,   # src=0
        0x20,               # glass_soc
        0x00,               # glass_sta
        sgc_at.CASE_FW_VERSION,  # case_version
    ])
    assert payload == expected, (
        f"payload={payload.hex()}, expected={expected.hex()}"
    )
