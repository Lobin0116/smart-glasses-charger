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
    """Reset firmware to IDLE before each test.

    RESET HIL command forces the state machine back to IDLE regardless of
    current state. update_mode_poll runs in main loop's blocking gaps
    (handshake attempt ~800ms, charge_poll ~200ms), so RESET lands within
    ~1s from any state except ST_OTA (synchronous ota_run).

    We deliberately do NOT send heartbeat responses here. Doing so would
    let firmware complete a handshake and enter CHARGING (30s heartbeat
    gap), and the leftover response frames then sit in the firmware RX
    buffer ahead of any HIL command — update_mode_poll peeks the response
    frame first and breaks (production protocol opcode), so RESET never
    reaches dispatch and the next test sees a polluted state.
    """
    import time as _t
    import sgc_at

    sgc_at.send_command(serial_port, "RESET")
    _t.sleep(0.3)
    serial_port.reset_input_buffer()
    sgc_at.reset_recv_buffer()

    yield

    # Query STATUS after each test for diagnostics (OTA fail_reason etc.).
    # Cheap (~50ms) and printed to stdout, only useful when a test fails.
    try:
        ack = sgc_at.send_command(serial_port, "STATUS")
        if ack:
            st = sgc_at.parse_hil_status(ack)
            print(f"[post-status] state={st['state']} ota_idx={st['ota_index']} "
                  f"ota_rxlen={st['ota_rxlen']} ota_fails={st['ota_fails']} "
                  f"ota_fail_reason={st['ota_fail_reason']} "
                  f"ota_succ_len={st['ota_succ_len']}", flush=True)
    except Exception:
        pass

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
