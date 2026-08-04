"""STATUS first (confirm alive), then stream SCAN output."""
import serial
import sys
import time

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM23"
ser = serial.Serial(PORT, 921600, timeout=1)

print("[1] STATUS (confirm firmware alive)...")
ser.reset_input_buffer()
ser.write(b"STATUS\n")
ser.flush()
time.sleep(1)
status = ser.read(128)
print(f"  {status!r}")

print("\n[2] SCAN (stream for 20s)...")
ser.reset_input_buffer()
ser.write(b"SCAN\n")
ser.flush()

end = time.time() + 20
buf = bytearray()
while time.time() < end:
    chunk = ser.read(32)
    if chunk:
        buf.extend(chunk)
        elapsed = 20 - (end - time.time())
        print(f"  [+{elapsed:.1f}s] {chunk!r}", flush=True)
    if b"\n" in buf and b"I2C:" in buf:
        break

print(f"\nTotal SCAN output: {buf!r}")
if b"I2C:" in buf:
    text = buf.decode(errors="replace")
    addrs = text.split("I2C:", 1)[1].strip().split()
    print(f"响应地址: {[f'0x{a}' for a in addrs]}")
ser.close()
