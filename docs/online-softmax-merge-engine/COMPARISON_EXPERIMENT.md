# Online Softmax Merge Engine 对比实验

## 实验目的

对比同一 benchmark 输入下两条路径的差异：

- 不采用 merge engine：CPU scalar reference path。
- 采用 merge engine：通过 MMIO 启动 cluster-local online merge engine，由 engine
  自己发起 TCDM 访问并写回结果。

注意：当前实现中的 engine 不是片外模块，而是 cluster-local 旁路硬件模块。
如果“片外模块”指的是该新增硬件 engine，本实验即为对应 A/B 对比。当前 RTL
仍是受限语义原型，只覆盖 `l_old=0`、`l_tile=0`、以及构造出的等权特例；合法
但未支持的完整 online softmax mixed-scalar 输入会返回 `MERGE_STATUS.error`。

## 实验环境

日期：2026-05-31 CST

命令：

```bash
make -C hw/system/spatz_cluster sw.vlt
ctest -R online-softmax-merge -V
```

目录：

```text
hw/system/spatz_cluster/sw/build
```

结果：

```text
1/1 Test #91: spatzBenchmarks-rtl-spatzBenchmarks-rtl-online-softmax-merge ... Passed 229.02 sec
1/1 Test #91: spatzBenchmarks-rtl-spatzBenchmarks-rtl-online-softmax-merge ... Passed 232.16 sec
1/1 Test #91: spatzBenchmarks-rtl-spatzBenchmarks-rtl-online-softmax-merge ... Passed 219.20 sec
```

三次 verbose CTest 的有效 case 输出完全一致。下文使用三次一致的 cycle 和
TCDM counter 数据；CTest wall time 只用于说明实验运行耗时，不作为架构性能指标。

## Case 语义

| Case | 受限语义 |
|---:|---|
| 0 | `l_tile=0`，输出选择 old state。 |
| 1 | `l_old=0`，输出选择 tile state。 |
| 2 | `m_old==m_tile && l_old==l_tile`，等权 merge，主要用于规模 sweep。 |
| 3 | `m_old==m_tile && l_old==l_tile`，小 `l` 等权 case。 |
| 4 | `m_old==m_tile && l_old==l_tile`，另一组小 `l` 等权 case。 |

## 原始数据

```text
N=1,  D=1,  case=0: cpu=191,   engine=1216, tcdm_accessed=328,  tcdm_congested=1
N=4,  D=8,  case=1: cpu=915,   engine=1486, tcdm_accessed=495,  tcdm_congested=2
N=8,  D=8,  case=2: cpu=1704,  engine=1789, tcdm_accessed=682,  tcdm_congested=3
N=8,  D=12, case=2: cpu=2433,  engine=2011, tcdm_accessed=834,  tcdm_congested=4
N=4,  D=16, case=2: cpu=1613,  engine=1694, tcdm_accessed=636,  tcdm_congested=3
N=8,  D=16, case=2: cpu=3114,  engine=2254, tcdm_accessed=977,  tcdm_congested=7
N=8,  D=24, case=2: cpu=4520,  engine=2798, tcdm_accessed=1285, tcdm_congested=13
N=8,  D=32, case=2: cpu=5910,  engine=3284, tcdm_accessed=1574, tcdm_congested=11
N=16, D=32, case=2: cpu=11744, engine=5502, tcdm_accessed=2850, tcdm_congested=20
N=16, D=64, case=2: cpu=23033, engine=9607, tcdm_accessed=5243, tcdm_congested=41
N=4,  D=8,  case=3: cpu=904,   engine=1452, tcdm_accessed=498,  tcdm_congested=5
N=4,  D=8,  case=4: cpu=930,   engine=1417, tcdm_accessed=498,  tcdm_congested=5
```

错误路径和未支持路径：

```text
invalid n-zero status=0x4
invalid d-zero status=0x4
invalid misaligned-address status=0x4
invalid misaligned-stride status=0x4
unsupported mixed-m status=0x4
unsupported unequal-l status=0x4
full-ref-probe generic-mixed status=0x4 ref_l0=0x3f5e3b41 ref_o00=0xbe567a2b
```

## 性能对比

`speedup = cpu_cycles / engine_cycles`。大于 1 表示采用 engine 更快，小于 1
表示采用 engine 更慢。

| Case | N | D | CPU cycles | Engine cycles | Speedup | 结论 |
|---|---:|---:|---:|---:|---:|---|
| 0 | 1 | 1 | 191 | 1216 | 0.16x | engine 慢约 6.37x |
| 1 | 4 | 8 | 915 | 1486 | 0.62x | engine 慢约 1.62x |
| 2 | 8 | 8 | 1704 | 1789 | 0.95x | engine 慢约 1.05x |
| 2 | 8 | 12 | 2433 | 2011 | 1.21x | engine 快约 1.21x |
| 2 | 4 | 16 | 1613 | 1694 | 0.95x | engine 慢约 1.05x |
| 2 | 8 | 16 | 3114 | 2254 | 1.38x | engine 快约 1.38x |
| 2 | 8 | 24 | 4520 | 2798 | 1.62x | engine 快约 1.62x |
| 2 | 8 | 32 | 5910 | 3284 | 1.80x | engine 快约 1.80x |
| 2 | 16 | 32 | 11744 | 5502 | 2.13x | engine 快约 2.13x |
| 2 | 16 | 64 | 23033 | 9607 | 2.40x | engine 快约 2.40x |
| 3 | 4 | 8 | 904 | 1452 | 0.62x | engine 慢约 1.61x |
| 4 | 4 | 8 | 930 | 1417 | 0.66x | engine 慢约 1.52x |

在 `N=16, D=64, case=2` 中：

```text
cycle reduction = (23033 - 9607) / 23033 = 58.3%
```

## 如何评估结果

1. 功能正确性

   `online-softmax-merge PASS` 表示当前受限语义有效 case 的 `m/l/O` 输出与
   CPU reference 匹配，非法配置和未支持 mixed-scalar 输入按预期返回
   `MERGE_STATUS.error`。

2. 性能价值

   当前 engine 有固定启动开销，包括 MMIO 配置、状态轮询、scalar state 读取、
   TCDM 请求和写回。因此小问题规模下 engine 更慢；当 `N*D` 足够大时，engine
   能摊薄启动开销并超过 CPU scalar path。

3. 当前 break-even

   已测数据中 `N=4,D=16` 与 `N=8,D=8` 都约为 0.95x，仍略慢；
   `N=8,D=12` 已达到 1.21x。因此当前受限等权语义下，break-even 位于约
   64 到 96 个 vector elements 之间。

4. TCDM counter

   `tcdm_accessed` 和 `tcdm_congested` 非零，说明 engine path 确实产生了 TCDM
   traffic。`N=16,D=64` 的 congestion 为 41，当前不是主要性能瓶颈；更大规模
   或多 engine/多 core 并发时才需要进一步分析 bank conflict。

## 结论

当前受限语义原型已经证明：对足够大的 merge workload，采用 cluster-local
merge engine 能显著降低 cycles，本次 `N=16,D=64` 实测为 2.40x speedup，cycle
减少 58.3%。但对小规模 workload，engine 固定开销超过收益，`N=1,D=1` 和
`N=4,D=8` 都不适合 offload；受限等权 case 的 break-even 约在 64 到 96 个
vector elements 之间。

这个结论不能直接外推到完整 online softmax merge 方程，因为完整 datapath
需要 `exp`、乘法缩放和除法归一化，硬件 latency、面积和吞吐都会变化。下一步
应做两件事：

- 接入完整 datapath 后，把 `full-ref-probe` 从 expected-error 测试切换为
  输出比对，再重新做同样的 A/B 实验。
