# 硬件引脚与原理图核对（SCH_Schematic1_2026-08-04）

基于 `docs/schematic/Netlist_Schematic1_2026-08-04.tel` 与固件 `firmware/src/hal/hal_pinmux.h` 全量比对。

## 1. 核对结论

| 项 | 结论 |
|----|------|
| GD32 引脚映射 | **100% 匹配**，固件 hal_pinmux.h 与原理图 netlist 完全一致 |
| I2C 总线拓扑 | **正确**，PB6=SCL/PB7=SDA，3 个从机 + 上拉都挂对网络 |
| INT 引脚映射 | **正确**，PA8/PA11/PA12 分别连 CW2017/IP5353/MT5706 |
| 上拉电阻 | R16/R17 = **3.3kΩ 接 3V3** |
| CW2017 供电 | VDD 经 R38(470Ω) 接 **VBAT**，VCELL 经 R39(470Ω) 接 VBAT |

**I2C 全 NACK 不是引脚配置/地址错误，问题在物理层**（焊接/走线/供电/共地）。

## 2. 芯片清单（U 编号 → 功能）

| U# | 封装 | 芯片 | I2C 地址 | 识别依据 |
|----|------|------|----------|----------|
| U3 | QFN-32 | **IP5353**（移动电源 SOC） | 0x74 / 0x75 | 有 CC1/CC2(USB PD)、USBD±、VBUS_IN、VSYS、VOUT、LX |
| U8 | WLCSP-40 | **MT5706**（Qi2.0 无线充电 TX） | 0x2B | 有 COIL+/COIL-、PMIC_IN、G5=SDA/H5=SCL（与 MT5706 DS 一致） |
| U15 | TDFN8-2×2 | **CW2017**（燃料计） | 0x63 | pin7=SCL/pin8=SDA/pin5=INT_N（与 CW2016 DS 一致） |
| U6 | LQFP-48 | **GD32E230C8T6**（MCU） | — | 主控 |
| U1 | SOT-23-6 | DCDC（3V3 生成） | — | LX 经 L1 到 3V3 |
| U4 | SOT-23-5 | RT9013-18（1.8V LDO） | — | EN=U4.3 接 1V8EN |
| U9 | QFN14p | ET3328（SPDT 模拟开关） | — | POGO 5V/UART 切换 |
| U13 / U14 | TSSOP-8 / SC-70-6 | BL1551B（半双工收发） + 方向开关 | — | T/R_SWITH 控制 |
| U11 | LED-SMD 6P | A-SP1924 RGBWW LED | — | R/G/B/W 引脚 |
| U5 | SOT-23-5 | 船运模式 MOS 控制 | — | SHIP_CTR |
| U7 | CONN 2P | POGO 输出连接器 | — | VOUT_POGO |

## 3. 电源网络

| 网络 | 来源 | 关键负载 |
|------|------|----------|
| **3V3** | U1 DCDC → L1 → 3V3 | U6 VDD(pin1/9/24/48)、**R16/R17 上拉**、U9、U11、U13、J1.1、J2.3 |
| **VBAT** | 电池座 CN1.4/5 | **U15 CW2017 VDD 经 R38** / VCELL 经 R39、U3.14 IP5353、U4.1、L2.2 |
| **VSYS** | Q4 → U3 IP5353 | U3.11/31、C14/15/16 |
| **VBUS_IN** | USB → Q1 → | U3.27/28 IP5353 |
| **VOUT** | U3 IP5353 输出 | U9.3、C18/19 |
| **1V8** | U4 RT9013-18 → | U9/U13/U14（POGO UART 电平转换） |
| **1V8EN** | U6.21 PB10 | U4.3（LDO 使能） |
| **PMIC_IN** | Q1 | U8.E1/E2 MT5706 |
| **COIL+/COIL-** | U8 MT5706 → L3 | 无线充电线圈 |
| **GND** | — | 全板共地（含 U6.8/23/47、U15.4、U3.33、U8.A1/A2/A4/A5/E5/F5） |

## 4. GD32E230C8T6 (U6) LQFP-48 引脚映射

| Pin | 原理图网络 | 固件 GPIO | 功能 | 状态 |
|-----|-----------|-----------|------|------|
| 1 | 3V3 | VDD | 数字电源 | ✓ |
| 5 | PF0-OSCIN | — | HXTAL 12MHz | ✓ |
| 6 | PF1-OSCOUT | — | HXTAL | ✓ |
| 7 | NRST | — | 复位 | ✓ |
| 8 | GND | VSS | 数字地 | ✓ |
| 9 | 3V3 | VDD | 数字电源 | ✓ |
| 10–17 | PA0–PA7 | ADC_IN0–7 | 预留 ADC | ✓ |
| 18 | PB0 | ADC_IN8 | 预留 | ✓ |
| 19 | PB1 | ADC_IN9 | 预留 | ✓ |
| 20 | PB2 | LED_2812 | WS2812 | ✓ |
| 21 | 1V8EN | PB10 | 1.8V LDO 使能 | ✓ |
| 22 | CHIP_EN2 | PB11 | MT5706 EN（经 D7 或门） | ✓ |
| 23 | GND | VSS | 数字地 | ✓ |
| 24 | 3V3 | VDD | 数字电源 | ✓ |
| 25 | T/R_SWITH | PB12 | 半双工方向（注：原理图拼错"SWITH"） | ✓ |
| 26 | POGO_IN | PB13 | ET3328 IN（POGO 模式选择） | ✓ |
| 27 | SHIP_CTR | PB14 | 船运模式控制 | ✓ |
| 28 | RPD | PB15 | POGO 放电使能 | ✓ |
| 29 | BAT_INT | PA8 | CW2017 INT_N | ✓ |
| 30 | CPU_TX | PA9 | USART0 TX | ✓ |
| 31 | CPU_RX | PA10 | USART0 RX | ✓ |
| 32 | CHAGER_INT | PA11 | IP5353 INT（注：原理图拼错"CHAGER"） | ✓ |
| 33 | COIL_INTB | PA12 | MT5706 INT | ✓ |
| 34 | SWDIO | PA13 | SWD 调试 | ✓ |
| 35 | LEDB | PF6 | LED 蓝 | ✓ |
| 36 | LEDW | PF7 | LED 白 | ✓ |
| 37 | SWCLK | PA14 | SWD 调试 | ✓ |
| 38 | PA15 | — | 预留 | ✓ |
| 39 | KEY | PB3 | 按键 | ✓ |
| 40 | HALL_OUT_DIG | PB4 | 霍尔传感器 | ✓ |
| 41 | PB5 | — | 预留 | ✓ |
| 42 | **SCL** | **PB6** | **I2C0_SCL** | ✓ |
| 43 | **SDA** | **PB7** | **I2C0_SDA** | ✓ |
| 44 | BOOT0 | — | 启动模式 | ✓ |
| 45 | LEDR | PB8 | LED 红 | ✓ |
| 46 | LEDG | PB9 | LED 绿 | ✓ |
| 47 | GND | VSS | 数字地 | ✓ |
| 48 | 3V3 | VDD | 数字电源 | ✓ |

## 5. I2C 总线拓扑（网络 SCL / SDA）

```
                3V3
                 │
                ┌┴┐ R16 (3.3kΩ) 上拉
                └┬┘
                 │  ┌─────────── SCL 网络 ───────────┐
   U6.42 (PB6) ──┼──┤ U3.32 (IP5353)                │
                 │  │ U8.H5  (MT5706)                │
                 │  │ U15.7  (CW2017)                │
                 │  └────────────────────────────────┘
                 │
                ┌┴┐ R17 (3.3kΩ) 上拉
                └┬┘
                 │  ┌─────────── SDA 网络 ───────────┐
   U6.43 (PB7) ──┼──┤ U3.1  (IP5353)                │
                 │  │ U8.G5 (MT5706)                │
                 │  │ U15.8 (CW2017)                │
                 │  └────────────────────────────────┘
```

3 个从机地址：IP5353=0x74/0x75，MT5706=0x2B，CW2017=0x63。

## 6. 各芯片关键引脚连接

### CW2017 (U15, TDFN8)
| Pin | 功能 | 原理图连接 |
|-----|------|-----------|
| 1 | ID | 未接（NC） |
| 2 | VCELL | 经 R39(470Ω) + C62(1µF) → **VBAT** |
| 3 | VDD | 经 R38(470Ω) + C61(1µF) → **VBAT** |
| 4 | VSS | GND |
| 5 | INT_N | BAT_INT → U6.29 (PA8)，R41(100kΩ) 上拉到 3V3 |
| 6 | TS | BAT_NTC → CN1.3（NTC 热敏） |
| 7 | SCL | SCL |
| 8 | SDA | SDA |

### IP5353 (U3, QFN-32)
| Pin | 功能 | 原理图连接 |
|-----|------|-----------|
| 1 | SDA | SDA |
| 2 | INT | CHAGER_INT → U6.32 (PA11)，R18(?) 上拉 |
| 3–6 | LX | 升降压电感 L2 |
| 11/31 | VSYS | VSYS |
| 14 | VBAT | VBAT（电池） |
| 19/20 | USBD± | USB D+/D- |
| 21/22 | CC1/CC2 | USB Type-C PD |
| 27/28 | VBUS_IN | USB 输入 |
| 29/30 | VOUT | 系统输出 |
| 32 | SCL | SCL |
| 33 | GND | GND |

### MT5706 (U8, WLCSP-40)
| Ball | 功能 | 原理图连接 |
|------|------|-----------|
| G5 | SDA | SDA |
| H5 | SCL | SCL |
| H4 | INT | COIL_INTB → U6.33 (PA12) |
| H3 | CHIP_EN | 经 D7（BAT54C 或门）← CHIP_EN2 (U6.22/PB11) 或 CHIP_EN1 |
| E1/E2 | PMIC_IN | Q1 输出 |
| B1/B2/C1/C2 | COIL+ | 无线充电线圈 L3 正端 |
| B4/B5/C4/C5 | ACN | 异物检测/谐振 |
| A1/A2/A4/A5/E5/F5 | GND | 地 |

### MT5706 使能链路
```
U6.22 (PB11, CHIP_EN2) ──┐
                          ├─→ D7 (BAT54C 共阴或门) ──→ CHIP_EN ──→ U8.H3
CHIP_EN1 (R8/R13 控制) ──┘
```
固件 `hal_chip_en2_enable()` 拉高 PB11 → D7.2 高 → D7.3 (CHIP_EN) 高 → MT5706 启用。

## 7. 发现的问题

### 7.1 原理图拼写（不影响功能，建议修正）
- `T/R_SWITH`：少一个 C，应为 `T/R_SWITCH`
- `CHAGER_INT`：少一个 R，应为 `CHARGER_INT`

### 7.2 I2C 全 NACK 故障（当前主问题）
原理图核对无误，但实测 3 个从机全部不响应：
- 逻辑分析仪看到 GD32 PB6/PB7 上有正确的 START+addr+NACK+STOP 波形
- 全扫描 127 个地址零个 ACK
- INT 线（PA11/PA12/PA8）能读到状态 → GD32 与从机之间至少 INT 线通

**推断**：波形从 GD32 出来了，但没传到从机，**问题在 PCB 物理层**。

### 7.3 CW2017 VDD 非直供
CW2017 VDD 经 R38(470Ω) 从 VBAT 供电（datasheet 典型应用是 VDD 直供）。470Ω 在 CW2017 normal 电流 17µA 下压降 8mV，可忽略；但若 PCB 上 R38 漏焊/虚焊，VDD 会掉电导致 CW2017 不响应 I2C。

## 8. I2C 故障排查方向（按嫌疑优先级）

| 序 | 排查项 | 工具 | 期望 | 异常→结论 |
|----|--------|------|------|----------|
| 1 | **3V3 网络电压** | 万用表 DCV | 3.3V | 0V→U1 DCDC 没启动，上拉无效 |
| 2 | **VBAT 网络电压** | 万用表 DCV | 3.0–4.2V | 0V→电池未接/接触不良 |
| 3 | **R16/R17 上拉是否到位** | 万用表Ω档 | 3.3kΩ | 漏焊/阻值错 |
| 4 | **PB6→U3.32→U8.H5→U15.7 通断** | 蜂鸣档 | 全通 | 断线 |
| 5 | **PB7→U3.1→U8.G5→U15.8 通断** | 蜂鸣档 | 全通 | 断线 |
| 6 | SDA/SCL 对 GND 电阻 | 万用表 | > 100kΩ | 短路→芯片烧 |
| 7 | SDA 与 SCL 之间电阻 | 万用表 | > 100kΩ | 短路→走线粘连 |
| 8 | GD32 GND ↔ 从机 GND | 蜂鸣档 | 全通 | 共地丢 |
| 9 | **从机端 SDA/SCL 是否有波形**（探 U3/U8/U15 引脚） | 示波器 | 有 I2C 波形 | 无→PB6/PB7 到从机走线断 |
| 10 | CW2017 R38/R39 焊接 | 目检/Ω档 | 470Ω 焊好 | 虚焊→VDD 掉 |
| 11 | I2C 上升沿时间 | 示波器 | < 1µs | 缓坡→上拉太弱 |

## 9. 实测验证结论（2026-08-04）

### 9.1 核心 bug：I2C 地址参数错位（已修复）

GD32E230 SPL `i2c_master_addressing(addr, dir)` 期望 **8-bit 已左移地址**（`addr7 << 1`），因为内部直接 `I2C_DATA = addr & 0xFFFFFFFE`（写）/ `addr | 0x01`（读）后作为完整地址字节发送。

固件原 `hal_i2c_master_addr` 传 7-bit 地址，导致总线地址字节全错位（如 CW2017 应发 0xC6 实发 0x62）。**所有从机 NACK**，扫描现象吻合：`a=1→0x00, a=2,3→0x01, a=4,5→0x02`（`& 0xFE` 把奇偶映射到同一偶数，每地址重复两次）。

**修复**：`i2c_master_addressing(I2C0, (uint32_t)addr7 << 1U, direction)`（hal_i2c.c:117 + update_mode.c:141）。

### 9.2 三个 I2C 从机全部确认工作

| 设备 | 地址 | 测试条件 | 状态 |
|------|------|----------|------|
| CW2017 | 0x63 | 电池供电即可 | ✅ ACK |
| MT5706 | 0x2B | **必须放无线充板上**（线圈供电，PMIC_IN 无独立电源） | ✅ ACK |
| IP5353 | 0x74/0x75 | **无线充或 USB 触发 + 延迟 ~2s**（纯电池 VSYS=4.2V 但不响应） | ✅ ACK |

### 9.3 IP5353 充电控制正常（硬件自动）

USB 接入后观察电池电压 4150-4210mV 循环跳动，对应 SYS_STATE5(0x69) 状态变化：
- `0x3A` (bits[6:4]=3) CV 恒压充电 → 电压升到 4210mV
- `0x08` (bits[6:4]=0) 充电停止 → 电压回落到 4150mV
- `0x7A` (bits[6:4]=7) 重启过渡

这是 IP5353 PMIC 硬件自动执行锂电池**充满→截止→回落→重启**维护循环，防过充。固件不需干预。

### 9.4 CW2017 单体异常（不影响充电）

- VERSION(0x00) = **0x0F**（datasheet 应 0xA0）
- SOC(0x04) = **0xFE = 254%**（应 0-100）
- VCELL(0x02-03) = 4.15V **合理**

VCELL 准确（ADC 直读），但 VER/SOC 异常。最可能：**CW2017 出厂未烧录 battery profile** 或非标准版本。充电由 IP5353 独立控制，不依赖 CW2017，故不影响充电功能；但 SOC 报告值不可信，需更换已烧录 profile 的 CW2017 或重新校准。

### 9.5 INT 引脚映射确认（netlist 与固件一致）

- PA8 (BAT_INT) → CW2017 INT_N ✓
- PA11 (CHAGER_INT) → IP5353 INT ✓（注意原理图拼写"CHAGER"少 R）
- PA12 (COIL_INTB) → MT5706 INT ✓

### 9.6 原理图拼写问题（不影响功能）

- `T/R_SWITH` 应为 `T/R_SWITCH`（少 C）
- `CHAGER_INT` 应为 `CHARGER_INT`（少 R）

