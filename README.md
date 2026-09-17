# AM5 Native Memory Lab

[![Build and Release](https://github.com/aksjfds/AM5-Native-Memory-Lab/actions/workflows/build-release.yml/badge.svg)](https://github.com/aksjfds/AM5-Native-Memory-Lab/actions/workflows/build-release.yml)

AM5 Native Memory Lab 是一个面向 Windows x64 / AMD AM5 平台的原生内存微基准与 **Copy 参数组瓶颈分析器**。

当前版本：**2.2.2**  
引擎：`AM5-Native-2.2.2-COPY-GROUPS-AVX2`  
结果 Schema：`AM5Native/4`  
诊断范围：`parameter_group_only`

项目的目标不是寻找“唯一最差的某个 tXXX”，而是回答一个更可验证的问题：

> 在当前内存配置下，哪些内存访问路径最明显限制本项目的 Copy workload，并把能够形成可靠证据的结果定位到参数组。

## 当前能做什么

程序目前已经具备完整的 Copy 专项测量框架：

- Copy-NT / Cached Copy 基线；
- 独立 Read / Write 参考；
- Copy 线程扩展；
- 64B → 64KiB 的 10 档 Read/Write turnaround sweep；
- 同 trial 配对统计、MAD 噪声门槛和 turnaround 回归；
- Copy / Read / Mixed 背景下的 loaded latency；
- Copy 背景下的短窗口 P50 / P99 / P99.9 / Max；
- Bank/Row 代理测试；
- 完整缓冲区数据校验和 WHEA 记录；
- Linux / Windows 双平台 CI、自检、ASan/UBSan 和 JSON/CSV/HTML 一致性校验。

目前报告只对**参数组**给出 `priority`、`watch`、`no_evidence`、`unresolved` 等状态。具体时序值可以作为快照展示，但不会在组内进行排名。

## 当前参数组

| 参数组 | 当前状态 |
| --- | --- |
| 读写转向参数组 | 已有可识别微基准，可进入组级排名 |
| 写入数据路径参数组 | 已有配对压力分析，可进入组级排名 |
| 读取数据路径参数组 | 已有配对压力分析，可进入组级排名 |
| Bank 激活/并行参数组 | 目前只有代理证据，等待真实 Bank/BankGroup 映射 |
| Row 周期参数组 | 目前只有代理证据，等待真实 Bank/Row 映射 |
| 刷新/尾延迟参数组 | 目前只有尾延迟证据，等待 Refresh/UMC 事件证据 |
| 全局数据路径 / IMC 排队 | 作为上下文使用，不直接伪装成可操作时序结论 |

## 当前还没有实现什么

当前仍有四个关键能力没有完成：

1. **实际参数读取**：还没有像 ZenTimings 一样直接读取实际 MCLK/UCLK/FCLK、UMC timings、GDM/Nitro/Refresh Mode、电压等；目前 `profile.ini` 只是临时快照。
2. **统一的 Copy 边际损失指标**：现有各诊断的 `score` 是证据强度/优先级分数，不是统一量纲的“该参数组吃掉了多少 Copy 性能”。
3. **物理地址 → Channel / BankGroup / Bank / Row 映射**：没有这一层时，Bank 和 Row 参数组不能形成高置信度正交实验。
4. **Refresh / UMC 硬件事件证据**：没有 UMC/Refresh 事件计数时，尾延迟不能被直接归因给刷新参数组。

因此当前版本应该被理解为：**已经具备可靠测量底座和部分参数组筛查能力，但还不是最终完整的参数组瓶颈定位器。**

## 下一阶段

下一阶段不会尝试恢复“单参数定位”，而是继续强化组级诊断：

1. 建立统一的 **Marginal Copy Loss / Marginal Recoverable Performance** 指标，让不同参数组可以在同一量纲下比较；
2. 实现并验证物理地址到 Bank / BankGroup / Row 的映射，构造 same-row、same-bank-different-row、cross-bank、cross-BG 正交微基准；
3. 接入 AMD UMC/DF 计数能力，优先获取 ACTIVATE、PRECHARGE、MemClk、读写流量及可用的 Refresh 相关事件；
4. 再接入类似 ZenTimings 的真实参数读取层，把实际参数归入对应参数组；
5. 最终输出“参数组 → 实测边际 Copy 损失 → 置信度 → 证据来源”，允许证据不足时保持 `unresolved`。

详细进度见 [TODO.md](TODO.md)，下一阶段技术方案见 [modification.md](modification.md)。

## 使用

在 Releases 下载 `AM5MemoryLab-Windows-x64.zip`，完整解压后运行：

- `START.cmd`：完整测试；
- `QUICK.cmd`：较短测试；
- `SELFTEST.cmd`：只执行内部自检。

运行要求：Windows x64 + AVX2。程序不修改 BIOS、电压、Secure Boot、Defender 或其他系统安全设置，也不会上传测试数据。

## 输出

每次完整运行生成：

- `report.html`：Copy 指标、参数组诊断、证据和运行质量；
- `results.json`：结构化原始结果与 diagnostics；
- `raw.csv`：逐样本数据；
- `whea-events.xml`：测试窗口存在 WHEA 时生成。

Copy 带宽按读取+写入两个方向的逻辑字节计数；报告同时显示有效复制速率。项目的 Copy workload 不等价于 AIDA64 Copy，也不声称复刻 AIDA64 的内部实现。

## 构建与质量门槛

```sh
python3 source/build_builds.py
source/build/am5lab-linux --self-test
```

当前 CI 包括：

- Linux 与 Windows x64 编译；
- 编译警告视为错误；
- Linux ASan + UBSan；
- Linux / Windows 65 项 self-test；
- Linux / Windows smoke workflow；
- JSON / CSV / HTML 一致性校验；
- Windows 发布包 SHA-256 校验。

当前项目定位：**AM5 Copy path / parameter-group bottleneck analyzer**。