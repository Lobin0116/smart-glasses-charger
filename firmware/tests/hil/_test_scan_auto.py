"""Read continuous SCAN output from main.c (115200 8N1)."""
import serial
import sys
import time

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM23"
ser = serial.Serial(PORT, 115200, timeout=1)
ser.reset_input_buffer()
print(f"Reading {PORT} for 15s...")
end = time.time() + 15
while time.time() < end:
    line = ser.readline()
    if line:
        print(line.decode(errors="replace").rstrip())
ser.close()
