# Spatz Architecture Notes

## 模块关系

Spatz 是一个面向 Snitch core 的 RVV 风格向量协处理器。仓库按硬件、软件和工具三层组织：

- `hw/ip/spatz/`：Spatz 向量核本体，顶层为 `spatz.sv`，内部包含 controller、decoder、VRF、VLSU、VSLDU、VFU、IPU 等模块。
- `hw/ip/spatz_cc/`：core complex 封装，`spatz_cc.sv` 将 Snitch core、Spatz 协处理器、DMA/内存接口等组合成可复用计算单元。
- `hw/system/spatz_cluster/`：完整 cluster 系统，`spatz_cluster.sv` 和生成的 wrapper/testharness 连接多个 core complex、TCDM、外部 AXI 内存、cluster peripheral 和仿真 testbench。
- `sw/snRuntime/`：裸机 runtime，提供多核启动、同步、DMA、L1 分配、printf、测试入口等基础能力。
- `sw/riscvTests/` 与 `sw/spatzBenchmarks/`：功能测试和 benchmark 程序，通过 CMake/CTest 注册并在 RTL simulator 上运行。

默认 `spatz_cluster.default.dram.hjson` 配置包含 2 个 core：一个 DMA core 和一个 compute core。每个 compute path 通过 Snitch 发起标量控制流，向 Spatz 下发向量指令，由 Spatz 的 load/store 和计算单元访问 cluster 内存。

## HW/SW 接口

软件通过标准 RISC-V ELF 进入仿真系统。Verilator/Questa/VCS testbench 使用 Front-end Server (`fesvr`) 预加载 ELF，并由 boot ROM/runtime 跳转到程序入口。

主要接口分为三类：

- 指令接口：C/ASM 测试中的 RVV 或 Spatz 扩展指令由 LLVM/Clang 编译成 RISC-V 指令流，Snitch 取指执行，向量相关操作 offload 到 Spatz。
- Runtime API：软件通过 `snrt_l1alloc()` 从 cluster L1/TCDM 分配工作区，通过 `snrt_dma_start_1d()` 等 DMA API 在 DRAM 与 L1/TCDM 之间搬运数据。
- 配置宏：`hw/system/spatz_cluster/Makefile` 从 HJSON 配置导出 `MEM_DRAM_ORIGIN`、`MEM_DRAM_SIZE`、`SNRT_TCDM_START_ADDR`、`SNRT_TCDM_SIZE`、`SNRT_CLUSTER_CORE_NUM`、`SNRT_NFPU_PER_CORE` 等宏，这些宏进入 CMake/runtime 和 linker script。

实际 benchmark 通常采用以下数据流：输入数据放在 DRAM，全局指针传入程序；DMA core 将数据搬到 TCDM；compute core 在 TCDM 上执行标量/向量计算；必要时再 DMA 写回 DRAM。

## Memory Map

Memory map 由 `hw/system/spatz_cluster/cfg/*.hjson` 决定，并同步给硬件生成、boot ROM、runtime 和 linker script。默认 DRAM 配置的关键区域如下：

| Region | Default address/size | Source |
| --- | --- | --- |
| Boot address | `0x00001000` | `cluster.boot_addr` |
| Cluster TCDM base | `0x00100000` | `cluster.cluster_base_addr` |
| Cluster TCDM size | `128 KiB` | `cluster.tcdm.size` |
| Cluster peripheral size | `64 KiB` | `cluster.cluster_periph_size` |
| DRAM base | `0x80000000` | `dram.address` |
| DRAM size | `0x80000000` | `dram.length` |

`sw/snRuntime/CMakeLists.txt` 还为不同 `SPATZ_CLUSTER_CFG` 选择默认外部内存：standalone DRAM 配置使用 `0x80000000` 起始地址，Carfield L2 配置使用 `0x78000000`、大小 `0x00400000`。修改 memory map 时，需要同时确认 HJSON 配置、生成文件和 runtime/linker script 都使用同一个 `SPATZ_CLUSTER_CFG`。

## Toolchain Role

根目录 `make all` 会准备项目固定版本的工具链和生成依赖：

- Bender：解析 RTL 依赖，生成 Verilator、VCS、QuestaSim 的 source list。
- LLVM/Clang：主要软件编译器，用于 RVV 和 Spatz 相关扩展指令。
- RISC-V GCC：提供 libstdc++ 等 GCC 工具链组件；项目软件主要不走 GCC 编译流。
- Spike/riscv-isa-sim：提供 `fesvr`，并用于 `spike-dasm` 反汇编 trace。
- Verilator：构建 cycle-level C++ 仿真模型 `bin/spatz_cluster.vlt`。
- riscv-opcodes：生成 `encoding.h` 和 `hw/ip/snitch/src/riscv_instr.sv`，保持软件编码和硬件译码一致。

## Build System

构建入口分两层。根目录 `Makefile` 负责工具链、Bender 和 opcode 生成：

```bash
make all
make init
make update_opcodes
```

cluster 目录的 `Makefile` 负责硬件生成、仿真器构建、软件配置和测试：

```bash
make -C hw/system/spatz_cluster generate
make -C hw/system/spatz_cluster bin/spatz_cluster.vlt
make -C hw/system/spatz_cluster sw.vlt
make -C hw/system/spatz_cluster sw.test.vlt
```

软件由 CMake 组织，入口是 `hw/system/spatz_cluster/sw/CMakeLists.txt`，它加入 `sw/snRuntime`、`sw/riscvTests` 和 `sw/spatzBenchmarks`。测试通过 `add_snitch_test(...)` 注册，最终由 CTest 驱动，并调用对应 simulator 运行 ELF：

```bash
cd hw/system/spatz_cluster/sw/build
ctest -R vadd
```

切换配置时使用 Make 变量，例如：

```bash
make -C hw/system/spatz_cluster sw.test.vlt SPATZ_CLUSTER_CFG=spatz_cluster.doublebw.dram.hjson
```
