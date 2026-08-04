"""Send I2C, read 10s to see if while(1) blocks main loop."""
import serial
import sys
import time

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM23"
ser = serial.Serial(PORT, 921600, timeout=1)
ser.reset_input_buffer()
print(f"Sending 'I2C\\n'...")
ser.write(b"I2C\n")
ser.flush()

print("Reading 10s...")
end = time.time() + 10
count = 0
while time.time() < end:
    chunk = ser.read(64)
    if chunk:
        count += 1
        elapsed = 10 - (end - time.time())
        print(f"  [+{elapsed:.2f}s] {chunk!r}", flush=True)

print(f"\nTotal chunks received: {count}")
if count <= 1:
    print("-> while(1) 卡住了 (只收到 I2C START, 之后无数据)")
else:
    print("-> while(1) 没卡 (后续有心跳等数据)")
ser.close()
