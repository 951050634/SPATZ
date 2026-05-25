# Spatz Online Softmax Merge-Update Engine 工程计划

## 概述

目标是在当前 Spatz 平台中加入一个 cluster-local 的 Streaming Merge-Update Engine，用于优化 online softmax merge 这类低算强、强状态依赖、在线归一化、短生命周期状态更新的 workload。

v1 采用 MMIO 控制加 TCDM master 端口的旁路硬件方案，让引擎直接在 TCDM 附近读取和更新 `m/l/O` 状态，避免完全依赖 tensor-centric 的 Spatz 执行路径。

默认设计选择：

- v1 不修改 Spatz ISA、decoder、controller、VFU 或 VRF。
- v1 只实现一个 cluster 级共享引擎。
- v1 只支持 TCDM 内 buffer，不直接访问外部 DRAM。
- v1 使用 FP32 数据格式优先验证语义正确性。
- 远端 Git 操作如果因网络失败，只在文档中标记，不把修复网络作为本任务内容。

## Node 0：Baseline 与 Git 准备

目标：

- 建立清晰的 feature branch，并记录当前仓库基线状态。

流程：

- 运行 `git status --short --branch`。
- 确认当前分支是 `feature/online-softmax-merge-engine`。
- 如果存在无关 dirty changes，先在文档中记录。
- 运行 `make -C hw/system/spatz_cluster help`，确认本地构建入口可用。

验收标准：

- 当前工作位于 `feature/online-softmax-merge-engine` 分支。
- [GIT_NOTES.md](GIT_NOTES.md) 记录了初始分支和状态。
- 所有无关 dirty files 都已明确记录。
- 构建入口已确认，且该确认过程未修改 tracked source files。

## Node 1：硬件接口定义

目标：

- 固定 v1 的软件可见寄存器接口和 TCDM 数据布局。

必须提供的 MMIO 寄存器：

```text
MERGE_SRC_M_OLD
MERGE_SRC_L_OLD
MERGE_SRC_O_OLD
MERGE_SRC_M_TILE
MERGE_SRC_L_TILE
MERGE_SRC_O_TILE
MERGE_DST_M
MERGE_DST_L
MERGE_DST_O
MERGE_N
MERGE_D
MERGE_STRIDE
MERGE_CTRL
MERGE_STATUS
```

必须提供的控制和状态字段：

```text
MERGE_CTRL.start
MERGE_CTRL.clear_done
MERGE_STATUS.busy
MERGE_STATUS.done
MERGE_STATUS.error
```

数据语义：

```text
m_new = max(m_old, m_tile)

l_new = l_old * exp(m_old - m_new)
      + l_tile * exp(m_tile - m_new)

O_new = O_old  * (l_old  * exp(m_old  - m_new) / l_new)
      + O_tile * (l_tile * exp(m_tile - m_new) / l_new)
```

默认 TCDM 布局：

```text
m_old[N], l_old[N], O_old[N][D]
m_tile[N], l_tile[N], O_tile[N][D]
m_out[N], l_out[N], O_out[N][D]
```

验收标准：

- 寄存器名称、字段和语义在 RTL 实现前已经文档化。
- 软件 reference 和硬件设计使用同一套 merge 方程。
- 地址、`N`、`D` 和 stride 的含义没有歧义。
- v1 明确排除直接 DRAM 访问和 Spatz ISA 修改。

## Node 2：MMIO 寄存器支持

目标：

- 扩展 cluster peripheral，使软件可以配置、启动并观察 merge engine。

主要文件：

```text
hw/system/spatz_cluster/src/spatz_cluster_peripheral/spatz_cluster_peripheral_reg.hjson
hw/system/spatz_cluster/src/spatz_cluster_peripheral/spatz_cluster_peripheral.sv
sw/snRuntime/include/spatz_cluster_peripheral.h
```

流程：

- 在现有 reggen HJSON 中加入 `MERGE_*` 寄存器定义。
- 使用已有 peripheral Makefile 重新生成寄存器 package、top module 和 C header。
- 在 `spatz_cluster_peripheral.sv` 中把寄存器配置导出到 cluster top。
- 由 engine 的 `busy`、`done` 和 `error` 信号驱动 `MERGE_STATUS`。

验收标准：

- 生成后的 C header 包含所有 `MERGE_*` offset 和 field define。
- RTL 能通过生成的 `reg2hw` 信号读取 merge 配置。
- RTL 能通过生成的 `hw2reg` 信号写回 merge 状态。
- 现有 peripheral 功能保持可编译，包括 perf counter、CLINT、hardware barrier、icache prefetch 和 cluster probe。

## Node 3：Engine RTL

目标：

- 新增一个独立的 Streaming Merge-Update Engine RTL 模块。

建议文件：

```text
hw/ip/online_merge/src/online_merge_update_engine.sv
```

必须提供的接口：

- clock 和 reset。
- 所有 source 和 destination base address 配置输入。
- `N`、`D` 和 stride 配置输入。
- start 和 clear-done 控制输入。
- busy、done 和 error 状态输出。
- 一个 `tcdm_req_t` / `tcdm_rsp_t` master 接口。

v1 必须包含的状态机：

```text
IDLE
LOAD_SCALAR
COMPUTE_SCALAR
STORE_SCALAR
UPDATE_VECTOR
DONE
ERROR
```

功能行为：

- 收到 `start` 后先检查配置合法性，并置 `busy`。
- 对每一行读取 old/tile scalar state，计算 `m_new` 和 `l_new`，写回 scalar 输出，然后流式处理 `D` 个 `O` 元素。
- 所有 `N` 行完成后置 `done`。
- 对 v1 不支持或非法的配置置 `error`。

验收标准：

- 模块可以作为 Spatz build source list 的一部分完成编译。
- `start` 后 `busy=1`，直到完成或出错。
- 完成所有指定行和向量元素后，`done=1` 且 `busy=0`。
- 非法 `N=0`、`D=0` 或未对齐地址会置 `error=1`。
- 数据搬运只通过 engine 自己的 TCDM master 端口完成。

## Node 4：Spatz Cluster 集成

目标：

- 将 engine 接入 cluster 的 MMIO 控制路径和 TCDM interconnect。

主要文件：

```text
hw/system/spatz_cluster/src/spatz_cluster.sv
Bender.yml
```

必须完成的集成：

- 在 `Bender.yml` 的 Spatz/cluster source list 中加入新 RTL 文件。
- 在 `spatz_cluster.sv` 中实例化 engine。
- 新增 `merge_req` 和 `merge_rsp` 信号。
- 将 narrow TCDM interconnect 输入数量从 `NrTCDMPortsCores + 1` 改为 `NrTCDMPortsCores + 2`。
- TCDM 输入连接包含 core ports、merge engine 和 AXI-to-TCDM。
- 设置 merge request 的 user metadata，用于标记非 core 流量。

验收标准：

- engine 可以通过 `spatz_tcdm_interconnect` 发起 TCDM 请求。
- core TCDM ports、AXI-to-TCDM、DMA、icache 和 peripheral 路径仍然可编译。
- TCDM event counter 逻辑仍然有效，或已经明确更新以包含新 engine port。
- v1 不修改 Spatz pipeline 文件。

## Node 5：Runtime Wrapper 与 Benchmark

目标：

- 提供软件调用入口和 benchmark，用于验证正确性并报告性能。

建议 benchmark 目录：

```text
sw/spatzBenchmarks/online-softmax-merge/
```

必须提供的软件 API：

```c
void smu_start(...);
void smu_wait(void);
int smu_done(void);
int smu_error(void);
```

benchmark 行为：

- 在 TCDM 中准备 old state、tile state 和 output state buffers。
- 使用 CPU reference 计算同一输入的 golden result。
- 通过 MMIO 启动硬件 engine。
- 等待 engine 完成。
- 比较硬件输出和 CPU reference。
- 报告 cycle count 和相关 TCDM performance counters。

验收标准：

- CMake 中存在 online softmax merge 测试目标。
- `ctest -R online-softmax-merge` 可以运行该 benchmark。
- 硬件结果在设定误差范围内匹配软件 reference。
- benchmark 能报告足够的 counters，用于比较 CPU scalar path 和 engine path。

## Node 6：验证与性能评估

目标：

- 验证功能正确性、构建集成和初步性能价值。

必须覆盖的正确性场景：

```text
N=1,  D=1
N=4,  D=8
N=16, D=64
m_old > m_tile
m_old < m_tile
m_old == m_tile
small l_old or l_tile
non-zero mixed O_old and O_tile
```

默认 FP32 误差标准：

```text
abs_or_rel_error <= 1e-4
```

必须运行的命令：

```bash
make -C hw/system/spatz_cluster bin/spatz_cluster.vlt
make -C hw/system/spatz_cluster sw.vlt
make -C hw/system/spatz_cluster sw.test.vlt
cd hw/system/spatz_cluster/sw/build
ctest -R online-softmax-merge
```

验收标准：

- Verilator cluster binary 可以构建。
- 面向 Verilator 的软件可以构建。
- online softmax merge 测试通过。
- 如果使用 `exp` 近似，其误差已经测量并记录。
- 至少一个目标 case 中 engine path 快于 CPU scalar reference。
- 已记录 TCDM accessed 和 congested counters。

## 建议提交结构

```text
[smu] Add online merge register interface
[smu] Add streaming merge-update engine RTL
[smu] Wire merge engine into cluster TCDM
[smu] Add runtime wrapper and benchmark
[smu] Add online merge reference tests
```

每个 commit 的验收标准：

- 每个 commit 只有一个清晰工程目的。
- 生成文件与产生它们的源修改一起提交。
- 避免无关格式化噪声。
- commit subject 使用祈使语气，首字母大写，长度小于 100 字符。

## v1 不包含的内容

- 新增 RISC-V 或 Spatz 自定义指令。
- merge engine 直接访问外部 DRAM。
- 复制多个 engine。
- 完整 attention kernel 加速。
- 最终面积、时序或物理设计收敛。

## 后续方向

- 增加 FP16 或 BF16 输入，保留 FP32 accumulation。
- 将 v1 的 `exp` 实现替换为低延迟近似。
- 增加多个 outstanding TCDM requests。
- 探索 bank-aware layout，降低 TCDM congestion。
- v1 证明价值后，再评估 RF-adjacent 或 Spatz pipeline 内集成。
