"""I 类 — OTA 升级测试.

PC 模拟眼镜端，触发盒子 OTA 流程。盒子握手 → CHARGING 后发 OTA 命令强制
ota_requested=true，状态机进 ST_OTA，ota_run 同步跑 prepare/read 协议。

I01 验证 OTA 触发链路（case_sta bit7 申请 → 同意 → 收到 PREPARE 请求）。
I02 完整烧录需要 App.bin 文件 + 板子重启重连，留作框架。
"""
import struct
import time

import sgc_at


def _flush_rx(serial_port):
    """Clear both the OS-level serial RX buffer and sgc_at's internal buffer
    (which holds bytes already pulled out of the OS buffer but not yet parsed
    into a frame)."""
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()


def _enter_charging(serial_port):
    """RESET+OPEN → heartbeat → reply → wait for next heartbeat confirming
    firmware left HANDSHAKING (interval > 500ms = MAINTAINING 1s or CHARGING 30s).
    Keeps replying so retries see a response."""
    import time as _t
    _flush_rx(serial_port)
    sgc_at.send_command(serial_port, "RESET")
    time.sleep(2.0)  # fixture open resets firmware via DTR edge; wait for v10 boot
    _flush_rx(serial_port)
    sgc_at.send_command(serial_port, "OPEN")
    frame = sgc_at.recv_request(serial_port, timeout=5.0)
    assert frame is not None, "OPEN 后 5s 未收到心跳"

    response = sgc_at.pack_heartbeat_response(
        glass_soc=0x20, glass_sta=0x00, case_version=0
    )
    last = _t.time()
    deadline = _t.time() + 6.0
    while _t.time() < deadline:
        serial_port.write(response)
        nxt = sgc_at.recv_request(serial_port, timeout=2.0)
        if nxt is None:
            continue
        interval_ms = (_t.time() - last) * 1000
        if interval_ms > 500:
            # Satisfy the REQ that just triggered exit: sm_send_heartbeat is
            # still blocked in hal_usart_recv waiting for this reply. Without
            # it, the next command we send (e.g. OTA) gets consumed as a
            # malformed heartbeat response and update_mode_poll never sees it.
            _flush_rx(serial_port)
            serial_port.write(response)
            _t.sleep(0.15)  # > hal_usart_recv's 100ms byte-gap timeout
            _flush_rx(serial_port)
            return frame
        last = _t.time()
    assert False, "6s 内 firmware 未离开 HANDSHAKING（握手没成功，OTA 命令来了也无效）"


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
    # OTA command is now a HIL protocol frame (opcode 0x3016). send_command
    # waits for the ACK — at_frame_recv's expected_opcode filter keeps
    # charge_poll from swallowing it, so no retry loop needed.
    ack = sgc_at.send_command(serial_port, "OTA")
    assert ack is not None, "OTA 命令未收到 HIL ACK"
    # Short port timeout for the request loops below so ser.read() inside
    # recv_request returns promptly when bytes arrive.
    serial_port.timeout = 0.1

    # Wait for case heartbeat with case_sta bit7 (OTA request).
    end = time.time() + 5.0
    seen = []
    ota_req = None
    while time.time() < end:
        f = sgc_at.recv_request(serial_port, timeout=1.0)
        if f is None:
            continue
        seen.append(f)
        try:
            parsed = sgc_at.parse_frame(f)
        except ValueError:
            continue
        opcode = struct.unpack_from(">H", f, 7)[0]
        if opcode == sgc_at.OPCODE_CASE_HEART and (parsed["payload"][3] & 0x80):
            ota_req = f
            break

    if ota_req is None:
        debug = []
        for fr in seen[-8:]:
            try:
                p = sgc_at.parse_frame(fr)
                debug.append(f"op={p['opcode']:#06x} payload={p['payload'].hex()}")
            except ValueError:
                debug.append(f"CRC-fail {fr.hex()[:24]}")
        assert False, f"OTA 后 5s 未收到 bit7 心跳，共 {len(seen)} 帧: {debug}"

    # Agree → case proceeds to ota_prepare.
    serial_port.write(sgc_at.pack_heartbeat_response(
        glass_soc=0x20, glass_sta=0x00, case_version=0x01, ota_agree=True
    ))

    end = time.time() + 3.0
    prepare = None
    seen = []
    while time.time() < end:
        f = sgc_at.recv_request(serial_port, timeout=1.0)
        if f is None:
            continue
        seen.append(f)
        got = struct.unpack_from(">H", f, 7)[0]
        if got == sgc_at.OPCODE_CASE_PACKET_PREPARE:
            prepare = f
            break

    if prepare is None:
        debug = []
        for fr in seen[-8:]:
            try:
                p = sgc_at.parse_frame(fr)
                debug.append(f"op={p['opcode']:#06x} payload={p['payload'].hex()}")
            except ValueError:
                debug.append(f"CRC-fail {fr.hex()[:24]}")
        assert False, f"同意 OTA 后 3s 未收到 PREPARE，共 {len(seen)} 帧: {debug}"

    # Reply size=0 so the case exits ota_run cleanly (OTA_ERR_PREPARE) instead
    # of staging a real image and resetting.
    serial_port.write(sgc_at.pack_prepare_response(0))


def test_i02_full_ota_with_app_bin(serial_port, tmp_path):
    """I02: push App.bin → case stages + resets → BL copies → new App runs.

    Skipped unless SGC_APP_BIN env var points to a built App.bin. The new image
    must have CASE_FW_VERSION differing from the running one — verification
    probes the post-reset firmware with a heartbeat carrying the new version
    and checks the case no longer auto-triggers OTA (mismatch detection).
    """
    import os
    import pytest
    app_bin = os.environ.get("SGC_APP_BIN")
    if not app_bin or not os.path.exists(app_bin):
        pytest.skip("set SGC_APP_BIN=<path> to a built App.bin to run this test")

    new_version = int(os.environ.get("SGC_NEW_FW_VERSION", "0x02"), 0)

    with open(app_bin, "rb") as f:
        fw = f.read()

    _enter_charging(serial_port)

    # OTA command is a HIL protocol frame; send_command waits for the ACK.
    ack = sgc_at.send_command(serial_port, "OTA")
    assert ack is not None, "OTA 命令未收到 HIL ACK"
    serial_port.timeout = 0.1

    # Wait for bit7 heartbeat, then agree.
    end = time.time() + 5.0
    ota_req = None
    while time.time() < end:
        f = sgc_at.recv_request(serial_port, timeout=1.0)
        if f is None:
            continue
        try:
            p = sgc_at.parse_frame(f)
        except ValueError:
            continue
        if p["opcode"] == sgc_at.OPCODE_CASE_HEART and (p["payload"][3] & 0x80):
            ota_req = f
            break
    assert ota_req is not None, "OTA 后 5s 未收到 bit7 心跳"

    serial_port.write(sgc_at.pack_heartbeat_response(
        glass_soc=0x20, glass_sta=0x00, case_version=new_version, ota_agree=True
    ))

    # PREPARE → reply with image size.
    end = time.time() + 3.0
    prepare = None
    while time.time() < end:
        f = sgc_at.recv_request(serial_port, timeout=1.0)
        if f is None:
            continue
        if struct.unpack_from(">H", f, 7)[0] == sgc_at.OPCODE_CASE_PACKET_PREPARE:
            prepare = f
            break
    assert prepare is not None, "同意 OTA 后 3s 未收到 PREPARE"
    serial_port.write(sgc_at.pack_prepare_response(len(fw)))

    # Stream firmware in 240-byte blocks; last block carries type=END.
    BLOCK = 240
    index = 0
    import sys as _sys
    t_stream_start = time.time()
    t_last_write_done = t_stream_start
    while True:
        chunk = fw[index * BLOCK:(index + 1) * BLOCK]
        is_end = len(chunk) < BLOCK
        end = time.time() + 3.0
        read_req = None
        diag_seen = []
        t_recv_start = time.time()
        while time.time() < end:
            f = sgc_at.recv_request(serial_port, timeout=1.0)
            if f is None:
                continue
            try:
                p = sgc_at.parse_frame(f)
                diag_seen.append(f"op={p['opcode']:#06x} idx={p['payload'][2]:3d}")
            except ValueError:
                diag_seen.append(f"CRC-fail {f.hex()[:24]}")
            if struct.unpack_from(">H", f, 7)[0] == sgc_at.OPCODE_CASE_PACKET_READ:
                read_req = f
                t_recv_done = time.time()
                break
        assert read_req is not None, (
            f"未收到 READ index={index}（T+{(time.time()-t_stream_start)*1000:.0f}ms），"
            f"等待期间看到: {diag_seen[-5:]}"
        )
        # Use the index from the REQ, not our counter — firmware may have retried
        # with a different index than we expect.
        p = sgc_at.parse_frame(read_req)
        fw_index = p["payload"][2]
        chunk = fw[fw_index * BLOCK:(fw_index + 1) * BLOCK]
        is_end = len(chunk) < BLOCK
        rsp = sgc_at.pack_read_response(fw_index, chunk, packet_type=1 if is_end else 0)
        t_write_start = time.time()
        written = serial_port.write(rsp)
        serial_port.flush()
        t_write_done = time.time()
        gap_ms = (t_recv_start - t_last_write_done) * 1000
        _sys.stderr.write(
            f"[ota] fw_idx={fw_index:3d} pc_idx={index:3d} "
            f"gap={gap_ms:5.0f}ms recv={(t_recv_done-t_recv_start)*1000:5.0f}ms "
            f"write={(t_write_done-t_write_start)*1000:4.0f}ms "
            f"@ T+{(t_write_done-t_stream_start)*1000:6.0f}ms\n"
        )
        t_last_write_done = t_write_done
        if is_end:
            break
        index += 1

    # Case sets staged + resets. BL copies Staging → App on next boot.
    serial_port.close()
    time.sleep(2.5)  # BL copy + App boot
    serial_port.open()
    serial_port.timeout = 0.1

    # Verification: RESET+OPEN the new App, send heartbeats carrying the new
    # case_version. If running firmware matches (upgrade succeeded), no version
    # mismatch → no auto-OTA → no bit7 heartbeat. If still old version,
    # mismatch → ST_OTA entry → bit7 heartbeat appears.
    _flush_rx(serial_port)
    sgc_at.send_command(serial_port, "RESET")
    time.sleep(0.5)
    _flush_rx(serial_port)
    sgc_at.send_command(serial_port, "OPEN")
    frame = sgc_at.recv_request(serial_port, timeout=5.0)
    assert frame is not None, "新固件启动后 OPEN 未收到心跳"

    response = sgc_at.pack_heartbeat_response(
        glass_soc=0x20, glass_sta=0x00, case_version=new_version
    )
    end = time.time() + 8.0
    saw_bit7 = False
    last_hb = None
    while time.time() < end:
        serial_port.write(response)
        f = sgc_at.recv_request(serial_port, timeout=1.0)
        if f is None:
            continue
        try:
            p = sgc_at.parse_frame(f)
        except ValueError:
            continue
        if p["opcode"] == sgc_at.OPCODE_CASE_HEART:
            last_hb = p["payload"]
            if p["payload"][3] & 0x80:
                saw_bit7 = True
                break
    assert last_hb is not None, "8s 内未收到任何心跳响应"
    assert not saw_bit7, (
        f"升级失败：新固件仍报告旧版本（PC 发 case_version={new_version:#x} "
        f"触发自动 OTA）。最后一帧 payload={last_hb.hex()}"
    )
