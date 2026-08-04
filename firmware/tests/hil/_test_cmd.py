"""Send OPEN then I2C, compare responses."""
import serial
import sys
import time

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM23"
ser = serial.Serial(PORT, 921600, timeout=2)

ser.reset_input_buffer()
ser.write(b"OPEN\n")
ser.flush()
time.sleep(1.0)
print(f"OPEN response: {ser.read(128)!r}")

ser.reset_input_buffer()
ser.write(b"I2C\n")
ser.flush()
time.sleep(1.0)
print(f"I2C response:  {ser.read(128)!r}")

ser.close()
