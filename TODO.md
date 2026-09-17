# TODO

本文档记录 AM5 Native Memory Lab 当前已经完成的能力、尚未完成的能力，以及下一阶段的开发顺序。

当前基线版本：**2.2.2**  
引擎：`AM5-Native-2.2.2-COPY-GROUPS-AVX2`  
诊断范围：`parameter_group_only`

## 已完成

### 1. Copy 专项测量框架

- [x] Copy-NT baseline
- [x] Cached Copy baseline
- [x] 独立 Read reference
- [x] 独立 Write-NT reference
- [x] Copy 1 / 2 / 4 / 8 / 最大线程扩展
- [x] 大工作集主存测试条件检查
- [x] 物理核心 affinity
- [x] 测试顺序随机化
- [x] 每轮逐样本原始数据记录

### 2. Read/Write turnaround 诊断

- [x] 64B / 128B / 256B / 512B / 1KiB / 2KiB / 4KiB / 8KiB / 16KiB / 64KiB 十档扫描
- [x] 固定 50/50 总读写量
- [x] turnaround logical event 计数
- [x] 同 trial 配对效应
- [x] MAD 噪声估计
- [x] 单位字节耗时对 event density 的线性回归
- [x] R² 质量门槛
- [x] 样本不足时 fail-closed

### 3. Read / Write 数据路径组筛查

- [x] 同 trial Read / Write / Copy 配对
- [x] 计算 Copy 每方向相对 Read / Write 参考压力
- [x] MAD 噪声门槛
- [x] 独立参考明显不自洽时自动降级为 `unresolved`
- [x] 不再尝试组内具体时序排名

### 4. Copy 背景压力与尾延迟

- [x] idle random latency
- [x] Read background loaded latency
- [x] Mixed background loaded latency
- [x] Copy background loaded latency
- [x] 64-access short-window probe
- [x] P50 / P95 / P99 / P99.9 / Max
- [x] Copy-loaded short-window probe
- [x] Copy 背景目标缓冲区完整性校验

### 5. Bank / Row 代理证据

- [x] 全范围随机依赖链
- [x] 页内局部依赖链
- [x] 1 / 2 / 4 / 8 independent chains
- [x] 没有真实 Bank/Row 映射时保持 `unresolved`
- [x] 不把虚拟页局部性伪装成真实 DRAM row-hit / row-conflict

### 6. 参数组输出契约

- [x] `diagnostic_scope = parameter_group_only`
- [x] 读写转向参数组
- [x] 读取数据路径参数组
- [x] 写入数据路径参数组
- [x] Bank 激活/并行参数组
- [x] Row 周期参数组
- [x] 刷新/尾延迟参数组
- [x] 全局数据路径 / IMC 排队上下文组
- [x] CPU 写策略上下文与 DRAM 参数组分离

### 7. 数据完整性与工程质量

- [x] 测试前后完整 pattern 校验
- [x] Copy `memcmp`
- [x] Write constant verification
- [x] WHEA 查询
- [x] worker / core / job bounds checks
- [x] `raw.csv` 写入错误检查
- [x] `profile.ini` 大小和读取完整性检查
- [x] 65 项 self-test
- [x] Linux / Windows `-Werror`
- [x] Linux AddressSanitizer
- [x] Linux UndefinedBehaviorSanitizer
- [x] Windows 实际 EXE smoke
- [x] JSON / CSV / HTML 一致性验证
- [x] 自动正式 Release

## 当前明确未完成

### A. 统一 Copy 边际损失指标

- [ ] 将不同参数组的结果转换到统一量纲
- [ ] 定义 `Marginal Copy Loss` / `Marginal Recoverable Performance`
- [ ] 不再把不同公式生成的 heuristic score 当成严格的组间性能排名
- [ ] 给每个组输出 effect estimate + uncertainty
- [ ] 处理参数组之间的 interaction / overlap
- [ ] 明确“不要求所有组 loss 相加等于 100%”

### B. 物理地址 → DRAM 拓扑映射

- [ ] Windows 下可靠获得可用于实验的物理页信息
- [ ] 自动探测候选 address bits / XOR hashing
- [ ] 构造并验证 same-row / same-bank-different-row 地址对
- [ ] 构造 cross-bank 地址组
- [ ] 构造 same-BankGroup / cross-BankGroup 地址组
- [ ] 给映射建立置信度和自检
- [ ] 映射失败时保持当前代理模式，不产生伪归因

### C. Bank / Row 正交微基准

- [ ] Row-hit baseline
- [ ] Same-bank different-row conflict
- [ ] Different-bank control
- [ ] Same-BG vs cross-BG activation
- [ ] 1 / 2 / 4 / 8 Bank activation scaling
- [ ] 将现有页内随机代理从核心诊断降级为 fallback/context
- [ ] 建立 Row 周期组的可测边际成本
- [ ] 建立 Bank 激活/并行组的可测边际成本

### D. AMD UMC / DF 硬件事件

- [ ] 确认 Ryzen 7 9700X 对应 Family/Model 的可用 UMC PMC 事件
- [ ] 读取 MemClk 或等价周期事件
- [ ] 读取 ACTIVATE 事件
- [ ] 读取 PRECHARGE / page-conflict 相关事件
- [ ] 读取可用的 Read / Write traffic 事件
- [ ] 调查是否存在可用 Refresh command / refresh busy 事件
- [ ] 将 UMC counters 与微基准结果交叉验证
- [ ] counter 不可用时明确降级，而不是猜测

### E. Refresh 归因

- [ ] 保存高频 short-window latency trace，而不只输出分位数
- [ ] stall duration histogram
- [ ] stall interval histogram
- [ ] autocorrelation / periodicity detection
- [ ] 与实际 tREFI / tRFC / Refresh Mode 对照
- [ ] 如果 UMC 有 Refresh counter，使用 counter 做硬件交叉验证
- [ ] 没有硬件 counter 时限制最高置信度，不输出“已证明 refresh”

### F. 类 ZenTimings 实际参数读取

- [ ] 设计 hardware readback abstraction
- [ ] MCLK
- [ ] UCLK
- [ ] FCLK
- [ ] Primary timings
- [ ] Secondary / tertiary timings
- [ ] GDM / Power Down / Nitro / Refresh Mode 等控制器设置
- [ ] 能可靠读取时加入 DRAM PMIC / SoC / VDDIO 等遥测，并明确数据来源
- [ ] 用硬件实读替换 `profile.ini` 作为主参数源
- [ ] 保留 `profile.ini` 仅作为用户备注/兼容输入
- [ ] 把实读参数归类到参数组，但不做组内单参数优劣排名

## 下一阶段开发顺序

推荐顺序：

1. **统一 Copy 边际损失模型**
2. **Bank / BankGroup / Row 映射基础设施**
3. **Row / Bank 正交微基准**
4. **UMC / DF 计数器接入**
5. **Refresh trace + counter 归因**
6. **ZenTimings 式参数读取**
7. **最终参数组边际 Copy Loss 排名与报告重构**

这样安排的原因是：实际参数读取只能告诉程序“当前设置是什么”，不能单独回答“哪个参数组最影响 Copy”。应先把组级因果证据和统一比较量纲做好，再把硬件实读参数接入结果模型。

## 最终目标

完整版本应输出类似：

```text
Current effective Copy: 58.4 GB/s

#1 Read/Write Turnaround Group
Marginal recoverable Copy: 5.8% ± 0.6%
Confidence: High
Evidence: orthogonal sweep + UMC traffic/counter agreement

#2 Row Cycle Group
Marginal recoverable Copy: 3.4% ± 0.5%
Confidence: High
Evidence: same-row vs same-bank-different-row + ACT/PRE counters

#3 Bank Activation/Parallelism Group
Marginal recoverable Copy: 1.1% ± 0.4%
Confidence: Medium

Refresh Group
Status: unresolved
Reason: no usable refresh hardware counter on this platform
```

程序必须始终允许输出：**当前没有足够证据证明某个参数组是主要 Copy 短板。**