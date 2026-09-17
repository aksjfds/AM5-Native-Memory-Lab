# modification.md

本文档定义 AM5 Native Memory Lab 在 2.2.2 之后的下一阶段详细修改方案。

目标保持不变：**只定位参数组，不定位组内具体时序。**

下一阶段要解决的不是“把更多数字塞进报告”，而是把当前的组级筛查器升级为可以在同一量纲下比较多个参数组、并且能用硬件拓扑和硬件事件验证结论的 Copy 参数组瓶颈分析器。

---

# 1. 总体目标

最终诊断链应当变成：

```text
实际 Copy workload
        ↓
Copy baseline / Read / Write / Turnaround / Row / Bank / Refresh tests
        ↓
物理地址拓扑 + UMC/DF hardware counters
        ↓
每个参数组的可观测事件数 × 单事件成本
        ↓
Marginal Copy Loss / Marginal Recoverable Performance
        ↓
重复测试 + MAD / CI / consistency checks
        ↓
参数组排名 + 置信度 + 证据来源
```

最终输出对象只能是参数组，例如：

- `TURNAROUND_GROUP`
- `READ_PATH_GROUP`
- `WRITE_PATH_GROUP`
- `ROW_CYCLE_GROUP`
- `BANK_ACTIVATION_GROUP`
- `REFRESH_GROUP`
- `GLOBAL_DATA_PATH_GROUP`

具体 `tRDWR`、`tRP`、`tRFC` 等值属于参数快照，不属于排名对象。

---

# 2. 第一阶段：统一 Copy 边际损失模型

这是下一步优先级最高的修改。

当前不同诊断使用不同 heuristic score。它们适合做“证据强弱”判断，但不适合严格回答“哪个参数组对 Copy 影响最大”。

## 2.1 新指标

新增统一指标：

```text
marginal_loss_pct
recoverable_copy_pct
uncertainty_pct
interaction_pct
```

核心定义：

```text
T_base = 当前 Copy workload 的基线耗时
T_relaxed(group) = 尽可能移除该组限制后的对照耗时

MarginalLoss(group) = (T_base - T_relaxed(group)) / T_base * 100%
```

注意：这里的 `relaxed` 不是修改 BIOS，而是通过正交微基准构造“降低某种路径事件密度”的对照。

不同组之间可能重叠，因此：

```text
sum(MarginalLoss(group)) 不要求等于 100%
```

## 2.2 数据结构修改

建议在 `Diagnostic` 中新增：

```c
typedef struct {
    ...
    double evidence_score;
    double marginal_loss_pct;
    double uncertainty_pct;
    double interaction_pct;
    unsigned evidence_flags;
} Diagnostic;
```

现有 `score` 可以逐步废弃，或重命名为 `evidence_score`，避免用户误解为可直接获得的性能百分比。

`results.json` 建议新增：

```json
{
  "parameter_group": "读写转向参数组",
  "marginal_loss_pct": 5.8,
  "uncertainty_pct": 0.6,
  "evidence_score": 86.2,
  "confidence": "high",
  "evidence_sources": [
    "turnaround_sweep",
    "copy_background",
    "umc_traffic"
  ]
}
```

## 2.3 统计方法

所有可排名组至少满足：

- ≥ 5 轮完整样本；
- 逐轮配对；
- 中位数作为中心估计；
- MAD 作为鲁棒噪声估计；
- 必要时 bootstrap paired deltas 生成置信区间；
- 方向一致性要求，例如 ≥ 80% trial 同方向；
- 不满足条件则 `unresolved` 或 `no_evidence`。

不要把 `R²` 当作概率。它只描述回归拟合程度。

---

# 3. 第二阶段：物理地址与 DRAM 拓扑映射

这是 Row / Bank 参数组从代理测试升级为真实正交测试的前提。

## 3.1 新增模块

建议新增：

```text
source/src/physmap.c
source/src/physmap.h
source/src/dram_map.c
source/src/dram_map.h
```

职责拆分：

### `physmap`

负责：

- 用户虚拟地址 → 物理页信息；
- 固定/锁定测试页；
- 提供稳定的 page descriptor；
- Windows / Linux 分平台实现。

### `dram_map`

负责：

- 对物理地址候选 bit 做差分实验；
- 推断 Bank / BankGroup / Row 相关 bit/XOR 组合；
- 建立可验证的地址集合；
- 不要求先得到完美闭式公式，只要求能构造高置信度测试池。

## 3.2 地址池

最终需要至少四类地址集合：

```text
POOL_SAME_ROW
POOL_SAME_BANK_DIFF_ROW
POOL_DIFF_BANK_SAME_BG
POOL_DIFF_BG
```

每个池必须带：

```text
sample_count
validation_runs
latency_signature
confidence
mapping_version
```

## 3.3 映射验证

不能只靠一次 latency clustering 就宣布映射成功。

验证要求：

- 多轮随机地址对；
- 多个物理页；
- 不同运行顺序；
- 显著且稳定的 row-hit / row-conflict 分布差异；
- 映射结果在重新采样后可复现；
- 映射置信度不足时禁止进入 Row/Bank 参数组排名。

失败策略：

```text
mapping_status = unavailable / weak / validated
```

只有 `validated` 才允许生成高置信度 Row/Bank 诊断。

---

# 4. 第三阶段：Row 周期参数组微基准

现有 `page_local_latency` 只能作为代理，不再作为核心归因依据。

## 4.1 新测试

新增：

```text
row_hit_latency
row_conflict_latency
cross_bank_latency
```

控制变量：

- 相同工作集规模；
- 相同访问次数；
- 相同线程；
- 相同依赖链结构；
- 只改变 DRAM 拓扑关系。

## 4.2 核心指标

```text
RowConflictPenalty =
    median(row_conflict_latency)
    - median(row_hit_latency)
```

同时记录：

```text
row_hit_ns
row_conflict_ns
cross_bank_ns
paired_mad_ns
```

如果有 UMC ACT/PRE counter，则进一步计算：

```text
observed_row_events_per_copy_gib
row_event_cost_ns
estimated_row_time_share
```

## 4.3 输出规则

只有满足：

```text
validated mapping
+ stable row-conflict delta
+ sufficient trial consistency
```

才允许：

```text
ROW_CYCLE_GROUP = priority / watch
```

否则保持 `unresolved`。

---

# 5. 第四阶段：Bank / BankGroup 激活参数组微基准

## 5.1 新测试矩阵

构造：

```text
1 Bank
2 Banks
4 Banks
8 Banks
```

同时分：

```text
same BankGroup
different BankGroup
```

所有测试保持：

- 总访问量一致；
- row-miss 数量可控；
- 线程数一致；
- 地址分布由已验证的 DRAM map 提供。

## 5.2 观测量

```text
activation_rate
latency_per_activation
bandwidth_scaling
same_bg_efficiency
cross_bg_efficiency
```

需要特别避免：

```text
“4 Bank 没继续提升 = tFAW 有问题”
```

因为仍可能受 IMC queue、CPU issue rate 或 UCLK/DF 路径限制。

因此必须与 UMC ACT counter、MemClk 等交叉验证。

---

# 6. 第五阶段：AMD UMC / DF 硬件计数器

建议独立成硬件计数模块，不与 diagnostic 逻辑耦合。

## 6.1 模块设计

新增：

```text
source/src/hw_counter.c
source/src/hw_counter.h
source/src/amd_umc.c
source/src/amd_umc.h
```

统一接口建议：

```c
typedef struct {
    bool available;
    uint64_t memclk_cycles;
    uint64_t read_events;
    uint64_t write_events;
    uint64_t activate_events;
    uint64_t precharge_events;
    uint64_t refresh_events;
    uint64_t refresh_busy_cycles;
} UmcCounters;
```

不存在的事件必须显式标记 unavailable，不能填 0 伪装成“没有发生”。

## 6.2 第一批事件优先级

优先寻找：

1. MemClk / controller cycles
2. ACTIVATE
3. PRECHARGE / page-conflict related
4. Read traffic
5. Write traffic
6. Refresh command
7. Refresh busy / blocked cycles

## 6.3 平台识别

必须基于 CPUID Family / Model / Stepping 选择事件定义。

不能拿别的 Family 1Ah 型号的事件编码直接硬套 9700X。

每个 counter profile 需要：

```text
cpu_family
cpu_model
source_document
validated_on_hardware
```

不匹配则关闭 counter，不允许猜。

---

# 7. 第六阶段：Refresh 证据链

## 7.1 保存原始 short-window trace

当前只保存分位数。下一步增加可选 trace：

```text
stall_trace.csv / binary
```

记录：

```text
timestamp
window_ns
background_gbps
cpu_id
```

完整模式建议至少采集几十万级窗口。

## 7.2 统计分析

新增：

- stall duration histogram
- interval histogram
- autocorrelation
- periodicity peak detection
- robust outlier classification

## 7.3 Refresh 判定条件

如果有硬件 counter：

```text
periodicity evidence
+ refresh counter increase
+ timing consistency
```

可以提高到 high confidence。

如果没有 hardware refresh counter，只允许：

```text
confidence <= medium
```

并且报告中必须写：

```text
periodic stall compatible with refresh; hardware attribution unavailable
```

不能写成“已证明 tRFC/tREFI 是问题”。

---

# 8. 第七阶段：ZenTimings 式硬件参数读取

这一阶段放在归因框架之后接入。

## 8.1 参数读取层接口

新增：

```text
source/src/memory_params.c
source/src/memory_params.h
source/src/amd_smn.c
source/src/amd_smn.h
```

统一输出：

```c
typedef struct {
    bool valid;
    char source[64];
    unsigned mclk;
    unsigned uclk;
    unsigned fclk;
    /* primary / secondary / tertiary timings */
    /* controller flags */
    /* voltages with explicit provenance */
} MemoryParams;
```

## 8.2 数据来源必须显式

每个值最好区分：

```text
BIOS register value
UMC effective value
PMIC readback
SMU telemetry
user annotation
```

不能把设置值和实时遥测混成同一概念。

## 8.3 与诊断器的边界

参数读取模块负责：

```text
读到什么
当前值是多少
属于哪个参数组
```

诊断器只负责：

```text
哪个参数组影响最大
证据是什么
边际 Copy Loss 是多少
```

诊断器不得重新恢复单参数排名。

---

# 9. 最终统一诊断算法

建议最终对每个参数组输出：

```text
GroupResult {
    group_id
    status
    marginal_loss_pct
    uncertainty_pct
    confidence
    evidence_score
    evidence_sources[]
    hardware_counter_support
    topology_support
    interaction_flags
    limitations[]
}
```

## 9.1 Confidence 规则

### High

要求同时满足大部分：

- 正交微基准直接刺激目标事件；
- 地址拓扑已验证；
- ≥5 配对 trial；
- effect 显著高于 MAD；
- 多轮方向一致；
- 有硬件 counter 交叉验证；
- 无数据错误；
- 无相关 WHEA；
- affinity 正常。

### Medium

有明确可重复信号，但缺少一类硬件证据。

### Low

只能看到代理现象，或者归因存在明显混杂。

### Unresolved

数据不足、映射失败、counter 不可用且无法排除主要混杂。

---

# 10. Interaction 处理

不能假设：

```text
Turnaround Loss + Row Loss + Refresh Loss = Total Copy Loss
```

需要至少增加两两 interaction 检测框架。

建议第一阶段只检测主要组：

```text
Turnaround × Row
Turnaround × Bank
Row × Bank
```

如果发现显著 interaction：

```text
interaction_pct > noise threshold
```

报告中单独显示，不强行归给任何单组。

---

# 11. 报告修改

最终报告首页建议只显示：

```text
Current Copy
Effective Copy
Top limiting parameter groups
Marginal loss
Uncertainty
Confidence
Evidence sources
```

示例：

```text
#1 读写转向参数组
Marginal Copy Loss: 5.8% ± 0.6%
Confidence: High
Evidence:
  - turnaround sweep
  - paired regression
  - UMC read/write traffic agreement

#2 Row 周期参数组
Marginal Copy Loss: 3.4% ± 0.5%
Confidence: High
Evidence:
  - validated same-row / row-conflict mapping
  - ACT/PRE counter agreement

#3 Bank 激活/并行参数组
Marginal Copy Loss: 1.1% ± 0.4%
Confidence: Medium
```

对于无法确认的组：

```text
刷新/尾延迟参数组
Status: unresolved
Reason: platform exposes no validated refresh counter
```

---

# 12. 测试与 CI 修改

每新增一层硬件能力，都必须增加对应的 fail-closed 测试。

## 12.1 单元/self-test

新增：

- marginal loss math
- paired bootstrap / MAD math
- topology classifier
- mapping consistency validator
- counter availability states
- unavailable counter ≠ zero counter
- interaction accounting

## 12.2 仿真 fixtures

新增离线 fixture：

```text
tests/fixtures/
  clean_turnaround.json
  row_conflict.json
  bank_limit.json
  refresh_periodic.json
  ambiguous.json
  missing_counter.json
```

CI 不依赖真实 AMD UMC 硬件也能验证诊断状态机。

## 12.3 Windows 硬件层

如果加入驱动/内核访问，发布流程必须额外检查：

- 驱动来源与签名策略；
- 最小权限；
- 只读寄存器路径；
- 不写 BIOS/SMN/UMC 配置寄存器；
- 失败时程序仍能退化为无硬件计数模式。

---

# 13. 不做的事情

下一阶段明确不做：

- 自动修改 BIOS；
- 自动改 DRAM 电压；
- 自动写 UMC timing register；
- 自动尝试危险时序；
- 根据一次运行宣称某个具体 tXXX 一定可以降低 1 cycle；
- 为了给出结果而强制所有参数组进入排名；
- 把 AIDA64 Copy 当作本项目 Copy workload 的严格等价物。

---

# 14. 推荐实施顺序

实际编码顺序建议：

```text
Step 1
统一 Diagnostic / GroupResult 数据模型

Step 2
实现 Marginal Copy Loss + uncertainty

Step 3
增加 synthetic fixtures 和诊断离线测试

Step 4
实现 physical page abstraction

Step 5
实现 DRAM topology inference + validation

Step 6
实现 Row 正交微基准

Step 7
实现 Bank/BG 正交微基准

Step 8
接入 UMC/DF counters

Step 9
实现 Refresh trace / periodicity / counter correlation

Step 10
接入实际内存参数读取

Step 11
报告升级为统一参数组边际损失排名
```

每一步都必须保持：

```text
证据不足 = unresolved
```

而不是回退到启发式猜测。

---

# 15. 完成标准

下一阶段完成的最低标准不是“功能都能跑”，而是满足：

1. 至少 Turnaround、Row、Bank 三个组可以使用同一个 `marginal_loss_pct` 比较；
2. Row / Bank 结论来自经过验证的实际 DRAM 拓扑地址，而不是虚拟页代理；
3. UMC counter 可用时参与证据链，不可用时能安全退化；
4. Refresh 没有硬件证据时不会被高置信度归因；
5. 所有参数组都允许 `unresolved`；
6. 报告不出现单参数优劣排名；
7. Linux / Windows CI、自检、sanitizer 和数据一致性检查继续全部通过；
8. 实际参数读取接入后只改变参数来源，不改变诊断边界。

达到以上条件后，项目才可以更严格地描述为：

> **能够在当前 AM5 配置下，用可验证的实验和硬件证据定位最影响 Copy 的参数组。**