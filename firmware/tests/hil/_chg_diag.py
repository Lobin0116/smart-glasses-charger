"""无线充电唤醒诊断 — 轮询 HIL_CHG_DIAG 画时间线，好坏盒子对比。

用法 (PowerShell):
  cd <repo>\\firmware\\tests\\hil
  python _chg_diag.py COM23 [duration_s]

操作: 先跑脚本，然后立刻把充电盒放上无线充板（或上电前先跑脚本再放板）。
每 250ms 轮询一次固件快照: PA11/PA12/PB11 电平 + IP5353/CW2017 原始寄存器。
字段变化时用 * 标出。跑两遍 (好盒子/坏盒子) 对比时间线差异。

payload (11B):
  [0] flags: bit0=PA11(CHAGER_INT) bit1=PA12(COIL_INT) bit2=PB11(CHIP_EN2)
             bit3=mt5706_is_enabled
  [1] ok:    bit0=IP5353_0x45 bit1=IP5353_0x50 bit2=IP5353_0x69
             bit3=CW_VCELL bit4=CW_SOC bit5=CW_VER bit6=CW_CFG
  [2] IP5353 SYS_STATE0 (0x45): bit3 vbusok, bit5 vinok, bit2/6 ov
  [3] IP5353 SYS_STATE2 (0x50): bits[2:0] sys_state(101=chg), bit5 charge_en, bit4 boost_en
  [4] IP5353 SYS_STATE5 (0x69): bits[6:4] chg_state (0=idle 2=CC 3=CV 5=full)
  [5:7] CW2017 VCELL raw (14-bit, mv = raw*5/16)
  [7] CW2017 SOC
  [8] CW2017 VERSION (应 0xA0, 0x0F=profile 未烧录)
  [9] CW2017 CONFIG
"""
import sys
import time
import serial

sys.path.insert(0, '.')
import sgc_at

# IP5353 SYS_STATE5 bits[6:4] chg_state (datasheet):
CHG_STATE = {0: '未充电', 1: '涓流', 2: 'CC', 3: 'CV', 4: 'CV检测', 5: '充满', 6: '超时!', 7: '延时充'}


def decode(p):
    flags, ok = p[0], p[1]
    s0, s2, s5 = p[2], p[3], p[4]
    vraw = ((p[5] << 8) | p[6]) & 0x3FFF
    mv = vraw * 5 // 16
    f = {
        'PA11': (flags >> 0) & 1,
        'PA12': (flags >> 1) & 1,
        'PB11': (flags >> 2) & 1,
        'ENsw': (flags >> 3) & 1,
        'vbusok': (s0 >> 3) & 1,
        'vinok': (s0 >> 5) & 1,
        'chg_state': CHG_STATE[(s5 >> 4) & 7],
        'chg_en': (s2 >> 5) & 1,
        'mv': mv,
        'soc': p[7],
        'ver': p[8],
        'cfg': p[9],
        's0': s0, 's2': s2, 's5': s5,
        'ntc': p[10],
        'i2c_ok': ok,
    }
    return f


def fmt(f, prev):
    def mark(k):
        return '*' if prev is not None and prev[k] != f[k] else ' '

    i2c = f['i2c_ok']
    ip_ok = (i2c & 0x07) == 0x07
    cw_ok = (i2c & 0x78) == 0x78
    ntc_ok = (i2c & 0x80) != 0
    return (f"PA11={f['PA11']}{mark('PA11')} PA12={f['PA12']}{mark('PA12')} "
            f"PB11={f['PB11']}{mark('PB11')} ENsw={f['ENsw']}{mark('ENsw')} | "
            f"IP5353:{'OK' if ip_ok else f'FAIL({i2c&7:02b})'} "
            f"s0={f['s0']:02X}(vbusok={f['vbusok']} vinok={f['vinok']}) "
            f"s2={f['s2']:02X}(chg_en={f['chg_en']}) "
            f"s5={f['s5']:02X}({f['chg_state']}) "
            f"NTC={f['ntc']:02X}{mark('ntc')}{'!' if ntc_ok and f['ntc'] != 0 else ''} | "
            f"CW:{'OK' if cw_ok else f'FAIL({(i2c>>3)&0xF:04b})'} "
            f"{f['mv']}mV{mark('mv')} SOC={f['soc']}{mark('soc')} VER={f['ver']:02X} CFG={f['cfg']:02X}")


def poll_once(ser):
    """Lightweight single CHG_DIAG poll (no per-call logging)."""
    ser.write(sgc_at.pack_hil_command(sgc_at.OPCODE_HIL_CHG_DIAG))
    ser.flush()
    end = time.time() + 0.6
    while time.time() < end:
        f = sgc_at.recv_response(ser, timeout=0.2)
        if f is None:
            continue
        try:
            p = sgc_at.parse_frame(f)
        except ValueError:
            continue
        if p["opcode"] == sgc_at.OPCODE_HIL_CHG_DIAG and p["status"] == 0:
            return p["payload"]
    return None


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else 'COM23'
    dur = float(sys.argv[2]) if len(sys.argv) > 2 else 30.0

    ser = serial.Serial(port, 115200, timeout=0.2)
    ser.reset_input_buffer()
    sgc_at.reset_recv_buffer()
    print(f"轮询 CHG_DIAG {port}，{dur:.0f}s。现在把盒子放上无线充板...")
    print("(每次只显示变化时刻; *=字段变化)\n")

    t0 = time.time()
    prev = None
    last_line = ""
    n = 0
    fail = 0
    while time.time() - t0 < dur:
        payload = poll_once(ser)
        if payload is None:
            fail += 1
            print(f"[{time.time()-t0:6.2f}s] no response")
            time.sleep(0.5)
            continue
        n += 1
        f = decode(payload)
        line = fmt(f, prev)
        if line != last_line:
            print(f"[{time.time()-t0:6.2f}s] {line}")
            last_line = line
        prev = f
        time.sleep(0.25)

    print(f"\n共 {n} 次快照。判定参考:")
    print("  PA12 不动 & IP5353 FAIL     → MT5706 没输出 (线圈/耦合/硬件)")
    print("  PA12 动 & IP5353 FAIL       → PMIC_IN 没上到 IP5353 (硬件)")
    print("  IP5353 OK & vbusok/vinok=0  → IP5353 认为输入无效 (测 PMIC_IN 电压)")
    print("  vbusok=1 & chg_state=IDLE   → IP5353 拒充 (电池过放锁定/NTC/使能位)")
    print("  chg_state=CC/CV 但 LED 不亮 → 显示链路问题 (软件)")
    print("  CW VER=0x0F                 → CW2017 profile 未烧录 (SOC 不可信)")
    ser.close()


if __name__ == '__main__':
    main()
