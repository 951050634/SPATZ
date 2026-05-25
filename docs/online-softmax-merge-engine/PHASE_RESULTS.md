# 阶段成果记录

本文件用于记录每个实施节点的进展、证据和备注。初始状态均为 `未开始`，后续执行时逐项更新。

## Node 0：Baseline 与 Git 准备

状态：已完成

目标：

- 建立清晰的 feature branch，并记录当前仓库基线状态。

验收标准：

- 当前工作位于 `feature/online-softmax-merge-engine`。
- 初始分支和状态已经记录。
- 无关 dirty files 已经记录。
- 构建入口可用。

证据：

- 2026-05-25 17:01 CST 复查 `git status --short --branch`，当前分支为
  `feature/online-softmax-merge-engine`。
- 2026-05-25 运行 `make -C hw/system/spatz_cluster help`，Makefile 目标列表
  可以正常打印；过程中只出现 Bender manifest warning，未修改 tracked source
  files。

备注：

- 本轮复查时工作树已经包含 online merge 相关改动和新增文件；这些改动与本
  任务相关，不作为无关 dirty changes 处理。
- 当前另有 `.codexignore` 和 `ARCHITECTURE.md` untracked 文件，尚未确认与
  本任务直接相关，本轮不纳入阶段提交。

## Node 1：硬件接口定义

状态：已完成

目标：

- 固定 v1 的软件可见寄存器接口和 TCDM 数据布局。

验收标准：

- 寄存器名称、字段和语义已经文档化。
- 软件 reference 和硬件设计使用同一套 merge 方程。
- 地址、`N`、`D` 和 stride 语义无歧义。
- v1 明确排除直接 DRAM 访问和 Spatz ISA 修改。

证据：

- [PLAN.md](PLAN.md) 已记录 `MERGE_*` MMIO 寄存器、`start` /
  `clear_done` 控制字段、`busy` / `done` / `error` 状态字段、TCDM buffer
  布局和默认 merge 方程。
- `spatz_cluster_peripheral_reg.hjson` 中已经加入同名寄存器定义，字段名与
  计划一致。

备注：

- v1 仍明确排除 Spatz ISA、decoder、controller、VFU、VRF 修改，以及直接
  DRAM 访问。

## Node 2：MMIO 寄存器支持

状态：已完成，待完整构建回归

目标：

- 扩展 cluster peripheral，用于 engine 配置、启动和状态观测。

验收标准：

- 生成后的 C header 包含所有 `MERGE_*` offset 和 field define。
- RTL 通过生成的 `reg2hw` 信号读取配置。
- RTL 通过生成的 `hw2reg` 信号写回状态。
- 现有 peripheral 功能仍然可编译。

证据：

- `hw/system/spatz_cluster/src/spatz_cluster_peripheral/spatz_cluster_peripheral_reg.hjson`
  定义了 `MERGE_SRC_M_OLD` 到 `MERGE_STATUS`。
- 生成文件
  `spatz_cluster_peripheral_reg_pkg.sv`、`spatz_cluster_peripheral_reg_top.sv`
  和 `sw/snRuntime/include/spatz_cluster_peripheral.h` 已包含对应 offset 和
  field define。
- `spatz_cluster_peripheral.sv` 将 reg2hw 配置导出为
  `merge_src_*`、`merge_dst_*`、`merge_n`、`merge_d`、`merge_stride`，
  并将 `merge_busy_i`、`merge_done_i`、`merge_error_i` 写入 hw2reg
  `MERGE_STATUS`。

备注：

- 需要通过 `bin/spatz_cluster.vlt` 或等价仿真构建确认生成寄存器代码与完整
  peripheral 仍可编译。

## Node 3：Engine RTL

状态：进行中，已有可集成受限语义版本

目标：

- 新增独立的 Streaming Merge-Update Engine RTL 模块。

验收标准：

- 模块可以加入 Spatz build source list 并完成编译。
- `start` 后 `busy=1`，直到完成或出错。
- 完成所有请求工作后，`done=1` 且 `busy=0`。
- 非法 `N=0`、`D=0` 或未对齐地址会置 `error=1`。
- 数据搬运只使用 engine 的 TCDM master 端口。

证据：

- 新增 `hw/ip/online_merge/src/online_merge_update_engine.sv`。
- 模块接口包含 clock/reset、9 个 base address、`N`、`D`、`stride`、
  `start`、`clear_done`、`busy`、`done`、`error` 和一组 TCDM
  `tcdm_req_t` / `tcdm_rsp_t` master 端口。
- RTL 状态机包含 `IDLE`、`LOAD_SCALAR`、`COMPUTE_SCALAR`、
  `STORE_SCALAR`、`UPDATE_VECTOR`、`DONE`、`ERROR`。
- `valid_cfg()` 覆盖 `N=0`、`D=0`、未 4-byte 对齐地址和未 4-byte 对齐
  stride 的错误路径。
- 2026-05-25 benchmark 新增 invalid-config cases，覆盖 `N=0`、`D=0`、
  misaligned address 和 misaligned stride 的 `error` 状态观测。
- 2026-05-25 RTL 新增 `supported_scalar_merge()` 检查：当前受限语义只允许
  `l_old=0`、`l_tile=0`，或 `m_old==m_tile && l_old==l_tile` 的等权特例；
  对其它合法但不支持的 mixed-scalar 配置显式置 `error`，避免静默写出近似
  错误结果。

备注：

- 当前 RTL 没有实现完整 `exp()` 和除法 datapath。`compute_scalar_merge()`
  与 `compute_vector_merge()` 只覆盖 `l_old=0`、`l_tile=0`、以及构造出的等
  权重/同指数特例；因此不能声称已满足 PLAN 中完整 online softmax merge
  方程。
- 下一步开发应在两条路径中择一推进：接入 FPnew/专用近似单元实现完整
  `exp`、加法、乘法、除法流水；或将 v1 明确收敛为“受限 merge 语义验证
  原型”，并把测试和文档的验收标准同步收窄。

## Node 4：Spatz Cluster 集成

状态：已完成，待完整构建回归

目标：

- 将 engine 接入 cluster MMIO 控制路径和 TCDM interconnect。

验收标准：

- engine 通过 `spatz_tcdm_interconnect` 发起请求。
- core、AXI-to-TCDM、DMA、icache 和 peripheral 路径仍然可编译。
- TCDM event counter 逻辑有效，或已经明确更新。
- v1 不修改 Spatz pipeline 文件。

证据：

- `Bender.yml` 已加入
  `hw/ip/online_merge/src/online_merge_update_engine.sv`。
- `spatz_cluster.sv` 中 `NumTCDMIn = NrTCDMPortsCores + 2`，narrow TCDM
  interconnect 输入连接为 `{axi_soc_req, merge_req, tcdm_req}`。
- `spatz_cluster.sv` 实例化 `i_online_merge_update_engine`，连接 MMIO 导出
  配置、状态返回和 TCDM master 端口。
- `merge_req.q.user.is_core = 1'b0`，`core_id` 与 `req_id` 清零，用于标记
  非 core 流量。
- TCDM event counter 的 `tcdm_event_req/rsp` 使用 `NumTCDMIn`，并纳入
  `merge_req/merge_rsp`。

备注：

- 未发现 Spatz pipeline 文件改动；本轮只读检查覆盖 `spatz_cluster.sv`、
  `Bender.yml` 和 peripheral 连接。

## Node 5：Runtime Wrapper 与 Benchmark

状态：进行中，已有受限语义 benchmark

目标：

- 提供软件入口和 benchmark 覆盖。

验收标准：

- CMake 中存在 online softmax merge 测试目标。
- `ctest -R online-softmax-merge` 可以运行。
- 硬件输出在误差范围内匹配 CPU reference。
- benchmark 报告 CPU scalar path 和 engine path 对比所需 counters。

证据：

- 新增 `sw/spatzBenchmarks/online-softmax-merge/main.c`。
- Benchmark 内提供 `smu_start(...)`、`smu_wait(void)`、`smu_done(void)`、
  `smu_error(void)`。
- `sw/spatzBenchmarks/CMakeLists.txt` 已加入
  `add_snitch_test(online-softmax-merge online-softmax-merge/main.c)`。
- Benchmark 在 TCDM 中分配 old/tile/output/ref buffers，启动 engine 后比较
  `m/l/O` 输出，并打印 CPU cycles、engine cycles、TCDM accessed 和
  congested counters。
- 2026-05-25 benchmark 增加 `run_invalid_cases()`，对 `N=0`、`D=0`、
  misaligned address、misaligned stride 四种非法配置要求 engine cleanly report
  `MERGE_STATUS.error` 且 `busy=0`。
- 2026-05-25 benchmark 增加 `run_unsupported_cases()`，覆盖配置合法但当前
  受限 datapath 不支持的 `mixed-m` 和 `unequal-l` cases，要求 engine report
  `MERGE_STATUS.error`。
- 2026-05-25 benchmark 增加 `run_stride_zero_case()`，使用 packed vector
  buffers 和 `MERGE_STRIDE=0` 覆盖 RTL 中 `stride==0` 表示 `D * 4` 默认
  row stride 的路径。

备注：

- 当前 CPU reference 与输入 case 被构造成匹配 RTL 的受限 merge 语义；它不是
  PLAN 中完整 online softmax 方程的通用 reference。
- 2026-05-25 已通过 Verilator simulator 单项 CTest 确认 benchmark 可执行并
  通过；覆盖范围仍限于当前受限语义和非法配置 error path。

## Node 6：验证与性能评估

状态：部分完成

目标：

- 验证正确性、构建集成和初步性能价值。

验收标准：

- Verilator cluster binary 可以构建。
- 面向 Verilator 的软件可以构建。
- online softmax merge 测试通过。
- 近似误差已经测量并记录。
- 至少一个目标 case 中 engine path 快于 CPU scalar reference。
- 已记录 TCDM accessed 和 congested counters。

证据：

- 2026-05-25 运行 `make -C hw/system/spatz_cluster bin/spatz_cluster.vlt`，结果
  为 `bin/spatz_cluster.vlt` 已是最新，命令退出码为 0。
- 2026-05-25 运行 `make -C hw/system/spatz_cluster sw.vlt`，CMake 日志确认
  添加 `online-softmax-merge`，软件全量构建完成，命令退出码为 0。
- 2026-05-25 在 `hw/system/spatz_cluster/sw/build` 运行
  `ctest -R online-softmax-merge --output-on-failure`，1/1 测试通过，总耗时
  90.85 秒。
- 2026-05-25 增加 invalid-config benchmark 覆盖后，重新运行
  `make -C hw/system/spatz_cluster sw.vlt`，软件全量构建完成，命令退出码为 0。
- 2026-05-25 在 `hw/system/spatz_cluster/sw/build` 重新运行
  `ctest -R online-softmax-merge --output-on-failure`，1/1 测试通过，总耗时
  108.40 秒，覆盖有效 case 与非法配置 error-path case。
- 2026-05-25 unsupported mixed-scalar 显式报错改动后，重新运行
  `make -C hw/system/spatz_cluster bin/spatz_cluster.vlt`，Verilator 硬件构建
  完成，命令退出码为 0。
- 2026-05-25 重新运行 `make -C hw/system/spatz_cluster sw.vlt`，软件全量构建
  完成，命令退出码为 0。
- 2026-05-25 在 `hw/system/spatz_cluster/sw/build` 重新运行
  `ctest -R online-softmax-merge --output-on-failure`，1/1 测试通过，总耗时
  109.14 秒，覆盖有效 case、非法配置 error-path case 和 unsupported mixed-scalar
  error-path case。

备注：

- 构建日志包含既有 warning，例如 `tech_cells_generic` manifest warning、
  runtime/benchmark 编译 warning，以及 debug address range table warning；
  未导致构建失败。
- 当前测试通过的是受限语义 benchmark，不能证明完整 online softmax merge
  方程已经实现；不支持的 mixed-scalar 合法配置现在应显式报错，而不是返回
  近似输出。
- CTest 输出未展示 benchmark 内部 cycle/counter 打印；若需要性能评估证据，
  下一阶段应保留 simulator stdout 或直接运行目标并归档 `cpu`、`engine`、
  `tcdm_accessed`、`tcdm_congested` 数值。
- 2026-05-25 新增非法配置测试已经通过 `sw.vlt` 和单项 CTest 验证。
- 2026-05-25 新增 zero-stride packed-layout benchmark case 后，重新运行
  `make -C hw/system/spatz_cluster sw.vlt`，软件全量构建完成，命令退出码为
  0。
- 2026-05-25 在 `hw/system/spatz_cluster/sw/build` 重新运行
  `ctest -R online-softmax-merge --output-on-failure`，1/1 测试通过，总耗时
  116.51 秒，覆盖有效 case、zero-stride packed-layout case、非法配置
  error-path case 和 unsupported mixed-scalar error-path case。
