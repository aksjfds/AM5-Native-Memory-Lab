# AM5 Native Memory Lab 2.2.2 — Copy 参数组短板分析器

这是一个 Windows x64 原生内存微基准，目标是：**找出当前配置下最可能限制 Copy 性能的访问路径，并只定位到参数组。**

程序不再尝试判断“组内哪一个具体 tXXX 最差”。具体参数值可以作为快照展示，但诊断结论只输出参数组。

## 使用

解压 Release 中的 `AM5MemoryLab-Windows-x64.zip`，运行：

- `START.cmd`：完整测试；
- `QUICK.cmd`：较短测试；
- `SELFTEST.cmd`：只执行 65 项内核/计算自检。

程序不会修改 BIOS、电压、Secure Boot、Defender 或其他系统设置。

## 参数读取状态

当前仍使用 `profile.ini` 作为临时参数快照。它不是 BIOS/UMC 实时读回，且**不参与参数组评分**。后续会单独实现类似 ZenTimings 的实际参数读取层。

读取到的具体参数只需要归入对应参数组，不再要求诊断器继续做组内单参数归因。

## 诊断范围

`results.json` 会明确输出：

`diagnostic_scope: "parameter_group_only"`

目前组级诊断包括：

- **读写转向参数组**；
- **写入数据路径参数组**；
- **读取数据路径参数组**；
- **Bank 激活/并行参数组**；
- **Row 周期参数组**；
- **刷新/尾延迟参数组**；
- 全局数据路径、IMC 排队和 CPU store policy 等上下文组。

具体时序名只用于说明一个组由哪些设置构成，不表示程序已经区分了组内成员。

## 诊断逻辑

程序首先测 Copy-NT、Cached Copy、独立 Read、独立 Write，并对 Copy 做线程扩展；然后执行 10 档读写方向切换扫描、Copy 背景 loaded latency、Copy 背景短窗口 tail probe，以及 Bank/Row 代理测试。

只有具备可识别证据的参数组才进入 Copy 短板排名。2.2.1 起保留的质量规则仍然有效：Read/Write/Copy 侧向压力使用同一 trial 的配对统计和 MAD 噪声门槛；若独立 Read/Write 参考与 Copy 数据流明显不自洽，该项会降级为 `unresolved`。

没有 Bank/Row 映射时，Bank 激活组和 Row 周期组保持 `unresolved`；没有 UMC refresh counter 时，刷新/尾延迟组也不会被提高到高置信度。

`priority` / `watch` 表示“在本轮 Copy workload 中值得优先检查的参数组”，不是稳定性结论，也不是组内某个具体时序的结论。

## 数据口径

Copy 总逻辑带宽按 `read bytes + write bytes` 计数；有效复制数据率为总逻辑带宽的一半。Read/Write 独立带宽不是 Copy 的严格物理上限，只用于判断哪个方向更接近自身独立参考边界。

Turnaround sweep 固定总读写量和 50/50 比例，只改变连续读写分组：64B → 64KiB，并用事件密度和单位字节耗时回归评估有效切换成本。

Copy 背景 loaded-latency 和 Copy-loaded tail 阶段会在结束后检查完整目标缓冲区，数据不一致会使整轮诊断失效。

## 输出文件

- `report.html`
- `results.json` (`AM5Native/4`，`diagnostic_scope=parameter_group_only`)
- `raw.csv`
- 可选 `whea-events.xml`

本工具的 Copy workload 不等价于 AIDA64 Copy，也不声称复现 AIDA64 的内部实现。
