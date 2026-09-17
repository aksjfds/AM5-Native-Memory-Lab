# AM5 Native Memory Lab

[![Build and Release](https://github.com/aksjfds/AM5-Native-Memory-Lab/actions/workflows/build-release.yml/badge.svg)](https://github.com/aksjfds/AM5-Native-Memory-Lab/actions/workflows/build-release.yml)

Windows x64 原生 **Copy 短板分析器**。当前版本 **2.2.0**，引擎 `AM5-Native-2.2.0-COPY-AVX2`，结果 Schema `AM5Native/4`。

项目现在只聚焦一个目标：

> 在当前内存配置下，通过正交微基准找出最可能限制本项目 Copy workload 的访问路径，并把有证据的路径映射到候选参数组；没有足够证据时明确输出 `no_evidence` 或 `unresolved`，不强行挑时序。

## 下载

在 Releases 下载 `AM5MemoryLab-Windows-x64.zip`，完整解压后运行 `START.cmd`。需要 Windows x64 + AVX2；不需要 Python 或编译器。程序不修改 BIOS、电压或系统安全设置。

## 2.2.0 的 Copy 专项测试

1. **Copy baseline + Read/Write 对照**：测 `Copy-NT`、`Cached Copy`、独立 Read、独立 Write，并对 Copy 做 1/2/4/8/最大线程扩展。
2. **10 档 turnaround sweep**：固定 50/50 总读写字节，只改变连续读/写分组：64B、128B、256B、512B、1KiB、2KiB、4KiB、8KiB、16KiB、64KiB。按 `elapsed/logical_bytes` 对 `events/logical_bytes` 做回归，给出有效 `ns/transition`、R² 和配对 MAD。
3. **Copy 背景 loaded latency**：同一随机依赖读取线程下，比较空载、Read、50/50 Mixed、Copy 背景压力。
4. **Copy-loaded short-window tail**：64 次依赖访问为一个窗口，比较空载和 Copy 背景下 P50/P99/P99.9/Max。
5. **Bank/Row 代理测试**：保留独立链和页内局部性证据，但在没有真实 Bank/Row 映射时只标记 `unresolved`，不进入参数排名。

## Copy 短板排名

报告只把达到阈值的项目列入 `priority` / `watch` 排名。当前可进入排名的主要证据包括：

- **Read/Write turnaround** → `tRDWR / tWRRD / tWTRS / tWTRL / IMC turnaround`
- **Write-side pressure** → `tWRWRSCL / tWRWRSC-SD-DD / tCWL / write-side IMC`
- **Read-side pressure** → `tRDRDSCL / tRDRDSC-SD-DD / read-side IMC`

线程饱和、Cached/NT 差异等只作为上下文，不会被伪装成 BIOS 时序短板。

`score` 是“证据/优先级分数”，不是声称可直接获得的性能百分比；`effect_pct` 是对应诊断中实际观测到的路径效应。一次固定 BIOS 配置不能证明某个具体时序还能安全降低 1 cycle，最终单项因果仍需 A/B。

## 当前参数来源仍是占位

**2.2.0 还没有实现类似 ZenTimings 的硬件实读。** `profile.ini` 只作为临时参数快照展示，诊断评分完全来自实测，不读取 `profile.ini` 数值来决定谁是短板。

后续硬件读取层应提供实际 MCLK/UCLK/FCLK、UMC timings、GDM/Nitro/Refresh Mode、PMIC/相关电压，并在 JSON 中明确标记来源为硬件实读。

## 仍未解决的硬件层

以下能力缺失时，程序不会对相应具体参数做高置信度单项归因：

- 物理地址 → Channel / BankGroup / Bank / Row 映射；
- AMD UMC / DF / IBS 计数器；
- Refresh 事件计数器；
- 实际 BIOS/UMC 时序和电压读回。

因此 `tRRDS/tRRDL/tFAW`、`tRP/tRCDRD/tRC`、`tRFC/tREFI` 目前仍以 `unresolved` 形式保留证据，不参与可操作参数排名。

## 输出

每次运行生成：

- `report.html`：Copy 指标、短板排名、参数快照、测试证据；
- `results.json`：Schema 4 原始数据与结构化 diagnostics；
- `raw.csv`：逐样本数据，包括 `pattern_bytes`、`events`、P99.9、Max；
- `whea-events.xml`：仅测试窗口检测到 WHEA 时生成。

Copy 的 `GB/s` 默认按读取+写入两个方向的逻辑字节计数；报告同时显示“有效复制 GB/s = Copy 总逻辑 GB/s / 2”。这不是 AIDA64 Copy 的复刻分数。

## CI / Release

`.github/workflows/build-release.yml` 会：

- Ubuntu 24.04 + Clang/LLD 构建 Linux 验证程序与 Windows x64 EXE；
- Linux 和 Windows runner 都执行 **60 项 self-test** 和 smoke workflow；
- 校验 Schema 4、10 档 turnaround、Copy loaded-latency、Copy-loaded short-window probe、CSV/JSON/HTML 一致性；
- 两端都通过后发布正式 GitHub Release（非 Pre-release），附件为 ZIP、`BUILD_INFO.txt`、`SHA256SUMS.txt`。

## 本地构建

```sh
python3 source/build_builds.py
source/build/am5lab-linux --self-test
```

当前项目的正确产品定位是：**Copy path / parameter-group bottleneck analyzer**，不是“单次运行自动算出唯一最佳 BIOS 时序值”。
