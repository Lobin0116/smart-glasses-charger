"""I 类 — OTA 升级测试.

PC 模拟眼镜端，触发盒子 OTA 流程。盒子握手 → CHARGING 后发 OTA 命令强制
ota_requested=true，状态机进 ST_OTA，ota_run 同步跑 prepare/read 协议。

I01 验证 OTA 触发链路（case_sta bit7 申请 → 同意 → 收到 PREPARE 请求）。
I02 完整烧录需要 App.bin 文件 + 板子重启重连，留作框架。
"""
import struct
import time

import sgc_at


def _enter_charging(serial_port):
    """RESET+OPEN → handshake response → CHARGING. Returns the initial heartbeat frame."""
    serial_port.reset_input_buffer()
    sgc_at.send_command(serial_port, "RESET")
    time.sleep(0.5)
    serial_port.reset_input_buffer()
    sgc_at.send_command(serial_port, "OPEN")
    frame = sgc_at.recv_request(serial_port, timeout=5.0)
    assert frame is not None, "OPEN 后 5s 内未收到心跳"
    # 持续回响应，直到心跳间隔拉长（firmware 离开 HANDSHAKING retry 进 MAINTAINING/CHARGING）
    import time as _t
    last = _t.time()
    while _t.time() - last < 4.0:
        serial_port.write(sgc_at.pack_heartbeat_response(
            glass_soc=0x20, glass_sta=0x00, case_version=0x01
        ))
        nxt = sgc_at.recv_request(serial_port, timeout=2.0)
        if nxt is None:
            break
        interval_ms = (_t.time() - last) * 1000
        if interval_ms > 500:
            # 心跳间隔 > 500ms = 已离开 HANDSHAKING (~200ms) 进 MAINTAINING (1s) 或 CHARGING (30s)
            break
        last = _t.time()
    serial_port.reset_input_buffer()
    return frame


def _await_opcode(serial_port, opcode, timeout, body_check=None):
    """Wait for a request frame with the given opcode. Returns the frame or None."""
    end = time.time() + timeout
    while time.time() < end:
        f = sgc_at.recv_request(serial_port, timeout=1.0)
        if f is None:
            continue
        got = struct.unpack_from(">H", f, 7)[0]
        if got == opcode and (body_check is None or body_check(f)):
            return f
    return None


def test_i01_ota_trigger_reaches_prepare(serial_port):
    """I01: OTA cmd → case heartbeat with case_sta bit7=1 → agree → PREPARE.

    Triggered via update_mode OTA command (forces ota_requested). sm_tick_charging
    sees it next tick, enters ST_OTA, ota_run calls ota_request which sends a
    heartbeat with the OTA bit set; PC agrees (glass_sta bit7=1); case then
    sends opcode 0x3003 PREPARE. Reply size=0 so ota_prepare returns fw_size=0
    and ota_run bails out OTA_ERR_PREPARE (no real staging write).
    """
    _enter_charging(serial_port)
    sgc_at.send_command(serial_port, "OTA")
    time.sleep(0.3)
    reply = serial_port.read(64)
    assert b"OK_OTA" in reply, f"OTA 命令未回 OK_OTA，firmware 没收到命令。reply={reply!r}"

    # Wait for case heartbeat with case_sta bit7 (OTA request).
    def has_ota_bit(f):
        try:
            parsed = sgc_at.parse_frame(f)
        except ValueError:
            return False
        return (parsed["payload"][3] & 0x80) != 0

    ota_req = _await_opcode(serial_port, sgc_at.OPCODE_CASE_HEART, timeout=5.0, body_check=has_ota_bit)
    assert ota_req is not None, "OTA 命令后 5s 内未收到 case_sta bit7=1 心跳"

    # Agree → case proceeds to ota_prepare.
    serial_port.write(sgc_at.pack_heartbeat_response(
        glass_soc=0x20, glass_sta=0x00, case_version=0x01, ota_agree=True
    ))

    prepare = _await_opcode(serial_port, sgc_at.OPCODE_CASE_PACKET_PREPARE, timeout=3.0)
    assert prepare is not None, "同意 OTA 后 3s 内未收到 PREPARE 请求"

    # Reply size=0 so the case exits ota_run cleanly (OTA_ERR_PREPARE) instead
    # of staging a real image and resetting.
    serial_port.write(sgc_at.pack_prepare_response(0))


def test_i02_full_ota_with_app_bin(serial_port, tmp_path):
    """I02: push App.bin → case stages + resets → BL copies → new App runs.

    Skipped unless SGC_APP_BIN env var points to a built App.bin. The new image
    must differ observably from the running one (e.g. CASE_FW_VERSION bumped, or
    different USART beacon) so the test can confirm the swap.
    """
    import os
    import pytest
    app_bin = os.environ.get("SGC_APP_BIN")
    if not app_bin or not os.path.exists(app_bin):
        pytest.skip("set SGC_APP_BIN=<path> to a built App.bin to run this test")

    with open(app_bin, "rb") as f:
        fw = f.read()

    _enter_charging(serial_port)
    sgc_at.send_command(serial_port, "OTA")

    def has_ota_bit(f):
        parsed = sgc_at.parse_frame(f)
        return (parsed["payload"][3] & 0x80) != 0

    ota_req = _await_opcode(serial_port, sgc_at.OPCODE_CASE_HEART, timeout=5.0, body_check=has_ota_bit)
    assert ota_req is not None
    serial_port.write(sgc_at.pack_heartbeat_response(
        glass_soc=0x20, glass_sta=0x00, case_version=0x01, ota_agree=True
    ))

    prepare = _await_opcode(serial_port, sgc_at.OPCODE_CASE_PACKET_PREPARE, timeout=3.0)
    assert prepare is not None
    serial_port.write(sgc_at.pack_prepare_response(len(fw)))

    # Stream firmware in 240-byte blocks; last block carries type=END.
    BLOCK = 240
    index = 0
    while True:
        chunk = fw[index * BLOCK:(index + 1) * BLOCK]
        if len(chunk) < BLOCK:
            is_end = True
        else:
            is_end = False
        read_req = _await_opcode(serial_port, sgc_at.OPCODE_CASE_PACKET_READ, timeout=3.0)
        assert read_req is not None, f"未收到 READ index={index}"
        serial_port.write(sgc_at.pack_read_response(index, chunk, packet_type=1 if is_end else 0))
        if is_end:
            break
        index += 1

    # Case sets staged + resets. BL copies Staging → App on next boot.
    # Re-open serial after reset, then verify the new App runs (version beacon,
    # STATUS reply, etc.). Specifics depend on how the new image differs.
    serial_port.close()
    time.sleep(2.0)  # BL copy + App boot
    serial_port.open()
    # TODO: verify new App — e.g. STATUS reply differs, or version string.
