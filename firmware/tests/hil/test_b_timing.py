"""B 类 — 时序测试.

验证固件握手/心跳的关键时序参数。所有时序通过 PC 测量帧到达时间戳推断，
不依赖示波器。HANDSHAKE 5V pulse / 泄放等硬件时序需待完整 PCB（B01/B02）。
"""
import time

import pytest

import sgc_at


def _reset_open(serial_port):
    """RESET → 清状态 → OPEN → 触发 HANDSHAKING."""
    serial_port.reset_input_buffer()
    sgc_at.send_command(serial_port, "RESET")
    time.sleep(0.5)
    serial_port.reset_input_buffer()
    sgc_at.send_command(serial_port, "OPEN")


def _collect_heartbeat_timestamps(serial_port, count, timeout=10.0):
    """收集 count 个心跳帧的到达时间戳。"""
    timestamps = []
    end = time.time() + timeout
    while len(timestamps) < count and time.time() < end:
        frame = sgc_at.recv_request(serial_port, timeout=2.0)
        if frame:
            timestamps.append(time.time())
    return timestamps


def test_b03_handshake_retry_interval(serial_port):
    """B03 + B04: 握手期间不回响应，心跳 retry 间隔 ~200ms.

    固件 sm_do_handshake 3 次 retry:
    - 每次 sm_send_heartbeat 发心跳 + hal_usart_recv 等 100ms (COMM_TIMEOUT_MS)
    - 失败后 hal_timer_delay_ms(100) (HANDSHAKE_RETRY_INTERVAL_MS)
    - 间隔 = 100 + 100 = 200ms

    B04 (recv timeout 100ms) 通过此间隔的 recv 分量间接验证。
    """
    _reset_open(serial_port)
    # Wait for HANDSHAKING to start: 5V pulse (300ms) + discharge (100ms) = 400ms.
    time.sleep(0.5)
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()
    serial_port.timeout = 0.02
    # Collect 5 timestamps to increase chance of catching within-attempt retries
    # even if the first heartbeat pair has a stale-RSP-induced long gap.
    timestamps = _collect_heartbeat_timestamps(serial_port, count=5, timeout=5.0)
    serial_port.timeout = 2.0

    assert len(timestamps) >= 3, f"5s 内只收到 {len(timestamps)} 个心跳，期望 ≥3"

    intervals_ms = [(timestamps[i + 1] - timestamps[i]) * 1000 for i in range(len(timestamps) - 1)]

    # At least one pair should be within-attempt retry interval (~200ms).
    # Other pairs may be between-attempt gaps (~500ms) if stale RSPs caused
    # the first handshake attempt to succeed.
    retry_intervals = [iv for iv in intervals_ms if 150 < iv < 300]
    assert retry_intervals, (
        f"没有心跳间隔在 150-300ms 范围内（期望至少一对 ~200ms 的 retry 间隔）。"
        f"all intervals = {intervals_ms}"
    )


def test_b04_recv_timeout_100ms(serial_port):
    """B04: 100ms recv timeout — 不回响应，firmware 应在 ~200ms 后发 retry.

    recv timeout = hal_usart_recv 等 100ms (COMM_TIMEOUT_MS) 后放弃；加 retry delay
    100ms (HANDSHAKE_RETRY_INTERVAL_MS)，retry 周期 ~200ms。收到 retry 即证明
    firmware 在 100ms recv 窗口超时后放弃了上一次请求。
    """
    _reset_open(serial_port)
    serial_port.timeout = 0.02

    frame = sgc_at.recv_request(serial_port, timeout=3.0)
    assert frame is not None, "RESET+OPEN 后未收到初始心跳"

    # 等 150ms (past 100ms recv timeout)；retry 周期 ~200ms，可能恰好在窗口边界。
    time.sleep(0.150)

    # 收集任何 retry（最多再等 300ms 让 ~200ms 周期的 retry 到达）。
    retries = []
    end = time.time() + 0.300
    while time.time() < end:
        f = sgc_at.recv_request(serial_port, timeout=0.05)
        if f:
            retries.append(f)
            break

    serial_port.timeout = 2.0

    # 发响应清理 firmware 状态（响应给 retry 即可，避免下个测试受影响）。
    response = sgc_at.pack_heartbeat_response(
        glass_soc=0x20, glass_sta=0x00, case_version=sgc_at.CASE_FW_VERSION
    )
    serial_port.write(response)

    assert len(retries) >= 1, (
        "150ms 等待 + 300ms 收集窗口内未收到 retry — "
        "firmware 未在 100ms recv timeout 后重试"
    )


def _enter_charging(serial_port):
    """RESET+OPEN → 持续 write response + STATUS 轮询确认进 CHARGING.

    Returns the first heartbeat frame received (caller usually ignores it).
    Reuses the pattern from test_i_ota._enter_charging: STATUS queries the
    state machine directly so we don't have to wait a full 30s CHARGING
    heartbeat period to confirm we left HANDSHAKING.
    """
    response = sgc_at.pack_heartbeat_response(
        glass_soc=0x20, glass_sta=0x00, case_version=sgc_at.CASE_FW_VERSION
    )
    _reset_open(serial_port)
    serial_port.timeout = 0.1
    deadline = time.time() + 10.0
    last_status_query = 0.0
    while time.time() < deadline:
        serial_port.write(response)
        time.sleep(0.15)
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
            serial_port.reset_input_buffer()
            sgc_at.reset_recv_buffer()
            return
    pytest.fail("10s 内 STATUS 未确认进 CHARGING/MAINTAINING")


def test_b07_open_lid_heartbeat_30s(serial_port):
    """B07: 开盖充电心跳周期 30s (SM_CHARGE_POLL_OPEN_MS).

    进入 CHARGING 后清空 buffer，记录两次心跳间隔。预期 30s ±2s
    (允许调度抖动)。SOC>15 走 CHARGING（30s），不是 MAINTAINING（<1.2s）。
    """
    _enter_charging(serial_port)

    # 收两个心跳，测间隔。第一个心跳可能在 STATUS 之后立刻来（剩余 charge_poll
    # 周期），所以从第一个心跳开始计时。
    serial_port.timeout = 35.0
    t1 = None
    t2 = None
    end = time.time() + 70.0
    while time.time() < end:
        f = sgc_at.recv_request(serial_port, timeout=35.0)
        if f is None:
            continue
        if t1 is None:
            t1 = time.time()
        else:
            t2 = time.time()
            break
    serial_port.timeout = 2.0

    assert t1 is not None and t2 is not None, "70s 内未收到两个开盖心跳"
    interval_ms = (t2 - t1) * 1000
    assert 28000 < interval_ms < 32000, (
        f"开盖心跳间隔 {interval_ms:.0f}ms，预期 30000±2000ms (SM_CHARGE_POLL_OPEN_MS)"
    )
