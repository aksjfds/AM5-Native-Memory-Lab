# AM5 Native Memory Lab 2.0.0

这是 Windows x64 原生内存实测程序，不是上一版“手填 AIDA64 分数”的网页工具。

## 直接运行

把整个文件夹解压到你的 Windows 台式机上的可写入位置，关闭游戏、AIDA64、TM5 等其他跑分/压力测试，保存正在编辑的文件，然后双击 `START.cmd`。不需要安装 Python、AIDA64、编译器、驱动或管理员权限。

程序先执行 56 项内核/计算自检，再连续运行整套测试，最后自动打开中文报告。默认每种负载测 5 轮，单次计时约 500 ms；不要在测试过程中操作其他大型程序。按 `Ctrl+C` 可以中止，已经完成的样本会保存。

`QUICK.cmd` 是缩短测量时间的同一套测试（3 轮、单次约 180 ms），不适合据小差距判断调参收益。`SELFTEST.cmd` 只做程序内核自检，不是整机稳定性测试。

Windows 可执行文件已经附带，无须自己编译。仅依赖 Windows 自带的系统 DLL，没有另外的运行库安装包。程序未做代码签名；出现安全提示时不要关闭 Defender、SmartScreen 或其他系统安全功能来强行运行。源码和 SHA256 校验清单包含在包内，可用于审查；校验码证明文件是否一致，不证明软件安全或程序不存在缺陷。

## 输出在哪里

每次运行生成一个独立文件夹：

```
results/年月日_时分秒_进程号/
    report.html        中文可视化报告；双击即可重新查看
    results.json       本轮机器信息、配置声明、每个实测样本
    raw.csv            边测边保存的原始数据
    whea-events.xml    仅观察到 WHEA 事件时生成
```

不需要手工录入跑分。每次修改 BIOS 后重新运行 `START.cmd` 即可测量当前配置。

## profile.ini 的用途

只用于报告注释和理论时序换算，不控制测试结果，不更改硬件。

已填写本轮约束：DDR5-8400、UCLK 2100、FCLK 2100、DRAM Voltage 1.350 V、DRAM VDDQ Voltage 1.350 V、DRAM VPP Voltage 1.800 V，其余电压 Auto。时序注释填写的是此前明确报告稳定的 C42 基线，不把后来的 C40 目标冒充已确认实装。

修改 BIOS 后，把本轮时序声明同步到该文件即可得到正确的注释/纳秒换算。不更新文件仍可真实测量当前性能，但报告里的配置注释可能过时。软件没有读取 BIOS、SPD 实际配置或实际电压。

## 确实执行的测试

- AVX2 Read、非临时 Write/Copy、普通缓存 Write/Copy；所有分数来自真实访问字节数与实际耗时。
- 固定总工作集的 1/2/4/8 等线程档位读取，每个线程绑定到不同物理核心的一条逻辑线程。
- 75:25、50:50、25:75 读写比例；64B 交替读写与 64KiB 分组读写。
- 超过缓存范围的随机依赖指针链、页内局部性链、顺序链，以及 2/4/8 条独立访问链。
- 一条依赖访问线程与其他核心上的独立后台流量并行，实测带宽—延迟曲线。
- L1/L2/L3 容量范围内的单线程 Read/Write/Copy/Latency。没有缓存命中计数器，因此不是“严格隔离某一级缓存”。
- 地址相关数据模式、读校验与复制/写入结果回读；查询运行时段内 System 日志的 WHEA-Logger 事件。日志无法访问时明确记作“未获取”，不会当作零错误。

## 如何理解性能单位

GB/s 使用十进制 10^9 字节/秒。Copy 的“读＋写”吞吐计算为 `2 × 复制字节数 ÷ 耗时`；报告另外显示仅计算复制有效数据的结果。普通缓存写会产生未被本程序计数的缓存/RFO/写回流量，所以逻辑 GB/s 不是实测物理 DRAM 总线流量。

单链延迟用真实依赖读取计时，不用 tCL 等公式生成。多链的 ns/access 是多个请求并行后的摊销时间，不能当成单次访问延迟。窗口 P95/P99 是“一批 8192 次访问的平均值”的分位数，不是单请求尾延迟，更不是刷新时长。

缓存带宽是单线程，不与 AIDA64 的多核聚合缓存带宽直接等同。Memory 的方法、指令、线程和字节口径也可能与 AIDA64 不同。

## 主动定位能到哪一步

报告使用本次运行的不同负载，量化并行访问、局部性、读写分组和负载增加的实际影响；不依赖历史参数差分。

报告中的相关 BIOS 参数只列为候选组，不自动把组级结果归因给某一个时序。没有改变当前 BIOS 设置，不能知道一个尚未试过的 tRFC、tCL 或 Nitro 值是否稳定、会提升多少。程序不生成“必稳终极值”。

本版没有集成 AMD IBS/UMC 计数器、物理地址到 Bank/行映射、温度传感器、实际电压/时序读取，也不下载或加载低层驱动。因此不会输出伪造的刷新占用、Bank 冲突计数、某时序贡献率或传感器读数。

## 正确性与超频稳定性

检查通过仅说明本次有限的访问和回读未发现错误，不代替完整内存稳定性测试。程序不覆盖全部 24GB 物理内存，不测试冷启动训练或长时间高温。WHEA 事件可能来自 CPU、PCIe 或其他设备，不能自动归因内存。

程序不会提高电压、修改 BIOS、修改注册表、自动重启、关闭安全功能、写入控制器寄存器或联网上传数据。它只读写自己申请的内存及本程序目录下的结果文件，配置文件为只读输入。

## 验证范围

Windows x64 EXE 已通过交叉编译和 PE/导入依赖检查。共享测试内核在当前 Linux x86-64 环境进行了自检、完整实测流程、数值复核和页面测试。具体完成项目见 `VALIDATION.md`。

尚未在你的 9700X + B850 MPOWER 的 Windows 环境实际运行；不能把开发环境测试当成你的硬件测试或 Windows 兼容性保证。程序是本次生成的首版可运行实现，不是经过大量用户验证的成熟替代品。

## 高级启动参数

直接执行 `AM5MemoryLab.exe --help` 查看。一般无需修改。`--smoke` 是开发者功能测试，缩小工作集，报告会禁止把它作为 DRAM 诊断依据。

## 技术参考

- Microsoft 高分辨率时间戳：https://learn.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps
- Microsoft 处理器拓扑：https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getlogicalprocessorinformationex
- Microsoft VirtualAlloc：https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
- Microsoft WHEA 事件查询：https://learn.microsoft.com/en-us/windows-hardware/drivers/whea/querying-the-system-event-log-for-hardware-error-events
- STREAM 工作集与字节口径：https://www.cs.virginia.edu/stream/ref.html
- 实测带宽—延迟关系的研究：https://arxiv.org/abs/2405.10170

本程序独立编写，不是上述项目的官方版本，不声称复现 AIDA64、STREAM、Mess 的分数或模拟器结果。
