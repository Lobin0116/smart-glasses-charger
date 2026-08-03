#!/usr/bin/env python3
"""Temporary firmware update over USART0.

Triggers the app to jump to GD32E230 SystemMemory bootloader (via UPDATE
token), then burns the new bin via stm32loader.

Usage:
    python update.py [PORT] [BIN_FILE]

Default: COM3, firmware/build/smart_glasses_charger.bin

Requires:
    pip install pyserial stm32loader

Flow:
    1. Open USART0 at 921600 baud, send "UPDATE\\n".
    2. App jumps to SystemMemory bootloader at 0x1FFFF000.
    3. stm32loader connects at 115200 (ISP auto-baud) and burns the bin.
    4. Power-cycle the board to run the new firmware.
"""
import serial
import subprocess
import sys
import time

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM3"
BIN = sys.argv[2] if len(sys.argv) > 2 else "firmware/build/smart_glasses_charger.bin"

print(f"[1/2] Triggering SystemMemory bootloader on {PORT} (921600 baud)...")
try:
    ser = serial.Serial(PORT, 921600, timeout=2)
except serial.SerialException as e:
    print(f"ERROR: cannot open {PORT}: {e}")
    sys.exit(1)
ser.reset_input_buffer()
ser.write(b"UPDATE\n")
ser.flush()
ser.close()
time.sleep(0.5)

print(f"[2/2] Burning {BIN} via stm32loader (115200, SystemMemory ISP)...")
try:
    subprocess.run(
        ["stm32loader", "-p", PORT, "-b", "115200", "-w", BIN],
        check=True,
    )
except FileNotFoundError:
    print("ERROR: stm32loader not installed. Run: pip install stm32loader")
    sys.exit(1)

print("Done. Press RESET on the board to run the new firmware.")
