# AM5 Native Memory Lab

[![Build and Release](https://github.com/aksjfds/AM5-Native-Memory-Lab/actions/workflows/build-release.yml/badge.svg)](https://github.com/aksjfds/AM5-Native-Memory-Lab/actions/workflows/build-release.yml)

Windows x64 原生内存实测与路径级参数诊断程序。核心为 C11/AVX2，使用真实内存访问、依赖指针追逐、读写方向切换扫描和短窗口尾延迟探针；离线 HTML 用于展示结果。

当前引擎：**AM5-Native-2.1.0-AVX2**，结果 Schema：**AM5Native/3**。

## 下载与运行

在 [Releases](https://github.com/aksjfds/AM5-Native-Memory-Lab/releases) 下载 `AM5MemoryLab-Windows-x64.zip`，完整解压后运行 `START.cmd`。运行需要支持 AVX2 的 Windows x64 电脑，不需要安装 Python、编译器或额外驱动。`QUICK.cmd` 为快速测试，`SELFTEST.cmd` 仅运行 60 项内核/计算自检。

程序不会修改 BIOS、电压或系统防护。程序未签名；不要为运行它关闭 Defender、SmartScreen 或其他系统安全功能。

## 2.1.0：从固定候选表改为实测路径优先级

2.0.x 的报告虽然有真实性能数据，但参数候选表是固定映射，不能根据本轮结果判断哪条路径真正值得优先处理。2.1.0 新增：

- **Turnaround sweep**：保持 50/50 总读写字节不变，仅把读→写分组大小从 64 B 扫到 64 KiB。每轮记录实际 direction-switch event 数，用 `elapsed/logical_bytes` 对 `events/logical_bytes` 做线性回归，输出有效 `ns/transition`、R² 和重复样本噪声。
- **Mixed-vs-read loaded latency**：同线程数下比较纯读和 50/50 混合读写对随机依赖读取的影响，作为 tWTR/tRDWR/tWRRD / IMC turnaround 的辅助证据。
- **1/2/4/8 independent chains**：计算并发请求重叠效率，用于判断 tRRD/tFAW/SCL 所在的“并行路径”是否存在明显缺口；没有 Bank 映射时不单项定责。
- **Short-window stall probe**：把尾延迟窗口从旧版 8192 次访问缩短为 64 次依赖读取，记录 P50/P95/P99/P99.9/Max。它只证明存在 stall；没有 UMC refresh counter 时不会自动归因给 tRFC/tREFI。
- **Structured diagnostics**：`results.json` 新增 `diagnostics` 数组，包含 `status`、`score`、`parameter_group`、`evidence`、`confidence` 和 `limitation`。没有证据时输出 `no_evidence`，无法隔离时输出 `unresolved`，不再机械列出全部时序。

当前诊断目标是：

> 单次运行定位“瓶颈访问路径 → 候选参数组 → 实测影响 → 置信度”。

它**不会**在没有反事实测试的情况下声称某个单项时序应该改成具体数值。

## 当前仍未解决：实际 BIOS/UMC 参数读回

`profile.ini` 仍只是用户声明/备注，不是 BIOS、SPD 或 UMC 实时读回。它只用于报告显示和理论 ns 换算，**不参与 2.1.0 的路径优先级评分**。

因此本版仍不能证明当前真正生效的 tCL/tRFC/Nitro/GDM/电压与 `profile.ini` 一致。实际硬件参数读回需要独立的内核级硬件访问层，后续实现时应明确区分“硬件实读值”和“用户声明值”。

## 当前仍未解决：Bank/Row 与 UMC 硬件计数器

程序仍没有：

- 虚拟/物理地址 → Channel / BankGroup / Bank / Row 映射；
- AMD IBS / UMC / DF 计数器；
- Refresh 事件计数器；
- 内存温度实时采集；
- BIOS 自动写入或自动重启调参。

因此 `address_locality` 和 `stall_tail` 这两类诊断会明确标记为 `unresolved`，不会进入高置信度参数优先级。精确判断 `tRP`、`tRCDRD`、`tFAW`、`tRFC` 的单项贡献仍需要上述硬件层或真实 A/B。

## 输出

每次运行在 `results/YYYYMMDD_HHMMSS_PID/` 生成：

- `report.html`：离线中文报告和路径级参数优先级；
- `results.json`：机器信息、所有原始样本、`diagnostics`、声明配置；
- `raw.csv`：逐样本原始数据，新增 `pattern_bytes`、`events`、P99.9、Max；
- `whea-events.xml`：仅当测试窗口观察到 WHEA 事件时生成。

Copy 吞吐按读+写逻辑字节计数。Cached Write 的 RFO、写回和协议流量未用 UMC 计数器直接测量，所以逻辑 GB/s 不等于物理 DRAM 总线流量。

## 自动编译与 Release

工作流：`.github/workflows/build-release.yml`，名称 **Build and Release**。

| 触发方式 | 行为 |
|---|---|
| 推送到 `main` | 编译、验证，并发布独立的 `ci-运行编号-尝试编号` 正式 Release，标记为 Latest |
| 向 `main` 提交 Pull Request | 编译和验证，仅保存 Actions artifacts，不发布 Release |
| 推送 `v` 加数字开头的标签，如 `v2.1.0` | 编译、验证，使用该标签发布正式 Release，标记为 Latest |
| 推送含 `-` 的版本标签，如 `v2.1.0-rc.1` | 同样发布正式 Release，不设置 Pre-release 标记 |
| 在 Actions 页面手动 Run workflow | `main` 和版本标签发布正式 Release；其他分支只构建 |

Ubuntu runner 使用 `source/build_builds.py` 构建 Linux 验证程序和 Windows x64 EXE。Linux 与 Windows runner 都执行 60 项自检和端到端 smoke workflow。`scripts/verify_smoke.py` 还会确认 turnaround sweep 六个分组、短窗口 stall probe、Schema 3、新 CSV/JSON 字段以及报告嵌入数据的一致性，不设置云主机性能门槛。

只有 Linux 和 Windows 检查都通过，发布任务才上传：

- `AM5MemoryLab-Windows-x64.zip`
- `BUILD_INFO.txt`
- `SHA256SUMS.txt`

所有符合发布条件的构建都直接发布正式 Release（`--prerelease=false`），并标记为 Latest，不创建 Pre-release。

## 源码与本地构建

- `source/src/kernels.c`：AVX2 流式访问、turnaround sweep、依赖指针链；
- `source/src/bench.c`：负载矩阵、线程同步、短窗口 tail probe、逐样本保存；
- `source/src/diagnostic.c`：本轮差分证据、MAD 噪声、回归拟合、参数组优先级；
- `source/src/report.c` / `source/report/template.html`：Schema 3 序列化和离线报告；
- `source/src/selftest.c`：60 项内核/数学自检；
- `source/build_builds.py` / `source/winshim/`：Linux + Windows x64 交叉构建。

本地 Linux x86-64：

```sh
python3 source/build_builds.py
source/build/am5lab-linux --self-test
```

## 验证边界

CI smoke 只验证程序功能、跨平台启动、内核正确性和输出一致性，不是你的 Ryzen 7 9700X 性能基线，也不证明内存超频稳定。路径级优先级用于决定“下一步该验证哪一组参数”，不是替代 BIOS A/B 或长期稳定性测试。
