"""pytest fixtures for HIL testing.

Configure serial port via env var SGC_SERIAL_PORT (default COM3) and
SGC_SERIAL_BAUDRATE (default 115200). Tests require a USB-TTL adapter
wired to PA9 (RX) / PA10 (TX) / GND on the dev board.
"""
import os
import serial
import pytest

DEFAULT_PORT = os.environ.get("SGC_SERIAL_PORT", "COM3")
DEFAULT_BAUDRATE = int(os.environ.get("SGC_SERIAL_BAUDRATE", "115200"))


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
    import time
    import sgc_at
    serial_port.reset_input_buffer()
    sgc_at.send_command(serial_port, "RESET")
    time.sleep(0.5)
    serial_port.reset_input_buffer()
    sgc_at.send_command(serial_port, "OPEN")
    frame = sgc_at.recv_request(serial_port, timeout=10.0)
    if frame is None:
        pytest.fail("RESET+OPEN 后 10 秒内未收到心跳帧")
    return frame
