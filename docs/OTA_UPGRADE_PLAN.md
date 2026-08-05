# OTA 升级方案

GD32E230 充电盒固件 OTA 升级设计文档。基于 `docs/spec/AT_Communication_Protocol.pdf` v1.2、`dual_pin_timing_v0.1.xlsx` §盒子ota流程、`docs/REQUIREMENT_TRACEABILITY.md` 与现有 `firmware/src/app/ota_flow.c` 审计结果。

## 1. 决策摘要

| 项 | 决策 | 说明 |
|----|------|------|
| 升级对象 | **盒子** | 眼镜持有盒子固件（云端拉取），盒子通过 POGO 拉取烧自己 |
| 触发主导 | 盒子主动申请 | 协议 step 1 "盒子申请进入ota" |
| 镜像布局 | **选项 B — Dual-bank 轮换** | 掉电安全，运行 bank 完好时写备份 bank |
| 固件校验 | **预留接口不实现** | 协议未定义校验字段，留 `ota_verify()` 空函数，待协议扩展后填 |
| Flash 页大小 | **1 KB** | GD32E230xx FMC page erase 粒度，64 page / 64 KB |
| 数据流方向 | 眼镜 → 盒子 | PREPARE/READ RSP 由眼镜发回 data |

## 2. 升级方向（协议确认）

依据 `dual_pin_timing_v0.1.xlsx` §盒子ota流程 R2 单元格原文：

> 1. 盒子申请进入ota，眼镜同意；
> 2. 盒子发prepare指令，眼镜回复size；
> 3. 盒子发read指令，包含包大小/index，眼镜端回复data/index/type；
> 4. 直到烧录完成，盒子发心跳包清ota标志位（bit7），退出ota模式；

数据流由 step 2/3 明确：眼镜回复 size / data → **数据方向 眼镜 → 盒子**，盒子烧自己。

物理合理性：眼镜有蓝牙/联网能力，从云端取盒子固件后存于眼镜本地；盒子无联网，靠 POGO 接触时从眼镜拉取。`at_glass_data.case_version`（注释"眼镜中盒子固件版本"）即眼镜向盒子报告"我持有的盒子固件版本"，盒子据此对比本地版本决定是否申请 OTA。

## 3. Flash 布局（Dual-bank）

GD32E230C8T6：64 KB flash @ `0x08000000`，page = 1 KB，共 64 page。当前固件 ~24 KB。

```
0x08000000 ┌───────────────────────┐
           │  Bank0  (pages 0-30)  │  31 KB  — 活跃 / 备份
           │  固件镜像 0            │
0x08007C00 ├───────────────────────┤
           │  Bank1  (pages 31-61) │  31 KB  — 备份 / 活跃
           │  固件镜像 1            │
0x0800F800 ├───────────────────────┤
           │  Meta   (page 62)     │  1 KB   — boot selector
0x0800FC00 ├───────────────────────┤
           │  Meta2  (page 63)     │  1 KB   — boot selector 备份
0x08010000 └───────────────────────┘
```

**每 bank 31 KB 上限** → 固件（含 HIL_TEST 调试代码）必须 ≤ 31 KB，当前 ~24 KB，余 7 KB。量产前裁掉 HIL_TEST / update_mode 后可降到 ~18 KB。

**Meta 双 page 轮换**（page 62/63）防 meta 自身写坏：每次切换写另一个 meta page（seq + 1），启动选 seq 大且 CRC 合法的。两个 page 互为备份，擦一个写另一个，掉电安全。

## 4. Boot Meta 结构

```c
/* hal_bootmeta.h */
#define BOOT_META_MAGIC   0x4F544131U  /* "OTA1" */
#define BOOT_META_ADDR_0  0x0800F800U  /* page 62 */
#define BOOT_META_ADDR_1  0x0800FC00U  /* page 63 */

typedef struct {
    uint32_t magic;         /* BOOT_META_MAGIC */
    uint32_t active_bank;   /* 0 = Bank0, 1 = Bank1 */
    uint32_t seq;           /* monotonically increasing */
    uint32_t fw_size;       /* active bank 固件字节大小 */
    uint32_t crc32;         /* 本结构 CRC32（除 crc32 字段） */
} boot_meta_t;
```

启动流程（`system_gd32e23x.c` / 启动钩子）：
1. 读 page 62 + page 63 的 meta
2. 选 `magic` 合法 + `crc32` 正确 + `seq` 最大的
3. 按 `active_bank` 设 `SCB->VTOR = 0x08000000` 或 `0x08007C00`
4. 继续 Reset_Handler

## 5. 升级流程

### 5.1 协议层（已实现，`ota_flow.c`）

| Step | 协议 | 代码 | 状态 |
|------|------|------|------|
| 1 | 盒子心跳 case_sta bit7=1，等眼镜 glass_sta bit7=1 | `ota_request()` | ✓ |
| 2 | 发 0x3003 PREPARE，收 `at_case_packet_prepare.size` | `ota_prepare()` | ✓ |
| 3 | 循环发 0x3004 READ(index)，收 data 直到 type=END | `ota_read_block()` | ✓ |
| 4 | 心跳清 case_sta bit7，退出 | `ota_finish()` | ✓ |

### 5.2 烧录层（待实现）

`ota_run()` 改造：

```
1. ota_prepare(&fw_size)                      # 收固件大小
2. target_bank = 1 - current_active_bank      # 写非活跃 bank
3. base_addr = (target_bank == 0) ? 0x08000000 : 0x08007C00
4. hal_flash_unlock()
5. for page in [base_addr .. base_addr + fw_size aligned to 1KB]:
       hal_flash_page_erase(page)             # 擦非活跃 bank 的 page
       hal_wwdgt_feed()                        # 防看门狗复位
6. offset = 0
   while ota_read_block(index, block, &dlen, &type):
       hal_flash_write(base_addr + offset, block, dlen)
       offset += dlen
       hal_wwdgt_feed()
       if type == END: break
7. ota_verify(base_addr, offset)              # 预留：当前空函数直接 return true
8. hal_bootmeta_update(target_bank, offset)   # 切 active_bank + seq++，写另一 meta page
9. hal_flash_lock()
10. ota_finish(ctx)                           # 协议层清 OTA flag
11. NVIC_SystemReset()                        # 重启，启动代码选新 bank
```

**关键约束**：
- 步骤 5/6 写非活跃 bank 的 page，**不擦写当前运行 bank**，运行固件不被破坏 → 掉电后旧 bank 仍可启动
- 擦写循环必须 `hal_wwdgt_feed()`（FMC page erase 3.2 ms / word program 42 µs，看门狗 20 ms 窗口）
- 擦写期间建议 `__disable_irq()` 包住单次 erase/program 操作（FMC 操作期间 flash 总线忙，取指会 stall）

## 6. 校验策略

协议（AT_Communication_Protocol.pdf + dual_pin_timing）**未定义固件校验字段**。

**决策：预留接口不实现。**

```c
/* ota_flow.c */
static bool ota_verify(uint32_t addr, uint32_t size) {
    /* TODO: 协议扩展后填。候选方案：
     *   (1) END 包 data[] 尾 4 字节 = 全固件 CRC32
     *   (2) 新增 opcode 0x3005 VERIFY
     *   (3) 扩展 at_case_packet_prepare 加 crc32 字段
     * 当前直接返回 true，跳过校验。 */
    (void)addr; (void)size;
    return true;
}
```

风险：烧坏的固件也会被启动代码加载。缓解：meta 的 `crc32`（meta 结构自身校验）保证至少**选对 bank**；固件本身损坏需启动后应用层自检（如版本字符串校验）兜底，本方案不覆盖。

## 7. 触发逻辑

### 7.1 现状缺陷
`ota_requested` 全代码无 `= true` 写入点（grep 仅见清零），状态机永不进 ST_OTA。

### 7.2 修复（版本对比触发）

```c
/* firmware/src/app/fw_version.h（新增）*/
#define CASE_FW_VERSION  0x01U   /* 每次发版递增 */

/* ota_flow.c ota_heartbeat() 解析响应处补：*/
ctx->reported_case_version = rsp->case_version;

/* state_machine.c sm_tick_charging() / sm_tick_maintaining() 头部：*/
if (ctx->reported_case_version != CASE_FW_VERSION) {
    ctx->ota_requested = true;
}
```

### 7.3 HIL 测试触发捷径

`update_mode.c` 加 `OTA` 命令直接 `ctx->ota_requested = true`，绕过版本对比，用于 HIL 链路验证。

## 8. 实现计划（分阶段）

### 阶段 1 — Dual-bank 烧录 MVP
工作量 ~3-4 天

- 新增 `hal_flash.c/h`：`hal_flash_unlock/lock`、`hal_flash_page_erase`、`hal_flash_write`（包装 SPL `fmc_*`，含喂狗 + 关中断）
- 新增 `hal_bootmeta.c/h`：`hal_bootmeta_read`（选 seq 大且 CRC 合法）、`hal_bootmeta_update`（写另一 meta page）
- 改 `system_gd32e23x.c`：启动钩子读 meta 设 `SCB->VTOR`
- 改 `ota_flow.c ota_run()`：实现 §5.2 烧录流程
- 改 `startup/gd32e230x8_flash.ld`：符号化 Bank0/Bank1/Meta 区域（不强制分段，仅 `_bank0_start` 等符号供 C 引用）
- 新增 `fw_version.h`，改 `ota_heartbeat` 读 `case_version`
- 改 `update_mode.c`：加 `OTA` 命令
- 改 `state_machine.c`：版本对比触发

### 阶段 2 — HIL 测试
工作量 ~1-2 天

- 改 `sgc_at.py`：`pack_prepare_response(size)`、`pack_read_response(index, data, type)`、`pack_ota_flow(fw_bytes)` 自动分块
- 新增 `test_i_ota.py`：
  - I01 OTA 触发（OTA 命令 → 收到 prepare 请求）
  - I02 完整烧录（喂固件 → 等盒子重启 → 重连）
  - I03 银行切换（重启后 active_bank 翻转，通过串口输出确认）

### 阶段 3 — 协议扩展（待眼镜端协调）
- 跟眼镜端对齐校验方案（§6 候选三选一）
- 实现 `ota_verify()`
- 量产前必须完成

## 9. 文件改动清单

```
新增:
  firmware/src/hal/hal_flash.c
  firmware/src/hal/hal_flash.h
  firmware/src/hal/hal_bootmeta.c
  firmware/src/hal/hal_bootmeta.h
  firmware/src/app/fw_version.h
  firmware/tests/hil/test_i_ota.py

改:
  firmware/src/app/ota_flow.c          ota_run 烧录流程 + ota_verify 预留 + 读 case_version
  firmware/src/app/state_machine.c     版本对比触发 ota_requested
  firmware/src/app/state_machine.h     sm_ctx_t 加 reported_case_version
  firmware/src/app/update_mode.c       加 OTA 命令（HIL 触发捷径）
  firmware/src/main.c                  board_init 调 hal_bootmeta_init
  firmware/Drivers/CMSIS/system_gd32e23x.c  启动设 SCB->VTOR
  firmware/tests/hil/sgc_at.py         OTA 响应模拟
```

## 10. 风险与待办

| # | 风险 | 缓解 |
|---|------|------|
| 1 | 擦写非活跃 bank 时 flash 总线 stall 影响 ISR | 单次 erase/program 包 `__disable_irq()`，操作完恢复；期间看门狗喂狗 |
| 2 | Meta page 自身写坏 | 双 page 轮换 + seq + CRC，选合法的最大 seq |
| 3 | 无固件校验，烧坏固件被加载 | 阶段 3 协议扩展后补；当前靠 meta CRC 保证选对 bank |
| 4 | 固件超 31 KB 上限 | 编译后检查 `.hex` size；量产裁 HIL_TEST |
| 5 | 启动代码读 meta 复杂化 Reset_Handler | 在 `system_init` 早期（`__main` 之前）读 meta 设 VTOR，Reset_Handler 不动 |
| 6 | GD32E230 page size 实际值 | `hal_flash.c` 用 `#define FMC_PAGE_SIZE 0x400U`，烧录前 grep `gd32e23x.h` CMSIS 确认 |
| 7 | 同步阻塞期间 LED/按键停 | OTA 已是"维护入盒"状态，用户体验影响可接受 |
| 8 | 协议触发逻辑（版本对比）协议未明文 | 当前实现是推断（case_version 字段语义），需眼镜端确认 |

## 11. 待用户/眼镜端协调

1. **固件版本号协议**：`case_version` 1 字节语义（递增？BCD？），需眼镜端开发确认
2. **校验字段**：阶段 3 选哪个方案（END 包尾 / 新 opcode / 扩展 prepare）
3. **眼镜端固件存储**：眼镜能存多大盒子固件？（31 KB），眼镜端 push 实现进度
