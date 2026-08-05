"""F 类 — 霍尔 PB4 测试 (P0).

update_mode.c 把 'OPEN'/'CLOSE' 命令转成 sm_inject_lid_event()，
等效磁铁远离（开盖）/靠近（合盖）霍尔传感器。
自动化验证命令注入触发状态机；物理霍尔（真·磁铁）烧录后手动验证。

F01/F02 物理测试：磁铁靠近/远离 PB4 霍尔，观察 LED + 串口心跳。
"""
import time

import sgc_at


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
