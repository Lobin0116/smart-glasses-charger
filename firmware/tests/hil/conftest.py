"""pytest fixtures for HIL testing.

Configure serial port via env var SGC_SERIAL_PORT (default COM3) and
SGC_SERIAL_BAUDRATE (default 921600). Tests require a USB-TTL adapter
wired to PA9 (RX) / PA10 (TX) / GND on the dev board.
"""
import os
import serial
import pytest

DEFAULT_PORT = os.environ.get("SGC_SERIAL_PORT", "COM3")
DEFAULT_BAUDRATE = int(os.environ.get("SGC_SERIAL_BAUDRATE", "921600"))


@pytest.fixture(scope="session")
def serial_port():
    ser = serial.Serial(
        port=DEFAULT_PORT,
        baudrate=DEFAULT_BAUDRATE,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=2.0,
    )
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    yield ser
    ser.close()


@pytest.fixture(scope="module")
def heartbeat_frame(serial_port):
    input(
        "\n[HIL] 请用磁铁靠近然后远离霍尔传感器（PB4），触发开盖事件，然后按回车..."
    )
    import sgc_at
    frame = sgc_at.recv_request(serial_port, timeout=15.0)
    pytest.skip("未收到心跳帧") if frame is None else None
    return frame
