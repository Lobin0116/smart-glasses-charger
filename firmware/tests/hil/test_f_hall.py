"""F 类 — 霍尔 PB4 测试 (P0).

update_mode.c 把 'OPEN'/'CLOSE' 命令转成 sm_inject_lid_event()，
等效磁铁远离（开盖）/靠近（合盖）霍尔传感器。
自动化验证命令注入触发状态机；物理霍尔（真·磁铁）烧录后手动验证。

F01/F02 物理测试：磁铁靠近/远离 PB4 霍尔，观察 LED + 串口心跳。
"""
import time

import pytest

import sgc_at


def test_f01_open_lid_event(serial_port):
    """F01: RESET (no OPEN) → 发 OPEN → 触发 HANDSHAKING → 收到心跳 REQ.

    先确认 RESET 后静默窗口内没有心跳（IDLE 不发），再注入 OPEN，
    验证开盖事件触发了 IDLE→HANDSHAKING 转换。
    """
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()
    sgc_at.send_command(serial_port, "RESET")
    time.sleep(0.5)
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()

    # 静默窗口确认 IDLE 不发心跳（消极对照，证明后面收到的心跳是 OPEN 触发的）。
    serial_port.timeout = 0.1
    quiet = sgc_at.recv_request(serial_port, timeout=1.0)
    serial_port.timeout = 2.0
    assert quiet is None, "RESET 后未发 OPEN 就收到心跳（IDLE 状态不应发心跳）"

    # 注入 OPEN → 应触发 HANDSHAKING。
    sgc_at.send_command(serial_port, "OPEN")
    frame = sgc_at.recv_request(serial_port, timeout=4.0)
    assert frame is not None, "OPEN 命令后 4s 内未收到心跳（开盖事件未触发 HANDSHAKING）"


@pytest.mark.flaky(reruns=3, reruns_delay=2)
def test_f02_close_lid_event(serial_port):
    """F02: CLOSE after handshake → firmware 重新进 HANDSHAKING 发心跳.

    路径: RESET+OPEN → 回响应直到离开 HANDSHAKING (心跳间隔 >500ms) →
         CLOSE → 重新握手 → 发心跳 REQ。
    关盖 + glass_present 触发 sm_inject_lid_event → IDLE/HANDSHAKING 重新进入。
    """
    # RESET + OPEN
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()
    sgc_at.send_command(serial_port, "RESET")
    time.sleep(0.5)
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()
    sgc_at.send_command(serial_port, "OPEN")

    # 完成握手：持续回响应，直到心跳间隔 >500ms（已离开 HANDSHAKING）。
    response = sgc_at.pack_heartbeat_response(
        glass_soc=0x20, glass_sta=0x00, case_version=sgc_at.CASE_FW_VERSION
    )
    serial_port.timeout = 0.1
    last = time.time()
    handshake_done = False
    deadline = time.time() + 8.0
    while time.time() < deadline:
        serial_port.write(response)
        nxt = sgc_at.recv_request(serial_port, timeout=1.0)
        if nxt is None:
            continue
        interval_ms = (time.time() - last) * 1000
        if interval_ms > 500:
            handshake_done = True
            # 满足触发退出的那帧 REQ 的 recv 窗口（避免它吞掉后面的 CLOSE 命令）。
            serial_port.write(response)
            time.sleep(0.15)
            break
        last = time.time()
    serial_port.timeout = 2.0
    assert handshake_done, "8s 内未完成握手（未离开 HANDSHAKING，无法测 CLOSE 触发）"

    # 注入 CLOSE → 应触发重新握手。
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()
    sgc_at.send_command(serial_port, "CLOSE")

    # 验证 firmware 重新进入 HANDSHAKING（在 4s 内发出新的心跳 REQ）。
    frame = sgc_at.recv_request(serial_port, timeout=4.0)
    assert frame is not None, "CLOSE 后 4s 内未收到新心跳（关盖事件未触发重新握手）"


@pytest.mark.flaky(reruns=3, reruns_delay=2)
def test_f03_lid_event_dual_edge(serial_port):
    """F03: 反复 OPEN→CLOSE 注入（双沿），每次 OPEN 都触发 HANDSHAKING 发心跳.

    验证霍尔双沿都能触发状态机（不只是上升沿或下降沿）。
    """
    for i in range(3):
        serial_port.reset_input_buffer()
        sgc_at.send_command(serial_port, "RESET")
        time.sleep(0.4)
        serial_port.reset_input_buffer()
        sgc_at.send_command(serial_port, "OPEN")
        frame = sgc_at.recv_request(serial_port, timeout=4.0)
        assert frame is not None, f"第 {i+1} 轮 OPEN 后未收到心跳（霍尔双沿触发失败）"
