# HIL 测试矩阵 (Hardware-in-the-Loop)

GD32E230 充电盒固件硬件在环测试方案。PC 模拟眼镜端，通过 USART0 跟固件双向 AT 帧交互，验证固件实现是否满足 `docs/spec/` 下需求文档。

## 测试架构

### 硬件接线（直连 PA9/PA10）

| 核心板 | USB-TTL 模块 |
|--------|-------------|
| PA9 (USART0 TX) | RXD |
| PA10 (USART0 RX) | TXD |
| GND | GND |

- 电平：3.3V（核心板原生，别接 5V TTL）
- 串口参数：`921600 8N1 无校验`
- 绕过 BL1551B 开关（固件仍切 PB12 但不接开关元件），全双工
- 霍尔触发：小磁铁靠近/远离 PB4 上的霍尔传感器
- 按键触发：手动按 PB3 按键

### 软件框架

- 目录：`firmware/tests/hil/`（新建，独立于现有 C 单元测试）
- 语言：Python 3
- 依赖：`pyserial`（串口）、`pytest`（测试框架）
- 入口：`pytest firmware/tests/hil/` 或 `python -m pytest`
- 固件：启用 HIL 测试钩子（`update_mode_poll` 的 OPEN/CLOSE/KEY/RESET 命令 + `sm_inject_lid_event` + ST_IDLE 不进 Deep-Sleep），编译 `firmware/build/smart_glasses_charger.hex`
- 环境变量：`SGC_SERIAL_PORT=COMx`（默认 COM3），`SGC_SERIAL_BAUDRATE=921600`

### 测试交互模型

PC 是"眼镜端"角色：
1. 监听 USART0，等待固件主动发的心跳/关机/OTA 帧
2. 解析帧，按协议回复响应（在 100ms 内）
3. 通过响应内容控制固件状态机走向（握手成功/失败、低电/高电、OTA 同意/拒绝）
4. 通过观察后续 AT 帧的内容和时序推断固件状态

**无法直接观察的内部状态**（如 CHARGING vs MAINTAINING），靠心跳间隔区分：
- CHARGING：30s（开盖）/ 60s（关盖）
- MAINTAINING：< 1.2s
- FORCE_CHARGING：3min

---

## 测试矩阵

### 分类 A — AT 协议帧格式 ✅ PASS 7/7 (A01/A03/A04/A07/A11/A13/A15)

| ID | 描述 | 来源 | 步骤 | 预期 | 分类 | P |
|----|------|------|------|------|------|---|
| A01 | 请求帧 Magic = `0x23415423` (#AT#) | PROT / TIM | 触发握手，抓请求帧前 4 字节 | `23 54 41 23` | 核心 | P0 |
| A02 | 响应帧 Magic = `0x23415023` (#AP#) | PROT / TIM | PC 发合法响应，固件接受 | 固件按响应走流程（不 retry） | 核心 | P0 |
| A03 | 帧头结构 10B：Magic(4)+CRC(1)+Size(2)+Opcode(2)+Status(1) | PROT | 抓任意帧量长度 | 头部 10 字节，Size 含 payload | 核心 | P0 |
| A04 | CRC8 校验正确（256 字节查找表） | PROT | PC 算 CRC 对比收到的 CRC | 一致 | 核心 | P0 |
| A05 | CRC 校验范围：Magic 到 Payload（不含自身） | PROT | 改 payload 一字节，重算 CRC | 固件接受新 CRC、拒绝旧 CRC | 核心 | P0 |
| A06 | 坏 CRC 帧被拒绝 | PROT | PC 故意发坏 CRC 响应 | 固件当无响应处理（retry） | 核心 | P1 |
| A07 | Opcode 0x3001 心跳请求 | PROT / TIM | 抓握手期间帧 | opcode = 0x3001 | 核心 | P0 |
| A08 | Opcode 0x3002 关机/船运 | PROT / TIM | 触发低电关机流程 | 收到 opcode = 0x3002 | 核心 | P0 |
| A09 | Opcode 0x3003 烧录准备 | PROT / TIM | 触发 OTA prepare | 收到 opcode = 0x3003 | 核心 | P1 |
| A10 | Opcode 0x3004 烧录数据 | PROT / TIM | 触发 OTA read | 收到 opcode = 0x3004 | 核心 | P1 |
| A11 | 心跳 payload `at_case_data`：role(2)+case_soc(1)+case_sta(1) | TIM / 代码 at_types.h | 解析心跳 payload | 结构匹配 | 核心 | P0 |
| A12 | case_soc bit7=充电中 / [6:0]=SOC% | TIM | 设 SOC 不同值（靠固件端 CW2017，核心板固定 0） | bit7 跟随充电状态 | 核心 | P1 |
| A13 | case_sta bit0=开盖 / bit7=OTA申请 | TIM | 开盖/合盖 + 触发 OTA | bit0/bit7 跟随 | 核心 | P0 |
| A14 | 响应 payload `at_glass_data`：role(2)+glass_soc(1)+glass_sta(1)+case_version(1) | TIM | 解析心跳响应 | 结构匹配 | 核心 | P0 |
| A15 | glass_soc bit7=充满（0xE4 满电） | TIM | PC 回 glass_soc=0xE4 | 固件进"眼镜满电"分支 | 核心 | P0 |
| A16 | glass_sta bit7=同意 OTA | TIM | PC 回 glass_sta bit7=1 | 固件进 OTA 流程 | 核心 | P1 |

### 分类 B — 时序

| ID | 描述 | 来源 | 步骤 | 预期 | 分类 | P |
|----|------|------|------|------|------|---|
| B01 | 握手 5V 脉冲 300ms | TIM 1.1 | 示波器测 ET3328 5V 输出（需 ET3328） | 300ms ± 5% | 待 PCB | P1 |
| B02 | 握手泄放 100ms | TIM 1.1 | 示波器测泄放段 | 100ms ± 5% | 待 PCB | P1 |
| B03 | 心跳 retry×3 间隔 100ms | TIM 1.1 | 握手期间不回响应，测 3 次请求间隔 | ~100ms ± 5% | 核心 | P0 |
| B04 | 单次请求-响应超时 100ms | TIM 首页 | PC 故意延迟响应，测固件放弃时间 | ~100ms | 核心 | P0 |
| B05 | 握手失败后 30s 进强充 | TIM 1.5 | 不回响应，测 30s 后行为变化 | ~30s 后心跳间隔变 3min | 核心 | P1 |
| B06 | 强充周期 3min，持续 9min | TIM 1.5 | 测强充期间心跳间隔 + 总时长 | 3min±5s，9min 后休眠 | 核心 | P2 |
| B07 | 开盖充电心跳 30s | TIM 1.3 | 握手成功后测心跳间隔 | ~30s | 核心 | P1 |
| B08 | 关盖充电心跳 60s | TIM 盒子ota 1.4 | 关盖后测心跳间隔 | ~60s | 核心 | P1 |
| B09 | MAINTAINING 心跳 <1.2s | TIM 1.4 | 低电握手成功后测心跳间隔 | <1200ms | 核心 | P1 |
| B10 | 复充阈值眼镜电量<98% | TIM 1.4 | 模拟眼镜从满电掉到 97% | 固件切回 CHARGING | 核心 | P2 |
| B11 | 关机指令 retry×5 | TIM 盒子ota 1.4 | 触发关机，测重试次数 | 5 次 | 核心 | P1 |
| B12 | 1.2s 出盒检测（眼镜端逻辑） | TIM 眼镜端逻辑 | PC 1.2s 不回心跳 | 固件判出盒（HANDSHAKE 失败） | 核心 | P2 |

### 分类 C — 状态机转换

| ID | 描述 | 来源 | 步骤 | 预期 | 分类 | P |
|----|------|------|------|------|------|---|
| C01 | IDLE + 开盖 → HANDSHAKING | TIM | 磁铁触发开盖 | 收到心跳帧 | 核心 | P0 |
| C02 | HANDSHAKING 成功 + 高电 → CHARGING | TIM 1.3 | 回合法响应 + 固件高电 | 心跳转 30s 周期 | 核心 | P0 |
| C03 | HANDSHAKING 成功 + 低电 → MAINTAINING | TIM 低电流程 | 回合法响应 + 固件低电 | 心跳转 <1.2s | 核心 | P0 |
| C04 | HANDSHAKING 失败 30s + 高电 → FORCE_CHARGING | TIM 1.5 | 不回响应 30s | 心跳转 3min | 核心 | P1 |
| C05 | HANDSHAKING 失败 + 低电 → IDLE | TIM 低电 1.4 | 不回响应 + 低电 | 进入休眠（无心跳） | 核心 | P1 |
| C06 | CHARGING + 充满 + 开盖 → MAINTAINING | TIM 1.4 | 开盖状态回 glass_soc=0xE4 | 心跳转 <1.2s | 核心 | P1 |
| C07 | CHARGING + 充满 + 关盖 → SHUTTING_DOWN | TIM ota 1.4 | 关盖状态回 glass_soc=0xE4 | 收到关机帧 0x3002 | 核心 | P0 |
| C08 | 任意 + OTA 申请 → OTA | TIM ota | 回 case_sta bit7=1 | 收到 0x3003 prepare | 核心 | P1 |
| C09 | OTA 完成 → 回原状态 | TIM ota 4 | 跑完 OTA 流程 | 回 CHARGING/MAINTAINING | 核心 | P2 |
| C10 | FORCE_CHARGING 9min → IDLE | TIM 1.5 | 强充 9min 后无响应 | 进入休眠 | 核心 | P2 |
| C11 | SHUTTING_DOWN retry×5 → IDLE | TIM ota 1.4 | 触发关机，不回响应 | 5 次后休眠 | 核心 | P1 |
| C12 | 低电关机前必发 AT_SHUTDOWN | REQ | 低电场景观察 | 收到 0x3002 | 核心 | P0 |
| C13 | 上电初始状态读 PB4（合盖/开盖） | 代码 sm_init | 上电时 PB4 状态 | 行为匹配（**当前代码 bug：sm_init 不读 PB4，待修**） | 核心 | P1 |

### 分类 D — LED 灯效（视觉观察，难全自动）

| ID | 描述 | 来源 | 步骤 | 预期 | 分类 | P |
|----|------|------|------|------|------|---|
| D01 | 充电中呼吸（白>40%/绿15-40%/红5-15%/红闪1-5%） | REQ | 设不同 SOC（需 CW2017） | 颜色匹配 | 待 PCB | P1 |
| D02 | 眼镜充电呼吸 | REQ | 握手成功充电中 | 呼吸灯效 | 待 PCB | P1 |
| D03 | 充满白色长亮 | REQ | 眼镜+盒子都满电 | 白色常亮 | 待 PCB | P1 |
| D04 | 眼镜&盒子同时满才白 | REQ | 只一边满 | 不是白色 | 待 PCB | P2 |
| D05 | 电量查看长亮 7s | REQ | 短按 PB3 | LED 亮 7s 后灭 | 核心 | P0 |
| D06 | 红闪 <5% | REQ | SOC<5% | 红色闪烁 | 待 PCB | P1 |
| D07 | 低电 0% 关机 | REQ | SOC=0 | 关机流程 | 待 PCB | P1 |
| D08 | 开/关盖触发 7s 电量显示 | REQ | 磁铁触发开/合盖 | LED 亮 7s | 核心 | P0 |
| D09 | SOC=0（CW2017 不在位）→ 红闪 | 代码 led_effect | 核心板默认 SOC=0 | 红色 1Hz 闪 | 核心 | P0 |

### 分类 E — 按键 (PB3)

| ID | 描述 | 来源 | 步骤 | 预期 | 分类 | P |
|----|------|------|------|------|------|---|
| E01 | 短按 <2s 查电量 | REQ | 按 PB3 <2s 松开 | LED 显示电量 7s | 核心 | P0 |
| E02 | 50ms 去抖 | 代码 button.c | 抖动按（<50ms） | 不响应 | 核心 | P2 |
| E03 | 长按 ≥2s 忽略（BLE 移除） | REQ | 按 PB3 ≥2s | 无响应（不触发配对） | 核心 | P1 |

### 分类 F — 霍尔 (PB4)

| ID | 描述 | 来源 | 步骤 | 预期 | 分类 | P |
|----|------|------|------|------|------|---|
| F01 | 开盖（磁铁远离）触发事件 | TIM | 磁铁从霍尔移开 | PB4 升高 + 收到心跳 | 核心 | P0 |
| F02 | 合盖（磁铁靠近）触发事件 | TIM | 磁铁靠近霍尔 | PB4 降低 + 状态变化 | 核心 | P0 |
| F03 | 双沿都能触发 | 代码 hal_exti | 反复靠近/远离 | 每次翻转都响应 | 核心 | P1 |
| F04 | 20ms 软件去抖生效 | 代码 main.c | 快速抖动磁铁 | 不误触发 | 核心 | P2 |

### 分类 G — 低功耗（需万用表/电流计）

| ID | 描述 | 来源 | 步骤 | 预期 | 分类 | P |
|----|------|------|------|------|------|---|
| G01 | Deep-Sleep 电流 <50µA | REQ | 合盖静置，测电流 | <50µA | 待 PCB | P1 |
| G02 | Standby 电流 <5µA | REQ | 触发船运模式 | <5µA | 待 PCB | P1 |
| G03 | WWDGT 在 Deep-Sleep 自动停（不复位） | 代码 hal_wwdgt | Deep-Sleep >2s 看是否复位 | 不复位 | 核心 | P0 |
| G04 | 船运模式 SHIP_CTR 拉高 | REQ / 代码 pm_enter_ship_mode | 触发船运 | PB14 高 | 待 PCB | P1 |

### 分类 H — 充电链路（需完整 PCB）

| ID | 描述 | 来源 | 步骤 | 预期 | 分类 | P |
|----|------|------|------|------|------|---|
| H01 | IP5353 Type-C 输入充电检测 | FUNC / IP | 插 Type-C | CHAGER_INT 触发 + 充电指示 | 待 PCB | P1 |
| H02 | CW2017 SOC 读取 | FUNC / CW | 读 case_soc | 跟实际电量匹配（±3%） | 待 PCB | P1 |
| H03 | CW2017 电压读取 | CW | 读 voltage_mv | 跟万用表一致（±50mV） | 待 PCB | P2 |
| H04 | CW2017 温度读取（NTC 保护） | REQ / CW | 改温度 | 0-15℃/15-45℃/45-60℃ 分段 | 待 PCB | P1 |
| H05 | MT5706 无线充电 | FUNC / MT | 放无线底座 | COIL_INT 触发 + 充电 | 待 PCB | P2 |
| H06 | ET3328 5V/UART 切换时序 | ET / TIM | 示波器测切换波形 | 匹配 CONTEXT.md 时序表 | 待 PCB | P1 |
| H07 | 充电仲裁 USB > 无线 | 代码 aux_logic | 同时插 Type-C + 无线 | 走 USB | 待 PCB | P2 |
| H08 | NTC >60℃ 停止充电 | REQ | 加热 NTC | 停止充电 | 待 PCB | P1 |

### 分类 I — OTA 流程（**整体跳过，协议待重新设计**）

> **用户决策**：原 OTA 设计（盒子烧眼镜）有缺陷，要改为"眼镜申请更新盒子固件"（方向反转）。新协议确认中。I 类全部标记跳过，等新协议落地后再补。

| ID | 描述 | 来源 | 步骤 | 预期 | 分类 | P |
|----|------|------|------|------|------|---|
| I01-I09 | OTA 全流程 | TIM ota | — | — | **跳过** | — |

---

## 执行优先级

### P0 — 必测（核心功能，立即可执行）
- A01-A05（帧格式 + CRC）
- A07, A11, A14（心跳结构）
- B03, B04（retry + 超时）
- C01, C02, C03（基础状态转换）
- C07, C12（关机流程）
- D05, D08, D09（电量显示）
- E01（按键查电量）
- F01, F02（霍尔触发）
- G03（WWDGT Deep-Sleep 共存）

### P1 — 重要（验证完整性）
- A06, A08-A10, A12, A13, A15, A16（其他协议字段）
- B05, B07, B08, B09, B11（充电/维持时序）
- C04-C06, C08, C11, C13（其他状态转换）
- D01-D03, D06（LED 灯效，部分需 PCB）
- E03, F03（去抖/双沿）
- G01, G02, G04（功耗，需 PCB）
- H01-H04, H06, H08（充电硬件）
- I01-I04（OTA）

### P2 — 完整性补充
- 边缘场景、长周期测试、复充逻辑、视觉细节

---

## 待确认项（需用户决策）

### #1 OTA 触发逻辑未实现 + 协议待重新设计（**已决策：跳过**）

**前因**：原设计是"盒子烧眼镜"（盒子主动申请 OTA）。代码 `ota_requested` 没人设 true，且协议里没有"眼镜固件版本"字段（盒子无从版本对比触发）。

**用户决策（2026-08-03）**：OTA 设计有缺陷，方向要反转 —— 改为"**眼镜申请更新盒子固件**"（眼镜端主导）。新协议确认中。

**处理**：I 类全部跳过，等新协议落地后重新设计测试。

### #2 sm_init 不读 PB4（**已澄清，可直接修**）

**前因**：`sm_init`（state_machine.c:198）直接 `ctx->lid_open = false`，不调 `hal_hall_get()`。上电时若 PB4 已是高（开盖），MCU 当合盖处理。

**澄清（2026-08-03）**：霍尔是**全极**型号（任何极性磁铁靠近都触发），行为确定 —— 靠近=低、离开=高。代码"高=开盖、低=合盖"的假设**正确**，不需要反转宏。

**处理**：sm_init 读 PB4 这个修复跟极性无关（永远正确），可以直接修：`ctx->lid_open = hal_hall_get();`（一行改动）。

### #3 低电场景模拟（**已消除，CW2017 在位**）

**前因**：之前误以为核心板 CW2017 不在位，case_soc 恒为 0。

**澄清（2026-08-03）**：CW2017 在位，`cw2017_get_soc()` 返回真实电量。低电/高电分支都能测（取决于电池当前电量）。

**处理**：问题消除。测试时记录电池电量，对应测低电或高电分支。要测特定 SOC 可以通过充/放电调整电池电量。

### #4 LED 测试自动化（**已决策：人眼观察**）

**用户决策（2026-08-03）**：pytest 脚本触发场景后暂停，测试者人眼观察 LED 颜色 + 状态，手动填 PASS/FAIL。

**实现**：D 类用例在 pytest 里用 `input()` 等待人工确认，不阻塞其他类的自动化。

### #6 AT_Protocol.pdf 未读取（**已决策：跳过**）

**用户决策（2026-08-03）**：跳过 PDF 重读，相信代码（at_types.h / at_frame.c）+ REQUIREMENT_TRACEABILITY + dual_pin_timing 的协议定义。代码是事实标准。

### #5 5V/泄放时序 B01/B02（**已消除，ET3328 在位**）

**前因**：之前误以为核心板 ET3328 不在位。

**澄清（2026-08-03）**：ET3328 在位，5V/泄放时序可测（需示波器测 ET3328 输出波形）。

**处理**：问题消除。B01/B02 改为"核心板可测（需示波器）"。

### #6 AT_Protocol.pdf 未读取（**已决策：跳过**）

**用户决策（2026-08-03）**：跳过 PDF 重读，相信代码（at_types.h / at_frame.c）+ REQUIREMENT_TRACEABILITY + dual_pin_timing 的协议定义。代码是事实标准。
