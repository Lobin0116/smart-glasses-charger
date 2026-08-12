"""C 类 — 状态机转换测试 (P0).

通过 OPEN/CLOSE 命令注入盖子事件，观察心跳帧 opcode 和到达间隔推断状态。
HIL_TEST 固件 ST_IDLE 不进 Deep-Sleep（state_machine.c），保证 UART 持续响应。

硬件限制：case_soc 来自 CW2017，当前报 0xFE=254%（见 cw2017-version-soc-anomaly
memory），恒高于低电阈值 15%，所以低电分支（C03/C05/C12）无法触发，标 skip。
"""
import struct
import time

import pytest

import sgc_at


OPCODE_SHUTDOWN = 0x3002


def _reset_open(serial_port):
    """RESET 清状态 → OPEN 注入开盖 → 触发 IDLE→HANDSHAKING."""
    serial_port.reset_input_buffer()
    sgc_at.send_command(serial_port, "RESET")
    time.sleep(0.5)
    serial_port.reset_input_buffer()
    sgc_at.send_command(serial_port, "OPEN")


def _send_full_response(serial_port):
    """回一个"眼镜充满"心跳响应 (glass_soc=0xE4: bit7=满, low7=100%)."""
    response = sgc_at.pack_heartbeat_response(
        glass_soc=0xE4, glass_sta=0x00, case_version=sgc_at.CASE_FW_VERSION
    )
    serial_port.write(response)


def _send_normal_response(serial_port, glass_soc=0x20):
    """回一个"眼镜未充满"心跳响应."""
    response = sgc_at.pack_heartbeat_response(
        glass_soc=glass_soc, glass_sta=0x00, case_version=sgc_at.CASE_FW_VERSION
    )
    serial_port.write(response)


def test_c01_idle_open_enters_handshaking(serial_port):
    """C01: IDLE + 开盖 → HANDSHAKING，5s 内发出心跳请求帧."""
    _reset_open(serial_port)
    frame = sgc_at.recv_request(serial_port, timeout=5.0)
    assert frame is not None, "OPEN 后 5s 内未收到心跳帧（IDLE→HANDSHAKING 未触发）"
    opcode = struct.unpack_from(">H", frame, 7)[0]
    assert opcode == sgc_at.OPCODE_CASE_HEART, f"opcode={opcode:#06x}, 应为心跳 0x3001"


def test_c02_handshake_success_stops_retry(serial_port):
    """C02: 握手成功 + 高电 → CHARGING，心跳从 ~200ms retry 变 30s 周期.

    合法响应后 2s 内不应再收到 retry 心跳（HANDSHAKING retry 间隔 ~200ms）。
    case_soc=254%>15% 走 CHARGING（30s 心跳），2s 内收到帧 = 握手失败还在 retry。
    """
    _reset_open(serial_port)
    serial_port.timeout = 0.02
    frame = sgc_at.recv_request(serial_port, timeout=5.0)
    assert frame is not None, "未收到初始心跳"

    _send_normal_response(serial_port, glass_soc=0x20)

    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()
    start = time.time()
    next_frame = sgc_at.recv_request(serial_port, timeout=2.0)
    elapsed = time.time() - start
    serial_port.timeout = 2.0

    assert next_frame is None or elapsed > 0.8, (
        f"响应后 {elapsed:.2f}s 就收到新帧，说明握手未成功（还在 200ms retry）。"
        f"frame={next_frame!r}"
    )


@pytest.mark.flaky(reruns=3, reruns_delay=2)
def test_c07_close_full_triggers_shutdown(serial_port):
    """C07: CHARGING + 充满 + 关盖 → SHUTTING_DOWN，收到 0x3002 关机帧.

    路径：OPEN → 连续回充满响应 → CHARGING→MAINTAINING（开盖+满）→
         CLOSE → 重新握手 → 回充满响应 → CHARGING + 关盖 + 满 →
         SHUTTING_DOWN → sm_do_shutdown 发 0x3002。
    """
    _reset_open(serial_port)

    # 阶段 1：握手 + 稳定到 MAINTAINING（开盖+满），持续回响应吃掉所有心跳
    serial_port.timeout = 0.5
    stabilize_end = time.time() + 6.0
    while time.time() < stabilize_end:
        frame = sgc_at.recv_request(serial_port, timeout=1.5)
        if frame is None:
            break
        _send_full_response(serial_port)

    # 阶段 2：CLOSE 触发关盖 → 重新握手 → 关盖+满 → SHUTTING_DOWN
    # Use F02's pattern: send RSP to close the recv window, wait 150ms past
    # the 100ms timeout, flush, then CLOSE. In MAINTAINING (1s heartbeat),
    # the main-loop window after recv returns is ~900ms — enough for CLOSE.
    serial_port.write(sgc_at.pack_heartbeat_response(
        glass_soc=0xE4, glass_sta=0x00, case_version=sgc_at.CASE_FW_VERSION))
    serial_port.flush()
    time.sleep(0.15)
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()
    sgc_at.send_command(serial_port, "CLOSE")

    # 持续回响应（重新握手需要），同时等 0x3002
    serial_port.timeout = 0.05
    shutdown_frame = None
    deadline = time.time() + 15.0
    while time.time() < deadline:
        frame = sgc_at.recv_request(serial_port, timeout=2.0)
        if frame is None:
            continue
        opcode = struct.unpack_from(">H", frame, 7)[0]
        if opcode == OPCODE_SHUTDOWN:
            shutdown_frame = frame
            break
        if opcode == sgc_at.OPCODE_CASE_HEART:
            _send_full_response(serial_port)

    assert shutdown_frame is not None, "CLOSE+充满 后 15s 内未收到关机帧 0x3002"


@pytest.mark.skip(reason="需 case_soc<=15 触发低电分支，当前 CW2017 报 254% — 见 memory cw2017-version-soc-anomaly")
def test_c03_low_soc_enters_maintaining(serial_port):
    """C03: 握手成功 + 低电 → MAINTAINING（心跳 <1.2s）."""
    _reset_open(serial_port)
    frame = sgc_at.recv_request(serial_port, timeout=5.0)
    assert frame is not None
    _send_normal_response(serial_port, glass_soc=0x20)

    # MAINTAINING 心跳 1s，收两个验证间隔
    ts = []
    end = time.time() + 5.0
    while len(ts) < 2 and time.time() < end:
        f = sgc_at.recv_request(serial_port, timeout=2.0)
        if f:
            ts.append(time.time())
            _send_normal_response(serial_port, glass_soc=0x20)
    assert len(ts) == 2, f"低电维持心跳不足 2 个（{len(ts)}），无法验证 MAINTAINING 间隔"
    interval_ms = (ts[1] - ts[0]) * 1000
    assert interval_ms < 1200, f"心跳间隔 {interval_ms:.0f}ms，应 <1200ms（MAINTAINING）"


@pytest.mark.skip(reason="需 case_soc<=15 触发低电关机，当前 CW2017 报 254%")
def test_c12_low_soc_shutdown_before_sleep(serial_port):
    """C12: 低电关机前必发 AT_SHUTDOWN (0x3002)."""
    _reset_open(serial_port)
    # 触发低电场景后观察 shutdown 帧
    pass
