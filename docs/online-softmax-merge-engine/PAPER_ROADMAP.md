# 双论文路线规划

## 目标

当前工作可以拆成两条论文线并行推进：

1. 受限语义原型论文：围绕已经完成的 cluster-local streaming
   merge-update engine，收敛为一篇系统实现和 microbenchmark 评估论文。
2. 完整 online softmax/attention 加速论文：在现有原型基础上补齐完整
   datapath 和端到端 workload，形成更强的体系结构论文。

这两条线不能混写。第一条线必须诚实限定在受限 merge-update 语义和
microbenchmark；第二条线才可以讨论完整 online softmax merge 方程、attention
kernel 和端到端收益。

## 论文 A：受限语义原型论文

### 临时题目

Cluster-Local Streaming Merge-Update Offload for Online Softmax State Updates
on Spatz

### 核心定位

这篇论文写当前已经实现的工程闭环：在 Spatz cluster 中新增一个由 MMIO 控制、
带 TCDM master 端口的 streaming merge-update engine，用于验证 online
softmax 状态更新类 workload 是否适合 cluster-local offload。

### 可以主张的贡献

- 在 Spatz 中实现一个非侵入式 cluster-local offload engine，不修改 ISA、
  decoder、VFU 或 VRF。
- 设计并接入 MMIO 配置寄存器、状态寄存器和 TCDM master 数据通路。
- 给出 CPU scalar path 与 merge engine path 的 A/B microbenchmark。
- 证明受限语义下，当 `N*D` 足够大时 engine 能摊薄启动开销并降低 cycle。
- 给出 break-even 趋势、TCDM accessed/congested counter 和失败 case 行为。

### 不能主张的贡献

- 不能声称已经实现完整 online softmax merge 方程。
- 不能声称已经加速完整 attention kernel 或 LLM 推理端到端流程。
- 不能把 AttnRes 软件 baseline 的 traffic 图当作硬件 speedup 证据。
- 不能把当前受限语义下的 speedup 直接外推到完整 datapath。

### 当前已有证据

- RTL：`hw/ip/online_merge/src/online_merge_update_engine.sv`。
- Cluster 集成：`hw/system/spatz_cluster/src/spatz_cluster.sv`。
- MMIO 接口：`spatz_cluster_peripheral_reg.hjson` 及生成文件。
- Runtime/header：`sw/snRuntime/include/spatz_cluster_peripheral.h`、
  `sw/snRuntime/include/perf_cnt.h`。
- Benchmark：`sw/spatzBenchmarks/online-softmax-merge/main.c`。
- 实验记录：`COMPARISON_EXPERIMENT.md`。
- 当前最大有效点：`N=16,D=64`，`cpu=23033`，`engine=9607`，
  speedup 约 `2.40x`，cycle reduction 约 `58.3%`。

### 必须补的最小工作

- 增加 break-even 附近 sweep：
  `N=4,D=16`、`N=8,D=8`、`N=8,D=12`、`N=8,D=24`。
- 重复运行当前实验至少 3 次，记录 cycle 稳定性。如果 simulator 结果完全确定，
  在文档中说明。
- 为每个实验点明确 `case_id` 的语义，避免图表读者不知道受限语义覆盖什么。
- 增加一张 engine 状态机图或数据通路图。
- 增加一张 Spatz cluster 集成图，标出 MMIO、TCDM interconnect 和 engine port。
- 写清楚启动开销来源：MMIO 配置、轮询、scalar load/store、TCDM 请求。
- 明确 error-path 测试覆盖：`N=0`、`D=0`、未对齐地址、未对齐 stride、
  unsupported mixed-scalar。

### 建议实验矩阵

| 类别 | 实验 | 目的 |
|---|---|---|
| Correctness | 有效受限语义 case 输出比对 | 证明 engine 与 CPU reference 一致 |
| Error path | 非法配置和 unsupported mixed-scalar | 证明不会静默产生错误结果 |
| Performance | CPU cycles vs engine cycles | 证明 offload 在大 workload 下有效 |
| Break-even | `N*D=32` 到 `N*D=128` sweep | 找到固定开销被摊薄的位置 |
| TCDM traffic | accessed/congested counters | 说明 engine 确实产生 TCDM traffic |

### 建议图表

- Figure 1：Spatz cluster 中的 merge engine 集成位置。
- Figure 2：Streaming merge-update engine 微架构和状态机。
- Figure 3：CPU path vs engine path cycles。
- Figure 4：speedup vs workload size。
- Figure 5：TCDM accessed/congested counters。
- Table 1：MMIO 寄存器接口。
- Table 2：支持和不支持的输入语义。
- Table 3：关键实验数据和 break-even。

### 章节结构

1. Introduction
2. Background and Motivation
   - Online softmax merge state
   - Spatz cluster and TCDM
   - 为什么 tensor-centric pipeline 不一定适合短生命周期状态更新
3. Design
   - 软件可见接口
   - Engine 微架构
   - Cluster 集成
   - 受限语义边界
4. Implementation
   - RTL、寄存器、runtime wrapper、benchmark
5. Evaluation
   - 实验环境
   - 正确性和 error path
   - cycle、speedup、break-even、TCDM counter
6. Limitations
   - 未实现完整 `exp`、乘法缩放、除法归一化
   - microbenchmark 不能代表端到端 attention
7. Related Work
8. Conclusion

### 完成标准

- 文中所有性能结论都能追溯到 `COMPARISON_EXPERIMENT.md` 或新增实验记录。
- 图表全部由仓库内脚本从 CSV 生成。
- 限制条件写在摘要、设计边界和 evaluation caveat 中，而不是只放在最后。
- 至少有一份完整初稿，哪怕先按技术报告格式写。

## 论文 B：完整 online softmax/attention 加速论文

### 临时题目

Hardware-Assisted Online Softmax Merge for Attention State Reuse on Spatz

### 核心定位

这篇论文建立在论文 A 的工程平台之上，但目标提升为完整 online softmax merge
方程和 attention-like workload。它需要证明：硬件 offload 不只是能搬运和处理
受限 case，而是在真实 mixed-scalar 输入、数值近似和端到端 attention 状态复用
中也有稳定收益。

### 目标贡献

- 实现完整 online softmax merge datapath：

  ```text
  m_new = max(m_old, m_tile)

  l_new = l_old * exp(m_old - m_new)
        + l_tile * exp(m_tile - m_new)

  O_new = O_old  * (l_old  * exp(m_old  - m_new) / l_new)
        + O_tile * (l_tile * exp(m_tile - m_new) / l_new)
  ```

- 设计适合 Spatz/TCDM 的 streaming datapath，支持 mixed-scalar 输入。
- 评估近似 `exp`、乘法、除法对误差、延迟、面积或资源的影响。
- 将 microbenchmark 扩展到 attention-like 或 AttnRes workload。
- 对比 naive full recompute、paper two-phase、software fusion 和 hardware bypass。

### 必须新增的工程工作

- 选择完整 datapath 方案：
  - 方案 1：接入 FPnew 或已有浮点单元，优先保证正确性。
  - 方案 2：实现专用近似 `exp2`/LUT、乘法和 reciprocal，优先追求低成本。
- 将当前 expected-error 的 `full-ref-probe` 改为真正输出比对。
- 扩展 benchmark 输入，覆盖：
  - `m_old > m_tile`
  - `m_old < m_tile`
  - `m_old == m_tile`
  - `l_old != l_tile`
  - 非零 mixed `O_old` 和 `O_tile`
  - 多种 `N`、`D`、stride 和 packed layout
- 增加误差统计：
  - max absolute error
  - guarded max relative error
  - mean absolute error
  - 可选：ULP 或分布图
- 把 AttnRes baseline 与硬件 path 连接起来，避免软件 traffic 图和硬件 cycle 图
  脱节。

### 必须新增的实验

| 类别 | 实验 | 目的 |
|---|---|---|
| Full correctness | 完整 mixed-scalar merge 输出比对 | 证明完整方程可用 |
| Approximation | LUT/近似参数 sweep | 找到误差和成本折中 |
| Microbenchmark | 完整 datapath CPU vs engine | 重新评估 speedup 和 break-even |
| Attention-like | AttnRes 或等价 workload | 证明不只适用于人工小 case |
| Sensitivity | `N`、`D`、hist_blocks、layers sweep | 说明收益随 workload 变化 |
| Resource | 综合面积、频率或 Verilator proxy | 给出硬件代价 |
| Traffic | TCDM counters 和软件估算 traffic | 解释性能来源 |

### 建议图表

- Figure 1：完整 datapath，包括 max、exp、scale、accumulate、normalize。
- Figure 2：近似 `exp` 或 reciprocal 的误差-资源折中。
- Figure 3：完整 mixed-scalar microbenchmark speedup。
- Figure 4：attention-like workload 的端到端 cycle 或 runtime proxy。
- Figure 5：不同 `D`、hist_blocks、layers 下的 sensitivity。
- Figure 6：TCDM accessed/congested 与性能的关系。
- Table 1：datapath 配置和 latency。
- Table 2：误差统计。
- Table 3：资源/面积/频率。

### 章节结构

1. Introduction
2. Background
   - Online softmax in attention
   - State reuse and partial block merge
   - Spatz architecture
3. Motivation
   - 软件 baseline traffic
   - 状态更新的低算强、强依赖特点
4. Architecture
   - 完整 merge datapath
   - Streaming memory schedule
   - TCDM integration and control
5. Numeric Design
   - `exp`/reciprocal 近似
   - 误差控制
6. Evaluation
   - Microbenchmark
   - Attention-like workload
   - Resource/cost
   - Sensitivity
7. Discussion
   - 适用范围
   - 与 tensor core/vector path 的关系
8. Related Work
9. Conclusion

### 完成标准

- `full-ref-probe` 不再是 expected-error，而是完整输出比对 PASS。
- 至少一个完整 mixed-scalar case 中 engine path 快于 CPU scalar path。
- 至少一个 attention-like workload 展示端到端 cycle 或 runtime proxy 改善。
- 有明确硬件代价数据，至少包括资源 proxy；最好包括综合面积和频率。
- 误差数据能支撑所选近似方案，不只展示单个 case。

## 两条线的依赖关系

论文 A 可以先写，不等待完整 datapath。论文 B 依赖论文 A 的平台和经验，但不能
复用论文 A 的受限语义结论作为完整 attention 结论。

推荐顺序：

1. 先完成论文 A 的补充 sweep、图表和初稿。
2. 同时开一个设计记录，比较完整 datapath 的两个实现方案。
3. 选定 datapath 后实现最小完整方程。
4. 将 `full-ref-probe` 从 expected-error 改为 correctness gate。
5. 重新跑完整 datapath 的 microbenchmark。
6. 再接 AttnRes 或 attention-like 端到端实验。

## 时间安排

### 第 1 周：论文 A 收敛

- 补 break-even sweep。
- 重跑并固化 CSV。
- 生成论文 A 所需图表。
- 画 cluster 集成图和 engine 状态机图。
- 写技术报告式初稿。

### 第 2 周：论文 A 修改与论文 B 方案选择

- 修改论文 A 的贡献边界和实验解释。
- 完成完整 datapath 方案比较。
- 确定使用 FPnew/已有浮点单元还是专用近似单元。
- 明确论文 B 的最小可交付版本。

### 第 3-4 周：完整 datapath 原型

- 实现 mixed-scalar 完整方程。
- 将 `full-ref-probe` 改为 PASS gate。
- 建立误差统计和 approximation sweep。
- 跑完整 datapath microbenchmark。

### 第 5-6 周：端到端 workload 与资源数据

- 接入 AttnRes 或 attention-like benchmark。
- 生成端到端 cycle/runtime proxy。
- 收集资源、频率或综合报告。
- 完成论文 B 图表和初稿。

## 风险和降级方案

| 风险 | 影响 | 降级方案 |
|---|---|---|
| 完整 datapath 太慢 | speedup 不成立 | 改写为 design-space/accuracy-cost 论文 |
| `exp`/division 资源过大 | 硬件代价难接受 | 使用 LUT/reciprocal 近似并限制精度目标 |
| 端到端 workload 接不上 | 论文 B 证据不足 | 保留完整 mixed-scalar microbenchmark，降低端到端主张 |
| TCDM congestion 放大 | 大规模性能下降 | 加 bank-aware layout 或多 outstanding request 分析 |
| Verilator 实验耗时太长 | sweep 不完整 | 缩小矩阵，保留关键点和趋势点 |

## 当前最重要的写作原则

- 论文 A 的关键词是 prototype、offload、microbenchmark、break-even。
- 论文 B 的关键词是 complete online softmax merge、numeric approximation、
  attention-like workload、end-to-end impact。
- 不要让读者误以为论文 A 已经完成论文 B 的语义。
- 所有图表必须标注数据来源、输入 case 和是否为受限语义。
