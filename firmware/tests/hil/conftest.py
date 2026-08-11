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


@pytest.fixture(autouse=True)
def _drain_firmware(serial_port):
    """Drain any leftover state before each test.

    Previous tests may leave firmware in HANDSHAKING or mid-OTA. Those
    states block update_mode_poll, so RESET/KEY/OTA commands get swallowed
    by hal_usart_recv. Sending heartbeats with case_version matching
    CASE_FW_VERSION for 2s avoids re-triggering version-mismatch OTA;
    then RESET clears state.
    """
    import time as _t
    import sgc_at

    rsp = sgc_at.pack_heartbeat_response(glass_soc=0x20, glass_sta=0x00, case_version=sgc_at.CASE_FW_VERSION)
    end = _t.time() + 2.0
    while _t.time() < end:
        serial_port.write(rsp)
        _t.sleep(0.05)
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()
    sgc_at.send_command(serial_port, "RESET")
    _t.sleep(0.3)
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()

    yield

    # Brief drain after test too
    end = _t.time() + 0.5
    while _t.time() < end:
        serial_port.write(rsp)
        _t.sleep(0.05)
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()


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
