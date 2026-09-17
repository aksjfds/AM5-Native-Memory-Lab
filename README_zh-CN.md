# AM5 Native Memory Lab 2.2.0 — Copy 短板分析器

这是一个 Windows x64 原生内存微基准，当前版本将目标收窄为：**找出当前配置下最可能限制 Copy 性能的访问路径和参数组。**

## 使用

解压 Release 中的 `AM5MemoryLab-Windows-x64.zip`，运行：

- `START.cmd`：完整测试；
- `QUICK.cmd`：较短测试；
- `SELFTEST.cmd`：只执行 60 项内核/计算自检。

程序不会修改 BIOS、电压、Secure Boot、Defender 或其他系统设置。

## 参数读取状态

当前仍使用 `profile.ini` 作为临时参数快照。它不是 BIOS/UMC 实时读回，且**不参与短板评分**。后续会单独实现类似 ZenTimings 的实际参数读取层。

## 诊断逻辑

程序首先测 Copy-NT、Cached Copy、独立 Read、独立 Write，并对 Copy 做线程扩展；然后执行 10 档读写方向切换扫描、Copy 背景 loaded latency、Copy 背景短窗口 tail probe，以及 Bank/Row 代理测试。

只有具备可识别证据的路径才进入 Copy 短板排名：

- turnaround → `tRDWR / tWRRD / tWTRS / tWTRL`；
- 写侧压力 → `tWRWRSCL / tWRWRSC-SD-DD / tCWL`；
- 读侧压力 → `tRDRDSCL / tRDRDSC-SD-DD`。

没有 Bank/Row 映射时，`tRRD/tFAW` 和 `tRP/tRCD/tRC` 只保留为 unresolved；没有 UMC refresh counter 时，尾延迟也不会被直接称为 `tRFC/tREFI` 问题。

`priority` / `watch` 表示“在本轮 Copy workload 中值得优先检查”，不是稳定性结论。具体某个时序能否减 1 cycle、实际能提升多少，最终仍要改 BIOS 后做 A/B。

## 数据口径

Copy 总逻辑带宽按 `read bytes + write bytes` 计数；有效复制数据率为总逻辑带宽的一半。Read/Write 独立带宽不是 Copy 的严格物理上限，只用于判断哪个方向更接近自身独立能力边界。

Turnaround sweep 固定总读写量和 50/50 比例，只改变连续读写分组：64B → 64KiB，并用事件密度和单位字节耗时回归评估有效切换成本。

## 输出文件

- `report.html`
- `results.json` (`AM5Native/4`)
- `raw.csv`
- 可选 `whea-events.xml`

本工具的 Copy workload 不等价于 AIDA64 Copy，也不声称复现 AIDA64 的内部实现。
