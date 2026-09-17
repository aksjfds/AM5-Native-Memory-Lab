# AM5 Native Memory Lab

[![Build and Release](https://github.com/aksjfds/AM5-Native-Memory-Lab/actions/workflows/build-release.yml/badge.svg)](https://github.com/aksjfds/AM5-Native-Memory-Lab/actions/workflows/build-release.yml)

Windows x64 原生 **Copy 短板分析器**。当前版本 **2.2.2**，引擎 `AM5-Native-2.2.2-COPY-GROUPS-AVX2`，结果 Schema `AM5Native/4`。

项目只聚焦一个目标：

> 在当前内存配置下，通过正交微基准找出最可能限制本项目 Copy workload 的访问路径，并把有证据的路径定位到**参数组**。项目不再尝试在组内判断某一个具体时序是谁最差或最值得改。

`results.json` 会明确写入 `diagnostic_scope: "parameter_group_only"`。

## 下载

在 Releases 下载 `AM5MemoryLab-Windows-x64.zip`，完整解压后运行 `START.cmd`。需要 Windows x64 + AVX2；不需要 Python 或编译器。程序不修改 BIOS、电压或系统安全设置。

## 2.2.2：只定位参数组

2.2.2 保留 2.2.1 的质量门槛和测量方法，但收紧诊断语义：

- `priority` / `watch` 的对象只允许是参数组；
- 不再把排名结果表达成“某个具体 tXXX 是短板”；
- `profile.ini` 或未来硬件实读得到的具体参数值可以继续作为运行快照展示，但不会参与组内排序；
- 具体参数名只用于说明某个组包含哪些设置，不代表程序已经区分了组内成员；
- 如果现有微基准只能证明某条路径异常、但不足以确定参数组，仍然输出 `unresolved`。

当前组级标签包括：

- **读写转向参数组**：读写方向切换相关时序与 IMC turnaround 设置；
- **写入数据路径参数组**：连续写、写侧 SCL/turnaround 等相关设置；
- **读取数据路径参数组**：连续读、读侧 SCL/turnaround 等相关设置；
- **Bank 激活/并行参数组**：需要真实 Bank 映射后才能提高置信度；
- **Row 周期参数组**：需要真实 Bank/Row 映射后才能提高置信度；
- **刷新/尾延迟参数组**：需要 UMC/Refresh 事件计数器才能提高置信度；
- **全局数据路径 / IMC 排队组**：目前主要作为上下文，不直接当作可操作时序组排名。

## 2.2.1 质量加固保留

2.2.1 引入的质量措施继续保留：

- Read/Write/Copy 侧向压力使用**同 trial 配对统计**和 MAD 噪声门槛；
- 如果 Copy 每方向吞吐超过对应独立 Read/Write 参考的 120%，相关诊断自动降级为 `unresolved`；
- 所有需要多核背景负载的诊断都先检查样本是否存在；
- Copy 背景负载和 Copy-loaded tail probe 结束后执行完整目标缓冲区校验；
- worker/core 范围、job 数组和 `raw.csv` 写入都有显式错误检查；
- `profile.ini` 超过 64 KiB 或读取失败时不作为有效快照；
- 编译启用 `-Werror`；Linux CI 运行 AddressSanitizer + UndefinedBehaviorSanitizer；
- CI 严格交叉校验 JSON/CSV 的线程、事件、字节、时间、数值、affinity 和重复次数；
- self-test 共 **65 项**。

## Copy 专项测试

1. **Copy baseline + Read/Write 对照**：测 `Copy-NT`、`Cached Copy`、独立 Read、独立 Write，并对 Copy 做 1/2/4/8/最大线程扩展。
2. **10 档 turnaround sweep**：固定 50/50 总读写字节，只改变连续读/写分组：64B、128B、256B、512B、1KiB、2KiB、4KiB、8KiB、16KiB、64KiB。按 `elapsed/logical_bytes` 对 `events/logical_bytes` 做回归，给出有效 `ns/transition`、R² 和配对 MAD。
3. **Copy 背景 loaded latency**：同一随机依赖读取线程下，比较空载、Read、50/50 Mixed、Copy 背景压力。
4. **Copy-loaded short-window tail**：64 次依赖访问为一个窗口，比较空载和 Copy 背景下 P50/P99/P99.9/Max。
5. **Bank/Row 代理测试**：保留独立链和页内局部性证据，但在没有真实 Bank/Row 映射时只标记 `unresolved`。

## Copy 参数组排名

报告只把达到阈值的参数组列入 `priority` / `watch`。线程饱和、Cached/NT 差异等只作为上下文，不会被伪装成 BIOS 时序短板。

`score` 是证据/优先级分数，不是声称可直接获得的性能百分比；`effect_pct` 是对应诊断中实际观测到的路径效应。一次固定 BIOS 配置不能证明组内某个具体时序还能安全降低 1 cycle，因此本项目不再做这种单参数结论。

## 当前参数来源仍是占位

**2.2.2 还没有实现类似 ZenTimings 的硬件实读。** `profile.ini` 只作为临时参数快照展示，诊断评分完全来自实测，不读取 `profile.ini` 数值来决定哪个参数组是短板。

后续硬件读取层应提供实际 MCLK/UCLK/FCLK、UMC timings、GDM/Nitro/Refresh Mode、PMIC/相关电压，并在 JSON 中明确标记来源为硬件实读。读取到的参数随后只需归入对应组，不要求诊断器继续做组内单参数归因。

## 仍未解决的硬件层

以下能力缺失时，相应组只能保持较低置信度或 `unresolved`：

- 物理地址 → Channel / BankGroup / Bank / Row 映射；
- AMD UMC / DF / IBS 计数器；
- Refresh 事件计数器；
- 实际 BIOS/UMC 时序和电压读回。

## 输出

每次运行生成：

- `report.html`：Copy 指标、参数组短板排名、参数快照、测试证据；
- `results.json`：Schema 4 原始数据与结构化 diagnostics，并标明 `parameter_group_only`；
- `raw.csv`：逐样本数据，包括 `pattern_bytes`、`events`、P99.9、Max；
- `whea-events.xml`：仅测试窗口检测到 WHEA 时生成。

Copy 的 `GB/s` 默认按读取+写入两个方向的逻辑字节计数；报告同时显示“有效复制 GB/s = Copy 总逻辑 GB/s / 2”。这不是 AIDA64 Copy 的复刻分数。

## CI / Release

`.github/workflows/build-release.yml` 会：

- Ubuntu 24.04 + Clang/LLD 构建 Linux 验证程序与 Windows x64 EXE；
- 编译器警告按错误处理；
- Linux 额外执行 ASan + UBSan self-test / smoke；
- Linux 和 Windows runner 都执行 **65 项 self-test** 和普通 smoke workflow；
- 校验 Schema 4、`parameter_group_only`、10 档 turnaround、Copy loaded-latency、Copy-loaded short-window probe、CSV/JSON/HTML 一致性；
- 两端都通过后发布正式 GitHub Release（非 Pre-release），附件为 ZIP、`BUILD_INFO.txt`、`SHA256SUMS.txt`。

## 本地构建

```sh
python3 source/build_builds.py
source/build/am5lab-linux --self-test
```

当前项目的正确产品定位是：**Copy path / parameter-group bottleneck analyzer**，只定位参数组，不输出“唯一最差单参数”。
