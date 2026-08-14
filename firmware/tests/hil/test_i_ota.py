"""I 类 — OTA 升级测试.

PC 模拟眼镜端，触发盒子 OTA 流程。盒子握手 → CHARGING 后发 OTA 命令强制
ota_requested=true，状态机进 ST_OTA，ota_run 同步跑 prepare/read 协议。

I01 验证 OTA 触发链路（case_sta bit7 申请 → 同意 → 收到 PREPARE 请求）。
I02 完整烧录需要 App.bin 文件 + 板子重启重连，留作框架。
"""
import struct
import time

import pytest
import sgc_at


def _flush_rx(serial_port):
    """Clear both the OS-level serial RX buffer and sgc_at's internal buffer
    (which holds bytes already pulled out of the OS buffer but not yet parsed
    into a frame)."""
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()


def _enter_charging(serial_port):
    """RESET+OPEN → heartbeat → reply for ~3s → query STATUS to verify firmware
    entered CHARGING (or MAINTAINING).

    State detection uses HIL STATUS query, not heartbeat interval. Reason:
    CHARGING has a 30s heartbeat period (TIM spec for open-lid), so any
    interval-based "left HANDSHAKING" check needs a 30s+ deadline. STATUS
    queries the state machine directly and returns within ~100ms.
    """
    import sys as _sys
    import time as _t
    _t0 = _t.time()

    def _dbg(msg):
        print(f"[echrg T+{(_t.time()-_t0)*1000:6.0f}ms] {msg}", flush=True)

    _STATE_NAMES = {0: "IDLE", 1: "HANDSHAKING", 2: "CHARGING",
                    3: "MAINTAINING", 4: "FORCE_CHARGING",
                    5: "SHUTTING_DOWN", 6: "OTA", 7: "SHIP_MODE"}

    _flush_rx(serial_port)
    _dbg("send RESET")
    sgc_at.send_command(serial_port, "RESET")
    _t.sleep(2.0)  # fixture open resets firmware via DTR edge; wait for v10 boot
    _flush_rx(serial_port)
    _dbg("send OPEN")
    sgc_at.send_command(serial_port, "OPEN")
    frame = sgc_at.recv_request(serial_port, timeout=5.0)
    assert frame is not None, "OPEN 后 5s 未收到心跳"
    _dbg(f"got initial frame opcode={struct.unpack_from('>H', frame, 7)[0]:#06x}")

    response = sgc_at.pack_heartbeat_response(
        glass_soc=0x20, glass_sta=0x00, case_version=0
    )
    # Single response per attempt + STATUS poll. Don't flood: 3s of flood
    # leaves ~60 response frames queued in firmware RX buffer; update_mode_poll
    # then breaks on the leading RSP frame (opcode HEART) and STATUS HIL
    # commands can never reach dispatch. Sending one response per loop and
    # polling STATUS in between keeps the buffer shallow.
    deadline = _t.time() + 10.0
    last_status_query = 0.0
    while _t.time() < deadline:
        serial_port.write(response)
        _t.sleep(0.15)  # > hal_usart_recv's 100ms byte-gap timeout so firmware
                         # has a chance to consume this response in a retry window
        # Query STATUS every ~0.5s to check if firmware entered CHARGING.
        if _t.time() - last_status_query < 0.5:
            continue
        last_status_query = _t.time()
        _flush_rx(serial_port)
        ack = sgc_at.send_command(serial_port, "STATUS")
        if ack is None:
            _dbg("STATUS: no ACK, retry")
            continue
        st = sgc_at.parse_hil_status(ack)
        s = st["state"]
        _dbg(f"STATUS: state={s} ({_STATE_NAMES.get(s, '?')}) case_soc={st['case_soc']}")
        if s in (2, 3):  # CHARGING or MAINTAINING
            _dbg(f"handshake confirmed, firmware in {_STATE_NAMES[s]}")
            _flush_rx(serial_port)
            return frame

    assert False, "10s STATUS 轮询未确认固件进 CHARGING/MAINTAINING"


def _await_opcode(serial_port, opcode, timeout, body_check=None):
    """Wait for a request frame with the given opcode. Returns the frame or None."""
    end = time.time() + timeout
    while time.time() < end:
        f = sgc_at.recv_request(serial_port, timeout=1.0)
        if f is None:
            continue
        got = struct.unpack_from("<H", f, 7)[0]
        if got == opcode and (body_check is None or body_check(f)):
            return f
    return None


@pytest.mark.flaky(reruns=3, reruns_delay=2)
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
        opcode = struct.unpack_from("<H", f, 7)[0]
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
        glass_soc=0x20, glass_sta=0x00, case_version=sgc_at.CASE_FW_VERSION, ota_agree=True
    ))

    end = time.time() + 3.0
    prepare = None
    seen = []
    while time.time() < end:
        f = sgc_at.recv_request(serial_port, timeout=1.0)
        if f is None:
            continue
        seen.append(f)
        got = struct.unpack_from("<H", f, 7)[0]
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


def _query_status_for_diag(serial_port):
    """Query STATUS once and format the OTA-related fields for assertion
    messages. Best-effort: returns a placeholder string if the query fails
    so the assertion still fires with whatever info we have."""
    try:
        serial_port.reset_input_buffer()
        sgc_at.reset_recv_buffer()
        ack = sgc_at.send_command(serial_port, "STATUS")
        if ack is None:
            return "<no ACK>"
        st = sgc_at.parse_hil_status(ack)
        return (
            f"state={st['state']} ota_idx={st['ota_index']} "
            f"fails={st['ota_fails']} fail_reason={st['ota_fail_reason']} "
            f"rxlen={st['ota_rxlen']} succ_len={st['ota_succ_len']}"
        )
    except Exception as exc:
        return f"<status query failed: {exc}>"


def test_i02_full_ota_with_app_bin(serial_port, tmp_path):
    """I02: push App.bin → case stages + resets → BL copies → new App runs.

    Skipped unless SGC_APP_BIN env var points to a built App.bin. The new image
    must have CASE_FW_VERSION differing from the running one — verification
    probes the post-reset firmware with a heartbeat carrying the new version
    and checks the case no longer auto-triggers OTA (mismatch detection).
    """
    import gc
    import os
    import pytest
    gc.disable()  # avoid GC pauses causing firmware retry (200ms timeout)
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
    print(f"[ota-trigger T+0ms] sent agree RSP, waiting for PREPARE", flush=True)

    # PREPARE → reply with image size.
    end = time.time() + 3.0
    prepare = None
    seen_after_agree = []
    t_prepare_start = time.time()
    while time.time() < end:
        f = sgc_at.recv_request(serial_port, timeout=1.0)
        if f is None:
            continue
        try:
            p = sgc_at.parse_frame(f)
            seen_after_agree.append(f"op=0x{p['opcode']:04X} payload={p['payload'].hex()[:20]}")
        except ValueError:
            seen_after_agree.append(f"CRC-fail {f.hex()[:20]}")
        if struct.unpack_from("<H", f, 7)[0] == sgc_at.OPCODE_CASE_PACKET_PREPARE:
            prepare = f
            break
    assert prepare is not None, (
        f"同意 OTA 后 3s 未收到 PREPARE，"
        f"收到 {len(seen_after_agree)} 帧: {seen_after_agree[-5:]}, "
        f"STATUS: {_query_status_for_diag(serial_port)}"
    )
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
            if struct.unpack_from("<H", f, 7)[0] == sgc_at.OPCODE_CASE_PACKET_READ:
                read_req = f
                t_recv_done = time.time()
                break
        assert read_req is not None, (
            f"未收到 READ index={index}（T+{(time.time()-t_stream_start)*1000:.0f}ms），"
            f"等待期间看到: {diag_seen[-5:]}, "
            f"STATUS: {_query_status_for_diag(serial_port)}"
        )
        # Use the index from the REQ, not our counter — firmware may have retried
        # with a different index than we expect.
        p = sgc_at.parse_frame(read_req)
        fw_index = p["payload"][2]
        chunk = fw[fw_index * BLOCK:(fw_index + 1) * BLOCK]
        is_end = len(chunk) < BLOCK
        rsp = sgc_at.pack_read_response(fw_index, chunk, packet_type=1 if is_end else 0)
        t_write_call = time.time()
        written = serial_port.write(rsp)
        t_write_ret = time.time()
        serial_port.flush()
        t_flush_done = time.time()
        gap_ms = (t_recv_start - t_last_write_done) * 1000
        print(
            f"[ota] fw_idx={fw_index:3d} pc_idx={index:3d} "
            f"recv={(t_recv_done-t_recv_start)*1000:5.0f}ms "
            f"w_call={(t_write_ret-t_write_call)*1000:3.0f}ms "
            f"flush={(t_flush_done-t_write_ret)*1000:4.0f}ms "
            f"@ T+{(t_flush_done-t_stream_start)*1000:6.0f}ms",
            flush=True,
        )
        t_last_write_done = t_flush_done
        if is_end:
            break
        index += 1

    # Case sets staged + resets. BL copies Staging → App on next boot.
    # BL copies ~27 KB at 8 MHz IRC: page erase + word program ≈ 1.5 s typical,
    # 3 s worst case. 2 s sleep covers typical + App boot + CW2017 auto-burn
    # self-check. If this turns out flaky we can push back to 2.5 s.
    t_verify_start = time.time()

    def _vlog(msg):
        print(f"[verify T+{(time.time()-t_verify_start)*1000:6.0f}ms] {msg}", flush=True)

    _vlog("close serial, wait for BL copy + App boot")
    serial_port.close()
    time.sleep(2.5)
    _vlog("reopen serial")
    serial_port.open()
    serial_port.timeout = 0.1

    # Verification: RESET+OPEN the new App, send heartbeats carrying the new
    # case_version, then query STATUS to confirm the state machine did NOT enter
    # ST_OTA. If running firmware matches (upgrade succeeded), no version
    # mismatch → no auto-OTA → state ends up CHARGING/MAINTAINING. If still old
    # version, mismatch → ST_OTA entry → state=6.
    _flush_rx(serial_port)
    _vlog("send RESET")
    ack = sgc_at.send_command(serial_port, "RESET")
    _vlog(f"RESET ack={ack!r}")
    time.sleep(0.5)
    _flush_rx(serial_port)
    _vlog("send OPEN")
    ack = sgc_at.send_command(serial_port, "OPEN")
    _vlog(f"OPEN ack={ack!r}")
    frame = sgc_at.recv_request(serial_port, timeout=5.0)
    _vlog(f"first frame after OPEN: {frame!r}")
    assert frame is not None, "新固件启动后 OPEN 未收到心跳"

    response = sgc_at.pack_heartbeat_response(
        glass_soc=0x20, glass_sta=0x00, case_version=new_version
    )
    # Send exactly ONE heartbeat response to the OPEN-triggered REQ. This lets
    # the firmware complete the handshake and enter CHARGING (state=2), which
    # is what we need to detect version mismatch (CHARGING → if mismatch, sm_tick
    # sets ota_requested and transitions to ST_OTA on the next tick).
    #
    # DO NOT loop-send responses: firmware is in CHARGING with 30s heartbeat
    # period, so it consumes only ONE response and the rest pile up in its RX
    # buffer. Subsequent HIL commands (STATUS) sit behind the stale RSPs and
    # update_mode_poll never reaches them (it peeks opcode, sees HEARTBEAT =
    # production frame, breaks to leave it for charge_poll).
    _vlog("send 1 heartbeat response to complete handshake")
    serial_port.write(response)
    serial_port.flush()
    time.sleep(0.5)  # give firmware time to consume + sm_tick to detect version

    # Final state check via STATUS. state=6 (OTA) means version mismatch →
    # upgrade failed. state=2/3 (CHARGING/MAINTAINING) means version match →
    # upgrade succeeded.
    _flush_rx(serial_port)
    _vlog("send STATUS")
    ack = sgc_at.send_command(serial_port, "STATUS")
    if ack is None:
        _vlog("STATUS: no ACK!")
    else:
        st = sgc_at.parse_hil_status(ack)
        _vlog(f"STATUS: state={st['state']} case_soc={st['case_soc']} "
              f"ota_fail_reason={st['ota_fail_reason']} "
              f"at_frame_stage={st['at_frame_fail_stage']} "
              f"at_frame_buf={st['at_frame_buf_bytes']}")
    assert ack is not None, "STATUS 命令无 ACK"
    st = sgc_at.parse_hil_status(ack)
    _STATE_OTA = 6
    assert st["state"] != _STATE_OTA, (
        f"升级失败：新固件仍报告旧版本（PC 发 case_version={new_version:#x} "
        f"触发自动 OTA，state=OTA）。STATUS={st}"
    )
    _vlog("verification passed")
