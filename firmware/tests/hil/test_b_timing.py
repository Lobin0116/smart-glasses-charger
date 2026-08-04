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
    timestamps = _collect_heartbeat_timestamps(serial_port, count=3, timeout=5.0)

    assert len(timestamps) >= 3, f"5s 内只收到 {len(timestamps)} 个心跳，期望 ≥3"

    intervals_ms = [(timestamps[i + 1] - timestamps[i]) * 1000 for i in range(len(timestamps) - 1)]

    for iv in intervals_ms:
        assert 150 < iv < 300, (
            f"心跳间隔 {iv:.0f}ms 超出 150-300ms 范围 "
            f"(期望 ~200ms = recv timeout 100ms + retry delay 100ms); "
            f"all intervals = {intervals_ms}"
        )
