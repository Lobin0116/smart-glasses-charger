# 需求-代码映射文档

本文档将所有源文件映射到具体的需求来源，方便对照规格文档审查代码。

## 需求来源缩写

| 缩写 | 文件 | 路径 |
|------|------|------|
| TIM | 双pin机盒通讯时序v0.1.xlsx | docs/spec/ |
| REQ | 眼镜盒产品需求v0.1.xlsx | docs/spec/ |
| PROT | AT+眼镜盒通信协议+.pdf | docs/spec/ |
| SCH | SCH_Schematic1_2026-07-10.pdf | docs/schematic/ |
| PIN | GD32E230C8T6 引脚定义表.xlsx | docs/schematic/ |
| CW | CW2017_Datasheet.pdf | docs/datasheet/ |
| IP | IP5353_Datasheet.pdf + Register_Map.pdf | docs/datasheet/ |
| ET | ET3328_Datasheet.pdf | docs/datasheet/ |

---

## HAL 层 (firmware/src/hal/)

### hal_pinmux.h
| 定义内容 | 需求来源 | 具体位置 |
|----------|---------|---------|
| 全部引脚端口/引脚号宏定义 | PIN | "完整引脚分配总表" sheet |
| hal_pin_t 枚举 (20个逻辑引脚) | PIN | "完整引脚分配总表" sheet |

### hal_gpio.c / hal_gpio.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| LED 输出 push-pull 初始低 (PB8/PB9/PF6/PF7/PB2) | PIN | LED 指示灯 section |
| 控制输出 push-pull 初始低 (PB10-PB15) | PIN | 电源/芯片使能输出 section |
| 输入上拉 (PB3 KEY, PB4 HALL) | PIN | 输入检测 section |
| 中断输入上拉 (PA8/PA11/PA12) | PIN | 中断输入 section |
| I2C AF1 开漏 (PB6/PB7) | PIN + SCH | I2C 总线 section |
| UART AF1 推挽 (PA9/PA10) | PIN + SCH | UART 串口 section |
| AF 编号 = 1 (USART0/I2C0) | GD32 Datasheet | Table 2-13, 2-14 AF summary |
| LED on/off/toggle 函数 | REQ | "指示灯" section |
| 按键低电平有效 (上拉, 按下为低) | PIN | PB3 KEY "上拉输入" |
| 使能脚函数 (1V8EN/CHIP_EN2/POGO_IN/SHIP_CTR/RPD/T/R_SWITCH) | PIN | 各引脚说明 |

### hal_usart.c / hal_usart.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| 波特率 921600 | TIM | 首页 "波特率：921600" |
| 8N1 (8数据位, 无校验, 1停止位) | TIM | 首页 "校验位：无/数据位：8/停止位：1" |
| 半双工 T/R_SWITCH 方向切换 | TIM + PIN | PB12 "半双工收发切换控制脚" |
| 通信电平 1.8V (硬件电平转换, 软件透明) | TIM | 首页 "通讯电平：1.8V" |
| 100ms 响应超时 | TIM | 首页 "一个指令发送和等待回码共占时100ms" |
| DMA TX=CH2, RX=CH1 | GD32 Datasheet | GD32E23x 固定 DMA 路由 |
| TC flag 等待 (确保最后一 bit 发完) | 工程实践 | TX 完成后再切方向 |

### hal_i2c.c / hal_i2c.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| 200kHz 总线速度 | IP | "I2C 建议用 200kHz（最高 300kHz）" |
| CW2017 地址 0x63 | CW | "Address of CW2017 is fixed on 0b1100011" |
| IP5353 控制寄存器 0x74, 状态寄存器 0x75 | IP | "I2C 设备地址有两组：0xE8/0xEA" |
| 100ms 超时防总线锁死 | 工程实践 | IP "MCU 操作流程" |
| 9-clock 总线恢复 | 工程实践 | I2C 标准恢复协议 |
| repeated START 读寄存器 | CW + IP | 先写寄存器地址再 restart 读 |

### hal_exti.c / hal_exti.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| PA8 BAT_INT 下降沿 | PIN | PA8 "外部中断，电量告警" |
| PA11 CHAGER_INT 下降沿 | PIN | PA11 "外部中断，充电状态上报" |
| PA12 COIL_INTB 下降沿 | PIN | PA12 "外部中断，无线充中断" |
| PB3 KEY 下降沿 | PIN | PB3 "上拉输入，按键检测" |
| PB4 HALL 双沿 | PIN | PB4 "霍尔磁场检测输入" |
| EXTI4_15 + EXTI2_3 中断向量 | GD32 Datasheet | 中断向量表 |

### hal_timer.c / hal_timer.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| SysTick 1ms 全局 tick | 工程实践 | 全模块超时判断基础 |
| hal_timer_delay_ms() | TIM | 所有 "300ms/100ms" 时序参数的执行 |
| hal_timer_expired() | TIM | 30s/60s/9min 等周期超时判断 |

### hal_pwr.c / hal_pwr.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| 5V 充电模式 (IN=L) | ET Truth Table | "IN=L/Floating, 1.5V<VPOGO<5.8V → POGO=CHG" |
| 100ms 放电泄放 (RPD=H) | ET Truth Table | "VCC1 Valid & VIO Valid & RPD=H → POGO Discharge" |
| 1.8V 通信模式 (IN=H + 1V8EN=H) | ET Truth Table | "IN=H & VPOGO<2V → POGO=UART" |
| 切换序列: 5V→放电→1.8V | TIM | "5V切Vbus充电，1.8V切Uart通信" |

### hal_fwdgt.c / hal_fwdgt.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| 2s 看门狗超时 | 工程实践 | 覆盖最长阻塞 (握手 800ms) |

---

## Driver 层 (firmware/src/drivers/)

### cw2017.c / cw2017.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| 上电序列: 写0x30→写0x00 到 CONFIG(0x08) | CW | "write 0x30 to clear sleep, then 0x00 to restart" |
| 验证 VERSION(0x00)==0xA0 | CW | Register Table "VERSION 0x00, Default 0xA0" |
| 读 SOC_H(0x04) 整数百分比 | CW | "high 8bit contains SOC in 1% units" |
| 读 VCELL(0x02-0x03) 转 mV, LSB=312.5uV | CW | "V(uV) = Value * 312.5" |
| 读 TEMP(0x06) 转摄氏度, T=-40+val/2 | CW | "TEMP(C) = -40 + Value/2" |

### ip5353.c / ip5353.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| SYS_STATE0(0x45) 读输入状态 (VIN/VBUS) | IP | "4.1 SYS_STATE0, addr 0x45" |
| SYS_STATE2(0x50) 读充电/Boost状态 | IP | "4.3 SYS_STATE2, addr 0x50" |
| SYS_STATE5(0x69) 读充电阶段 (恒流/恒压/满) | IP | "4.6 SYS_STATE5, addr 0x69" |
| INT 高 100ms 后才能 I2C | IP | "INT 持续为高 100ms 才可以读写" |
| read-modify-write 寄存器操作 | IP | "先读出来再进行计算后再写回" |

### mt5706.c / mt5706.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| CHIP_EN2(PB11) 使能/禁用无线充电 | PIN | PB11 "无线充芯片电源使能" |
| COIL_INTB(PA12) 中断事件标志 | PIN | PA12 "外部中断，无线充中断" |

### led.c / led.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| 4路 GPIO LED (红/绿/蓝/白) | PIN + SCH | PB8红/PB9绿/PF6蓝/PF7白 |
| WS2812(PB2) 备选 | SCH | "LED备选：带驱动芯片" |
| 呼吸效果 (软件 PWM 50Hz) | REQ | "充电盒指示灯呼吸" |
| 闪烁效果 (1Hz) | REQ | "红色灯闪烁7秒" |

---

## Protocol 层 (firmware/src/protocol/)

### at_types.h
| 定义内容 | 需求来源 | 具体位置 |
|----------|---------|---------|
| at_case_role_e 枚举 | PROT | "2.2.1 Payload 定义" |
| at_case_data (case_soc + case_sta) | PROT + TIM | TIM 心跳包 case_soc/case_sta 字段定义 |
| at_glass_data (glass_soc + glass_sta + case_version) | PROT + TIM | TIM 心跳包回复字段定义 |
| at_case_packet_prepare/read/transfer | PROT + TIM | TIM 烧录 prepare/read 结构体 |
| at_status 错误码枚举 | PROT | "2.2.2 状态码" |

### at_crc.c / at_crc.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| CRC8 256字节查找表 | PROT | "3. CRC校验" 完整查找表 |
| at_crc8(ptr, len, crc_origin) | PROT | 函数签名原文 |

### at_frame.c / at_frame.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| 请求帧 Magic 0x23415423 (#AT#) | PROT + TIM | "MAGIC（4 Byte）" |
| 响应帧 Magic 0x23415023 (#AP#) | PROT + TIM | "MAGIC（4 Byte）" |
| 帧头 10B: Magic+CRC+Size+Opcode+Status | PROT | "2.1 字段说明" |
| CRC 校验范围: Magic 到 Payload | PROT | 帧格式图 |

### at_opcode.h
| Opcode | 需求来源 | 具体位置 |
|--------|---------|---------|
| 0x3001 心跳包 | PROT + TIM | PROT "2.3 指令集" + TIM "心跳包" |
| 0x3002 关机/船运 | PROT + TIM | PROT "2.3" + TIM "关机/船运" |
| 0x3003 烧录准备 | PROT + TIM | PROT "2.3" + TIM "烧录准备(prepare)" |
| 0x3004 烧录数据 | PROT + TIM | PROT "2.3" + TIM "烧录(read)" |

---

## App 层 (firmware/src/app/)

### state_machine.c / state_machine.h
| 状态/转换 | 需求来源 | 具体位置 |
|-----------|---------|---------|
| ST_IDLE: 等待开盖 | TIM | 所有流程图的起点 |
| ST_HANDSHAKING: 5V/300ms→泄放→心跳retry3 | TIM | 开盖流程 1.1 / 入盒流程 2.2 |
| 握手成功 + 高电量 → CHARGING | TIM | 开盖流程 1.3 / 关盖流程 1.4 |
| 握手成功 + 低电量 → MAINTAINING | TIM | 开盖流程 2.3 / 入盒流程 1.3 |
| 握手失败 + 高电量 → FORCE_CHARGING | TIM | 开盖流程 1.5 |
| 握手失败 + 低电量 → IDLE | TIM | 开盖流程 2.4 / 入盒流程 1.4 |
| ST_CHARGING: 30s(开盖)/60s(关盖) 轮询 | TIM | 开盖 1.3 "每30s" / 关盖 1.4 "每60s" |
| ST_CHARGING + 充满 + 开盖 → MAINTAINING | TIM | 开盖流程 1.4 |
| ST_CHARGING + 充满 + 关盖 → SHUTTING_DOWN | TIM | 关盖流程 1.4 |
| ST_CHARGING + OTA请求 → ST_OTA | TIM | 盒子ota流程 1-4 |
| ST_MAINTAINING: 心跳 <1.2s | TIM | 开盖 2.3 "持续发心跳包(1.2s以内)" |
| ST_FORCE_CHARGING: 长供5V 3min/次 9min | TIM | 开盖流程 1.5 |
| ST_SHUTTING_DOWN: retry×5 | TIM | 关盖流程 1.4 "发5次" |
| 低电关机前发 AT_SHUTDOWN | REQ | "低电关机之前，发码给眼镜关机" |
| 开盖事件 → HANDSHAKING | TIM + REQ | 开盖流程触发 |
| 关盖事件 → 重新握手或显示电量 | TIM + REQ | 关盖流程触发 |
| 开/关盖触发 7s 电量显示 | REQ | "充电盒指示灯长亮7s，灭" |

### charge_flow.c / charge_flow.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| 握手: 5V/300ms 脉冲 | TIM | "开盖后输出5v 300ms让眼镜开机" |
| 握手: 0V/100ms 泄放 | TIM | "再输出0v 100ms泄放" |
| 握手: 心跳 retry×3, 间隔100ms | TIM | "心跳包握手失败需retry 3次，每次间隔100ms" |
| case_soc 字节: bit7=充电, [6:0]=SOC | TIM | "盒子电量: 最高位为1表示充电" |
| case_sta 字节: bit0=开盖, bit7=OTA | TIM | "bit0表示开关盖状态/bit7申请进入ota" |
| 充电轮询含 100ms 泄放 | TIM | "通信时注意加100ms的泄放" |
| 关机指令发送 | TIM | "关机/船运" 指令 |
| 强充: 长供5V + 3min 探测 | TIM | "长供5v，3分钟通信一次" |

### ota_flow.c / ota_flow.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| 心跳设 OTA 标志位等待同意 | TIM | OTA流程 "1.盒子申请进入ota，眼镜同意" |
| prepare 获取固件大小 | TIM | "2.盒子发prepare指令，眼镜回复size" |
| read 循环拉取数据包 | TIM | "3.盒子发read指令...眼镜端回复data" |
| END type 检测结束 | PROT | AT_PACKET_TYPE_END |
| 清 OTA 标志退出 | TIM | "4.直到烧录完成，盒子发心跳包清ota标志位" |

### led_effect.c / led_effect.h
| 灯效 | 需求来源 | 具体位置 |
|------|---------|---------|
| 外部充电呼吸: 白>40%/绿15-40%/红5-15%/红闪1-5% | REQ | "1.外部电源对充电盒充电" |
| 给眼镜充电呼吸 (各级电量颜色) | REQ | "2.充电盒给眼镜充电" |
| 充满白色长亮 | REQ | "充满电，白色指示灯长亮" |
| 眼镜&充电盒同时充满才白色 | REQ | "眼镜&充电盒同时充满才显示白色" |
| 电量查看长亮7s | REQ | "充电盒指示灯长亮7s，灭" |
| 红色闪烁 <5% | REQ | "1%＜盒子电量≤5%，红色灯闪烁7秒" |
| Overlay 优先级: 充电中被其他动作打断 | REQ | "充电过程中，有其他动作切入，优先显示切入动作" |

### button.c / button.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| 50ms 去抖 | 工程实践 | 消除机械抖动 |
| 短按 (<2s) 查电量 | REQ | "短按查询电量灯光提示" |
| 长按忽略 (BLE 功能已移除) | REQ | "长按3s触发蓝牙配对" — 已移除 |

### aux_logic.c / aux_logic.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| NTC 分区: 0-15°C/15-45°C/45-60°C/>60°C | REQ | "NTC 充电保护 分段保护" |
| NTC critical 停止充电 | REQ | 温度超过60°C停止 |
| USB 优先于无线充电 | 工程实践 | Type-C 为主要充电路径 |
| 复充阈值 <98% | TIM | "当眼镜电量低于98%的时候复充" |
| 无眼镜显示盒电量 | REQ | "无眼镜，盒子指示灯显示充电盒电量" |

### power_mgmt.c / power_mgmt.h
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| Deep-Sleep + EXTI 唤醒 | REQ | "合盖后...整机进入微安级低功耗待机" |
| EXTI pending 清除防弹簧 | 工程实践 | Deep-Sleep 前清 pending |
| Standby + SHIP_CTR 船运 | REQ | "船运模式 ✔YES" |
| 霍尔/按键/充电中断唤醒 | REQ | "开盖/合盖时磁场变化触发" |

---

## main.c
| 功能 | 需求来源 | 具体位置 |
|------|---------|---------|
| 系统初始化序列 | 工程实践 | 所有 HAL/Driver init |
| 超级循环 sm_tick + button_poll + led_poll | 工程实践 | Bare-metal 调度 |
| 5s 周期刷新 CW2017/IP5353 状态 | 工程实践 | case_soc/case_charging 更新 |
| 充电仲裁 charge_arbitrate | 工程实践 | USB 优先 |
| 看门狗喂狗 | 工程实践 | 每轮循环 |

---

## 单元测试 (firmware/tests/)

| 测试文件 | 测试目标 | assertion 数 |
|----------|---------|-------------|
| test_gpio_config.c | hal_gpio 引脚配置正确性 | 24 |
| test_protocol.c | at_crc8 + at_frame 组帧/解析 | 18 |
| test_charge_flow.c | case_soc/sta 字节 + NTC + 复充 + 仲裁 | 38 |
| test_state_machine.c | 状态转换路径全覆盖 | 19 |
