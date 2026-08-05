"""E 类 — 按键 PB3 测试 (P0).

update_mode.c 把 'KEY' 命令转成 button_on_press()，等效物理短按 PB3。
自动化部分验证命令链路（OK_KEY 回复）；LED 亮 7s 显示电量需人眼观察，
烧录固件后手动验证（pytest 不能看灯）。

物理按键测试（E02 50ms 去抖 / E03 长按 ≥2s 忽略）需真按 PB3，写为 manual。
"""
import time

import sgc_at


def test_e01_key_command_triggers_button_press(serial_port):
    """E01 自动化部分: KEY 命令 → OK_KEY 回复 → button_on_press() 调用.

    LED 部分（人眼）：KEY 命令后 LED 应亮 7s 显示电量再灭。烧录后手动确认。
    """
    serial_port.reset_input_buffer()
    sgc_at.send_command(serial_port, "RESET")
    time.sleep(0.5)
    serial_port.reset_input_buffer()

    sgc_at.send_command(serial_port, "KEY")
    time.sleep(0.3)
    reply = serial_port.read(64)
    assert b"OK_KEY" in reply, f"KEY 命令未回 OK_KEY，实际: {reply!r}"
