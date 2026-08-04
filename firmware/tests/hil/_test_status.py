"""Send STATUS command, parse CW2017 registers + clock tree."""
import re
import serial
import sys
import time

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM23"
ser = serial.Serial(PORT, 921600, timeout=2)
ser.reset_input_buffer()
ser.write(b"STATUS\n")
ser.flush()
time.sleep(0.6)
data = ser.read(128)
print(f"raw: {data!r}")

text = data.decode(errors="replace").strip()
labels = {
    "V": "CW2017 version",
    "S": "CW2017 soc",
    "C": "CW2017 config",
    "F": "SystemCoreClock",
    "H": "AHB actual",
    "P": "APB1 actual",
}

for m in re.finditer(r"([VSCHFP])=([0-9A-Fa-f]{2})(!?)", text):
    key, val, err = m.group(1), int(m.group(2), 16), m.group(3)
    label = labels.get(key, key)
    if key in "FHP":
        print(f"  {label:20} = {val} MHz (0x{val:02X})")
    else:
        status = "I2C_FAIL" if err else "OK"
        print(f"  {label:20} = 0x{val:02X}  {status}")

if "F=" in text and "H=" in text:
    fm = re.search(r"F=([0-9A-Fa-f]{2})", text)
    hm = re.search(r"H=([0-9A-Fa-f]{2})", text)
    if fm and hm:
        f_val = int(fm.group(1), 16)
        h_val = int(hm.group(1), 16)
        if f_val != h_val:
            print(f"\n-> ⚠️ SystemCoreClock={f_val}M 但 AHB 实际={h_val}M (不一致!)")
            if h_val == 18:
                print("-> 确认 system_clock_72m_irc8m 的 AHB=SYSCLK/4 bug")
                print("-> SysTick/hal_timer 慢 4 倍 (1 tick = 4ms)")
        else:
            print(f"\n-> 时钟一致 ({f_val}M)")
ser.close()
