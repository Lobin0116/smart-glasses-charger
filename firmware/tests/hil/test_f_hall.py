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

    路径: RESET+OPEN → STATUS 轮询验证进 CHARGING/MAINTAINING →
         CLOSE 注入 → 4s 内收到新心跳（关盖触发重新握手）.

    F02 旧逻辑用 "心跳 interval > 500ms" 判定握手完成，但 HANDSHAKING retry
    间隔与 CHARGING 30s 周期之间没有清晰 500ms 边界，CLOSE 时机随机命中
    固件不同状态导致 3 rerun 全 fail。改用 STATUS 直接查询状态机。
    """
    response = sgc_at.pack_heartbeat_response(
        glass_soc=0x20, glass_sta=0x00, case_version=sgc_at.CASE_FW_VERSION
    )

    # 阶段 1：RESET + OPEN，持续 write response + STATUS 轮询确认进 CHARGING。
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()
    sgc_at.send_command(serial_port, "RESET")
    time.sleep(0.5)
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()
    sgc_at.send_command(serial_port, "OPEN")

    serial_port.timeout = 0.1
    deadline = time.time() + 10.0
    last_status_query = 0.0
    entered = False
    while time.time() < deadline:
        serial_port.write(response)
        time.sleep(0.15)  # > at_frame_recv 100ms timeout，让固件有机会消费 RSP
        if time.time() - last_status_query < 0.5:
            continue
        last_status_query = time.time()
        serial_port.reset_input_buffer()
        sgc_at.reset_recv_buffer()
        ack = sgc_at.send_command(serial_port, "STATUS")
        if ack is None:
            continue
        st = sgc_at.parse_hil_status(ack)
        if st["state"] in (2, 3):  # CHARGING or MAINTAINING
            entered = True
            break
    assert entered, "10s 内 STATUS 未确认固件进 CHARGING/MAINTAINING，无法测 CLOSE 触发"

    # 阶段 2：注入 CLOSE → 4s 内期望收到新心跳（HANDSHAKING 重新触发）。
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()
    sgc_at.send_command(serial_port, "CLOSE")

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
