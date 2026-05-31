# AttnRes 与旁路对比可视化说明

本目录用于保存 AttnRes 软件侧对比，以及 online softmax merge 旁路前后
对比的可视化数据、绘图脚本和图片。这里的两组数据服务于不同结论，不能混用。

## 目录结构

```text
data_process/attnres/
├── code/
│   └── plot_attnres_results.py
├── data/
│   ├── attnres_software_baselines.csv
│   ├── online_softmax_merge_bypass.csv
│   └── online_softmax_merge_bypass_stability.csv
└── pic/
    ├── attnres_software_traffic.png
    ├── attnres_software_error.png
    ├── online_softmax_merge_bypass_break_even.png
    ├── online_softmax_merge_bypass_cycles.png
    ├── online_softmax_merge_bypass_speedup.png
    ├── online_softmax_merge_bypass_runtime_proxy.png
    ├── online_softmax_merge_bypass_tcdm.png
    ├── online_softmax_merge_cluster_integration.png
    └── online_softmax_merge_engine_flow.png
```

## 复现方式

在仓库根目录执行：

```bash
source .venv/bin/activate
python data_process/attnres/code/plot_attnres_results.py
```

图片会生成到：

```text
data_process/attnres/pic/
```

## 第一组：AttnRes 软件侧 baseline 对比

数据文件：

```text
data/attnres_software_baselines.csv
```

这组数据来自 host 模式下的 `attnres-baselines` 正确性验证。它比较的是三个
软件实现路径在同一组合成 attention-like 输入上的输出误差和软件估算 traffic。

注意：这组数据不是 Spatz RTL 上的真实 cycle 对比，也不是硬件旁路实验。

### 字段含义

| 字段 | 含义 |
|---|---|
| `name` | baseline 实现名称。 |
| `L` | 合成 workload 中的层数。 |
| `Q` | 每个 block 的 query row 数量。 |
| `D` | hidden dimension，也可以理解为每行向量宽度。 |
| `hist_blocks` | 当前 block 需要访问的历史 block 数量。 |
| `hist_bytes` | benchmark 内部估算的历史 block 数据访问量。 |
| `partial_bytes` | benchmark 内部估算的当前 partial block 数据访问量。 |
| `state_bytes` | benchmark 内部估算的中间 online-softmax 状态访问量。 |
| `max_abs_err` | 相对 `naive-full-recompute` reference 的最大绝对误差。 |
| `max_rel_err` | 相对 `naive-full-recompute` reference 的 guarded 最大相对误差。 |

### 三个 baseline 的含义

| baseline | 含义 |
|---|---|
| `naive-full-recompute` | reference 路径。每一层都重新遍历历史 blocks 和当前 partial block。 |
| `paper-two-phase` | 论文式两阶段路径。先准备历史 block summary，再逐层处理 partial block 并 merge。 |
| `software-fusion` | 纯软件融合路径。仍使用普通存储层级，但融合软件 pass，减少中间状态 traffic。 |

### 误差计算方式

```text
max_abs_err = max(|candidate - reference|)
max_rel_err = max(|candidate - reference| / max(|reference|, 1.0))
```

这里 `max_rel_err` 使用 `max(|reference|, 1.0)` 作为分母保护，避免 reference
接近 0 时相对误差被异常放大。因此当 reference 大部分落在 `[-1, 1]` 内时，
`max_abs_err` 和 `max_rel_err` 可能相等。

### 图像说明

`attnres_software_traffic.png`

- 展示三个软件 baseline 的总估算 traffic。
- 总 traffic = `hist_bytes + partial_bytes + state_bytes`。
- `paper-two-phase` 相比 `naive-full-recompute` 减少了重复历史 block 访问。
- `software-fusion` 主要减少 `state_bytes`，体现软件融合对中间状态访问量的优化。
- 这张图适合说明软件路径的 traffic 趋势，不适合说明硬件加速 cycle。

`attnres_software_error.png`

- 展示 `paper-two-phase` 和 `software-fusion` 相对 reference 的最大相对误差。
- 虚线阈值是 `1e-3`。
- 当前误差约为 `1e-6` 量级，说明软件侧变换在这组合成输入上保持了数值一致性。

## 第二组：采用旁路 vs 不采用旁路

数据文件：

```text
data/online_softmax_merge_bypass.csv
```

这组数据来自 `online-softmax-merge` RTL benchmark，原始实验记录见：

```text
docs/online-softmax-merge-engine/COMPARISON_EXPERIMENT.md
```

这组数据是当前最适合展示“添加硬件旁路可以提升效率”的证据。

### 对比对象

| 路径 | 含义 |
|---|---|
| CPU scalar path | 不采用旁路，由 CPU 软件标量 reference 完成 merge。 |
| merge engine path | 采用 cluster-local merge engine 旁路，由硬件 engine 完成 merge。 |

### 字段含义

| 字段 | 含义 |
|---|---|
| `N` | benchmark 中独立 merge row / merge state 的数量。 |
| `D` | 每个 merge row 的向量维度。 |
| `case_id` | benchmark 中构造的输入 case 编号。 |
| `cpu_cycles` | 不采用旁路时，CPU scalar reference path 的 cycle 数。 |
| `engine_cycles` | 采用旁路时，merge engine path 的 cycle 数。 |
| `tcdm_accessed` | engine path 运行期间的 TCDM accessed 硬件计数器。 |
| `tcdm_congested` | engine path 运行期间的 TCDM congested 硬件计数器。 |

### `case_id` 语义

| case_id | 语义 |
|---:|---|
| 0 | `l_tile=0`，输出选择 old state。 |
| 1 | `l_old=0`，输出选择 tile state。 |
| 2 | `m_old==m_tile && l_old==l_tile`，等权 merge，主要用于规模 sweep。 |
| 3 | `m_old==m_tile && l_old==l_tile`，小 `l` 等权 case。 |
| 4 | `m_old==m_tile && l_old==l_tile`，另一组小 `l` 等权 case。 |

### 派生指标

```text
speedup = cpu_cycles / engine_cycles
cycle_reduction = (cpu_cycles - engine_cycles) / cpu_cycles
```

解释：

- `speedup > 1.0`：采用旁路更快。
- `speedup = 1.0`：达到 break-even。
- `speedup < 1.0`：采用旁路更慢，通常说明 workload 太小，启动、MMIO 配置和轮询
  等固定开销还没有被摊薄。

### 图像说明

`online_softmax_merge_bypass_cycles.png`

- 蓝色柱表示不采用旁路的 CPU scalar path。
- 红色柱表示采用旁路的 merge engine path。
- 较大 workload 下，例如 `N=16,D=64`，engine path 的 cycle 明显更低。

`online_softmax_merge_bypass_speedup.png`

- 展示 `cpu_cycles / engine_cycles`。
- 虚线 `1.0x` 是 break-even。
- 当前数据中 `N=16,D=64` 达到约 `2.40x` speedup。
- 小规模 case 例如 `N=1,D=1`、`N=4,D=8` 仍然是 engine 更慢，说明固定开销占主导。

`online_softmax_merge_bypass_break_even.png`

- 只展示 `case_id=2` 的受限等权 merge sweep。
- `N=4,D=16` 和 `N=8,D=8` 仍慢，`N=8,D=12` 已经超过 break-even。
- 当前数据说明 break-even 位于约 64 到 96 个 vector elements 之间。

`online_softmax_merge_bypass_runtime_proxy.png`

- 将不采用旁路的 runtime 归一化为 `1.0`。
- 采用旁路的柱值为 `engine_cycles / cpu_cycles`。
- 在固定频率假设下，cycle 比例可以作为 runtime 比例的 proxy。
- 这张图适合组会展示“运行时间趋势”，但严格来说它仍然来源于 cycle，而不是 wall time。

`online_softmax_merge_bypass_tcdm.png`

- 展示 engine path 运行期间的 TCDM accessed 和 congested counters。
- accessed 随 `N*D` 增长，说明 engine 确实通过新增 TCDM master 端口产生访存。
- congested 计数非零但远低于 accessed；当前单 engine microbenchmark 中它不是主导瓶颈。

`online_softmax_merge_engine_flow.png`

- 展示论文 A 使用的 engine 控制流图，包括正常流和 error path。

`online_softmax_merge_cluster_integration.png`

- 展示论文 A 使用的 Spatz cluster 集成图，标出 core TCDM ports、AXI-to-TCDM、
  merge engine TCDM master、TCDM interconnect、TCDM banks 和 MMIO 寄存器控制路径。

### 稳定性记录

`data/online_softmax_merge_bypass_stability.csv` 保存三次 verbose CTest 的有效
case 输出。三次运行的 cycle 和 TCDM counter 逐项一致；CTest wall time 分别为
229.02 秒、232.16 秒和 219.20 秒。论文中应使用 cycle/counter 作为主要指标，
不要把 wall time 当作架构性能结论。

## 当前可以支撑的结论

1. AttnRes 软件侧 baseline 在合成输入上输出一致，误差低于 `1e-3` 阈值。
2. 软件侧两阶段和软件融合路径能减少估算 traffic，其中 software fusion 对中间
   状态 traffic 的削减更明显。
3. 对 online softmax merge 类 workload，cluster-local merge engine 旁路在
   足够大的 `N,D` 下能显著降低 cycle。
4. 当前最强数据点是 `N=16,D=64`，speedup 约为 `2.40x`，cycle reduction 约为
   `58.3%`。
5. 当前受限等权 case 的 break-even 位于约 64 到 96 个 vector elements 之间。

## 当前不能直接外推的结论

1. 这还不能证明完整 LLM attention 或 PPL pipeline 的端到端加速。
2. AttnRes 软件侧图不能证明硬件旁路性能，它只证明软件 baseline 的正确性和
   traffic 趋势。
3. 当前 merge engine 仍是受限语义原型，尚未覆盖完整 online-softmax merge
   方程中的所有 mixed-scalar 情况。

## 建议的下一步方向

1. 扩展 merge engine datapath，支持完整 online-softmax merge 方程，然后把当前
   expected-error 的 full-reference probe 改成真正输出比对。
2. 在完整 datapath 可用后，重新生成同样的 CPU vs engine A/B 图，形成更强的论文
   证据链。
