# Smart Glasses Charger — GD32E230C8T6 Firmware

## 项目背景

AI 眼镜充电盒固件，主控 GD32E230C8T6 (ARM Cortex-M23, LQFP48, 64KB Flash, 8KB SRAM)。
BLE 功能已移除，不实现任何蓝牙相关逻辑。

## 架构决策

- 运行模型: Bare-metal 超级循环 + 状态机（无 RTOS）
- 构建系统: CMake + arm-none-eabi-gcc（命令行构建）
- 外设库: GD32E23x 官方 Standard Peripheral Library V2.5.0
- 代码格式: clang-format 统一风格
- 交付: firmware/ 目录自包含，零 AI 痕迹

## 硬件平台

### MCU: GD32E230C8T6
- 12MHz 外部晶振 (PF0/PF1)
- 32.768K RTC晶振 (PC13/PC14/PC15)
- SWD调试: PA13(SWDIO) / PA14(SWCLK)

### 电源链路
- IP5353 (I2C_AABC): 移动电源SOC，Type-C PD/QC 18W输入，升压5V输出
- TPS563201: 12V降压
- RT9013-18: 1.8V LDO (PB10/1V8EN控制开关)
- 电池: 2000mAh 锂离子

### 无线充电
- MT5706: Qi2.0 EPP 15W (PB11/CHIP_EN2控制开关)

### I2C 总线 (PB6 SCL / PB7 SDA, I2C0, 200kHz)
- CW2017 电量计: 7-bit addr 0b1100011, 写=0xC6 读=0xC7
- IP5353: 控制寄存器 写=0xE8 读=0xE9, 状态寄存器 写=0xEA 读=0xEB

### POGO 通信链路
- ET3328 (SPDT模拟开关): POGO pin 5V充电 / 1.8V UART 切换
- BL1551B (U14): 半双工收发方向切换 (PB12/T/R_SWITCH)
- AiPTB0102TA8: 3.3V↔1.8V UART电平转换
- 物理层: 2-pin Pogo-Pin, UART 921600 8N1, 1.8V电平

### ET3328 真值表 (MCU控制映射)
| 模式 | PB13(IN) | PB15(RPD) | PB10(1V8EN) | POGO状态 |
|------|----------|-----------|-------------|----------|
| 5V充电 | L | L | X | POGO↔CHG(5V) |
| 放电泄放 | X | H | H(1.8V on) | POGO放电 |
| 1.8V通信 | H | L | H(1.8V on) | POGO↔UART |

切换时序: 5V充电 → RPD=H放电100ms → IN=H切UART → 1V8EN=H开LDO → 通信

## 引脚分配表

| 引脚 | 信号 | 功能 | 外设 |
|------|------|------|------|
| PA0 | ADC_IN0 | 预留 | ADC |
| PA1 | ADC_IN1 | 预留 | ADC |
| PA2 | ADC_IN2 | 预留 | ADC |
| PA3 | ADC_IN3 | 预留 | ADC |
| PA4 | ADC_IN4 | 预留 | ADC |
| PA5 | ADC_IN5 | 预留 | ADC |
| PA6 | ADC_IN6 | 预留 | ADC |
| PA7 | ADC_IN7 | 预留 | ADC |
| PA8 | BAT_INT | 电量计中断 | GPIO输入(EXTI) |
| PA9 | CPU_TX | UART发送 | USART0_TX |
| PA10 | CPU_RX | UART接收 | USART0_RX |
| PA11 | CHAGER_INT | 有线充电中断 | GPIO输入(EXTI) |
| PA12 | COIL_INTB | 无线充电中断 | GPIO输入(EXTI) |
| PA13 | SWDIO | 调试 | SWD (禁占用) |
| PA14 | SWCLK | 调试 | SWD (禁占用) |
| PA15 | NC | 预留 | |
| PB0 | ADC_IN8 | 预留 | ADC |
| PB1 | ADC_IN9 | 预留 | ADC |
| PB2 | LED_2812 | WS2812数据 | GPIO输出 |
| PB3 | KEY | 按键(短按查电量) | GPIO输入(上拉) |
| PB4 | HALL_OUT_DIG | 霍尔磁场检测 | GPIO输入 |
| PB5 | NC | 预留 | |
| PB6 | SCL | I2C时钟 | I2C0_SCL (开漏) |
| PB7 | SDA | I2C数据 | I2C0_SDA (开漏) |
| PB8 | LEDR | 红色LED | GPIO输出 |
| PB9 | LEDG | 绿色LED | GPIO输出 |
| PB10 | 1V8EN | 1.8V LDO使能 | GPIO输出 |
| PB11 | CHIP_EN2 | 无线充电使能 | GPIO输出 |
| PB12 | T/R_SWITCH | 单线串口收发切换 | GPIO输出 |
| PB13 | POGO_IN | POGO通信使能(ET3328 IN) | GPIO输出 |
| PB14 | SHIP_CTR | 船运模式控制 | GPIO输出 |
| PB15 | RPD | POGO放电使能(ET3328 RPD) | GPIO输出 |
| PF0 | OSCIN | 12MHz晶振 | 固定保留 |
| PF1 | OSCOUT | 12MHz晶振 | 固定保留 |
| PF6 | LEDB | 蓝色LED | GPIO输出 |
| PF7 | LEDW | 白色LED | GPIO输出 |

## AT 通信协议

### 帧格式
Magic(4B) + CRC8(1B) + Size(2B) + Opcode(2B) + Status/Reserved(1B) + Payload(NB)
- 请求Magic: 0x23415423 (#AT#)
- 响应Magic: 0x23415023 (#AP#)
- CRC-8 使用协议文档提供的256字节查找表

### 指令集
| Opcode | 名称 | REQ Payload | RSP Payload |
|--------|------|-------------|-------------|
| 0x3001 | 心跳包 | at_case_data | at_glass_data |
| 0x3002 | 关机/船运 | at_case_role | at_case_role |
| 0x3003 | 烧录准备 | at_case_role | at_case_packet_prepare |
| 0x3004 | 烧录数据 | at_case_packet_read | at_case_packet_transfer |

### 状态字段
- case_soc: bit7=1 充电中
- case_sta: bit0=开关盖(1开0关), bit7=申请OTA
- glass_soc: bit7=1 满电(0xE4=充满)
- glass_sta: bit7=1 同意

## 状态机设计

### 状态定义
- IDLE: 盖子关闭，无眼镜在盒，低功耗待机
- HANDSHAKING: 5V/300ms→0V/100ms泄放→1.8V发心跳(retry×3, 100ms间隔)
- CHARGING: 握手成功，供5V，30s(开盖)/60s(关盖)周期通信
- MAINTAINING: 握手成功但低电不充电，持续心跳维持入盒
- FORCE_CHARGING: 握手失败30s后，长供5V强充(3min/次, 持续9min)
- SHUTTING_DOWN: 充满/低电，发关机指令(retry×5)，然后休眠
- OTA: OTA传输流程
- SHIP_MODE: 船运模式

### 入盒检测机制
盒子发心跳 → 眼镜回复 = 入盒
盒子发心跳 → 无回复(retry 3次失败) = 不在盒/无法通信
眼镜端靠有无5V + 1.2s内心跳包维持或判断出盒

## 低功耗策略
- 正常休眠: Deep-Sleep，EXTI唤醒(霍尔/按键/CHAGER_INT/BAT_INT)，<50uA
- 船运模式: Standby + PB14(SHIP_CTR)外部控制，<5uA

## LED 灯效规则
- 充电中(呼吸): 白(>40%) / 绿(15-40%) / 红(5-15%) / 红闪(1-5%)
- 充满: 白色长亮
- 查看电量(开盖/关盖): 对应颜色长亮7s灭
- 低电分级: >15%可充电可通信 / 5-15%可充电可通信 / 1-5%不可充电可开关 / <=0%关机

## IP5353 操作约束
- INT 持续为高 100ms 后才可读写 I2C 寄存器
- 修改寄存器需先读后改再写（read-modify-write），不可直接覆盖
- I2C 建议用 200kHz（最高 300kHz）

## NTC 温度保护分段阈值
- 0°C ~ <15°C: 0.2C / 4.45V
- 15°C ~ <45°C: 1.5C / 4.45V
- 45°C ~ 60°C: 0.5C / 4.1V
- 放电保护: -20°C ~ 60°C (TBD)

## 通信超时
- AT 协议请求→响应周期: 100ms（超时即判失败）

## 低电阈值（待统一）
- 时序文档用电压: Vbat ≤ 3.4V = 低电
- 产品需求用 SOC%: ≤15% = 低电
- 3.4V 对应锂离子约 10-15% SOC，两文档不完全对齐
- 固件倾向以 CW2017 SOC% 为准，3.4V 作为后备判断

## LED 补充规则
- 眼镜&充电盒同时充满才显示白色
- 5min 无活动 → LED 关闭 → 进入休眠
- 低电关机前必须发 AT 关机指令给眼镜（适用所有低电休眠场景）

## 关键时序参数汇总
- 开盖/入盒握手: 5V/300ms → 0V/100ms泄放 → 1.8V心跳 retry×3(100ms间隔)
- 心跳维持入盒: <1.2s 间隔（眼镜端出盒检测超时）
- 开盖充电通信: 30s 周期
- 关盖充电通信: 60s 周期
- 强充: 30s握手失败后 → 长供5V, 3min/次, 持续9min
- 关机指令: retry×5, 无回复则认为已关机
- 充满后复充阈值: 眼镜电量 <98%
- LED电量显示: 长亮7s后灭

## 开发进度

实现方式: Claude Code (claude --dangerously-skip-permissions -p "..." --max-turns 30) 实现代码 → 子agent验证 → git commit。GD32 SPL 已解压至 /tmp/gd32_fwlib2/。

| Task | 内容 | 状态 | Commit |
|------|------|------|--------|
| 1 | 项目骨架 (SPL/CMSIS/CMake) | DONE | a91119f |
| 2 | HAL GPIO 初始化 | NEXT | |
| 3 | HAL USART0 (921600 半双工) | PENDING | |
| 4 | HAL I2C0 (200kHz) | PENDING | |
| 5 | HAL EXTI (霍尔/按键/中断) | PENDING | |
| 6 | HAL Timer (1ms tick) | PENDING | |
| 7 | HAL 电源切换 ET3328 | PENDING | |
| 8 | Driver CW2017 电量计 | PENDING | |
| 9 | Driver IP5353 充放电 | PENDING | |
| 10 | Driver MT5706 无线充电 | PENDING | |
| 11 | Driver LED | PENDING | |
| 12 | Protocol at_types/at_crc | PENDING | |
| 13 | Protocol at_frame | PENDING | |
| 14 | Protocol at_opcode | PENDING | |
| 15 | App 状态机核心 | PENDING | |
| 16 | App 开盖流程 | PENDING | |
| 17 | App 关盖流程 | PENDING | |
| 18 | App OTA 流程 | PENDING | |
| 19 | App LED 灯效 | PENDING | |
| 20 | App 低功耗管理 | PENDING | |
| 21 | App 按键处理 | PENDING | |
| 22 | main.c 整合 | PENDING | |
| 23 | 构建验证 | PENDING | |
| 24 | App 眼镜入盒流程 | PENDING | |
| 25 | App NTC温度保护 | PENDING | |
| 26 | App 有线/无线充电仲裁 | PENDING | |
| 27 | App 复充逻辑 | PENDING | |
| 28 | App 开盖无眼镜分支 | PENDING | |
| 29 | App LED并发优先级 | PENDING | |
| 30 | App 关盖无眼镜分支 | PENDING | |
| 31 | 5min空闲超时 | CANCELLED (BLE相关) | |
| 32 | HAL 看门狗 IWDG | PENDING | |

## 待确认项 (Pending)

1. LED方案: WS2812(PB2) vs 4路GPIO LED(PB8/PB9/PF6/PF7) — 待确认实际PCBA
   - 不阻塞开发，hal层抽象接口，后续切换驱动文件即可
2. A-SP1924RBGWW datasheet — 标注在POGO驱动区域，可能为霍尔信号调理
   - 不阻塞开发
