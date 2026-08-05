# HIL 测试（硬件在环）

通过 USART0 (PA9/PA10) 跟 GD32E230 充电盒固件双向 AT 帧交互，验证固件实现是否满足需求。

## 接线

| 核心板 | USB-TTL 模块 |
|--------|-------------|
| PA9 (USART0 TX) | RXD |
| PA10 (USART0 RX) | TXD |
| GND | GND |

电平 3.3V，参数 `115200 8N1 无校验`。

## 依赖

```bash
pip install pyserial pytest
```

## 配置串口号

默认 `COM3`。用环境变量改：

```bash
# Windows bash
SGC_SERIAL_PORT=COM5 python -m pytest firmware/tests/hil/ -v

# 或 PowerShell
$env:SGC_SERIAL_PORT="COM5"; python -m pytest firmware/tests/hil/ -v
```

## 跑测试

```bash
# 全部 HIL 测试
python -m pytest firmware/tests/hil/ -v

# 只跑 A 类（AT 协议帧格式）
python -m pytest firmware/tests/hil/test_a_protocol.py -v

# 跑单个用例
python -m pytest firmware/tests/hil/test_a_protocol.py::test_a01_request_magic -v
```

## 测试流程

1. 烧录当前 main 分支的 `smart_glasses_charger.hex` 到核心板
2. USB-TTL 接 PA9/PA10/GND
3. 跑 pytest
4. 测试会提示 `请用磁铁靠近然后远离霍尔传感器（PB4）` —— 按提示操作触发开盖事件
5. 脚本接收心跳帧，自动验证 Magic/CRC/Opcode/Payload

## 测试矩阵

完整用例清单见 `docs/HIL_TEST_MATRIX.md`（按分类 A-I，~70 用例，标 P0/P1/P2）。

当前实现：A 类 P0（帧格式 + CRC + 心跳结构）。

后续：B 类（时序）、C 类（状态机）、E/F（按键/霍尔）、G/H（功耗/充电链路）。
