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

日期：2026-05-26 CST

命令：

```bash
ctest -R online-softmax-merge -V
```

目录：

```text
hw/system/spatz_cluster/sw/build
```

结果：

```text
1/1 Test #91: spatzBenchmarks-rtl-spatzBenchmarks-rtl-online-softmax-merge ... Passed 231.66 sec
```

## 原始数据

```text
N=1,  D=1,  case=0: cpu=174,   engine=1225, tcdm_accessed=327,  tcdm_congested=0
N=4,  D=8,  case=1: cpu=902,   engine=1451, tcdm_accessed=495,  tcdm_congested=2
N=8,  D=16, case=2: cpu=3126,  engine=2276, tcdm_accessed=974,  tcdm_congested=4
N=8,  D=32, case=2: cpu=5933,  engine=3284, tcdm_accessed=1577, tcdm_congested=14
N=16, D=32, case=2: cpu=11736, engine=5490, tcdm_accessed=2856, tcdm_congested=26
N=16, D=64, case=2: cpu=23013, engine=9635, tcdm_accessed=5253, tcdm_congested=51
N=4,  D=8,  case=3: cpu=907,   engine=1408, tcdm_accessed=486,  tcdm_congested=4
N=4,  D=8,  case=4: cpu=913,   engine=1383, tcdm_accessed=484,  tcdm_congested=2
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
| 0 | 1 | 1 | 174 | 1225 | 0.14x | engine 慢约 7.04x |
| 1 | 4 | 8 | 902 | 1451 | 0.62x | engine 慢约 1.61x |
| 2 | 8 | 16 | 3126 | 2276 | 1.37x | engine 快约 1.37x |
| 2 | 8 | 32 | 5933 | 3284 | 1.81x | engine 快约 1.81x |
| 2 | 16 | 32 | 11736 | 5490 | 2.14x | engine 快约 2.14x |
| 2 | 16 | 64 | 23013 | 9635 | 2.39x | engine 快约 2.39x |
| 3 | 4 | 8 | 907 | 1408 | 0.64x | engine 慢约 1.55x |
| 4 | 4 | 8 | 913 | 1383 | 0.66x | engine 慢约 1.51x |

在 `N=16, D=64, case=2` 中：

```text
cycle reduction = (23013 - 9635) / 23013 = 58.1%
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

   已测数据中 `N=4, D=8` 仍慢，`N=8, D=16` 已经快 1.37x。因此当前受限语义下
   break-even 位于 32 个 vector 元素和 128 个 vector 元素之间。更精确的
   break-even 需要继续补 `N=4,D=16`、`N=8,D=8`、`N=8,D=12` 等更细扫描点。

4. TCDM counter

   `tcdm_accessed` 和 `tcdm_congested` 非零，说明 engine path 确实产生了 TCDM
   traffic。`N=16,D=64` 的 congestion 为 51，当前不是主要性能瓶颈；更大规模
   或多 engine/多 core 并发时才需要进一步分析 bank conflict。

## 结论

当前受限语义原型已经证明：对足够大的 merge workload，采用 cluster-local
merge engine 能显著降低 cycles，本次 `N=16,D=64` 实测为 2.38x speedup，cycle
减少 58.1%。但对小规模 workload，engine 固定开销超过收益，`N=1,D=1` 和
`N=4,D=8` 都不适合 offload。

这个结论不能直接外推到完整 online softmax merge 方程，因为完整 datapath
需要 `exp`、乘法缩放和除法归一化，硬件 latency、面积和吞吐都会变化。下一步
应做两件事：

- 增加更细粒度 `N/D` sweep，精确定位当前原型的 break-even。
- 接入完整 datapath 后，把 `full-ref-probe` 从 expected-error 测试切换为
  输出比对，再重新做同样的 A/B 实验。
