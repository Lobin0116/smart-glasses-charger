"""A 类 — AT 协议帧格式（P0）.

验证固件发出的心跳帧的 Magic / Size / Opcode / Status / CRC / Payload 结构。
前置：人工触发开盖（磁铁碰霍尔），固件进 HANDSHAKING 后会发心跳请求帧。
"""
import struct
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


def test_a15_send_response_accepted(serial_port, heartbeat_frame):
    parsed = sgc_at.parse_frame(heartbeat_frame)
    case_soc = parsed["payload"][2]
    glass_soc = (case_soc & 0x7F) | 0x80
    glass_sta = 0x00
    case_version = 0x01
    response = sgc_at.pack_heartbeat_response(
        glass_soc=glass_soc, glass_sta=glass_sta, case_version=case_version
    )
    serial_port.write(response)
    next_frame = sgc_at.recv_request(serial_port, timeout=3.0)
    assert next_frame is not None, "发合法响应后应在 ~30s 内（开盖充电周期）收到下一个心跳，但超时"
