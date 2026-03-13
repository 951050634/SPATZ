# dp-cdotp 与 dp-cmatmul 第一版排查与验证记录

## 1. 背景与目标

- 目标：解释第一版 `dp-cdotp`、`dp-cmatmul` 为什么无法稳定通过测试，并记录排查过程、验证方式与最终结论。
- 环境：`spatz_cluster.vlt` 仿真流程，构建目录为 `hw/system/spatz_cluster/sw/build`。

## 2. dp-cdotp 第一版（原始实现）

### 2.1 现象

- 在 `M=4096` 下运行第一版 `dp-cdotp`，仿真出现反复异常输出，典型包括：
  - `[Misaligned Load/Store Core 0] PC: 80000c88`
  - `[Misaligned Load/Store Core 0] PC: 80000308`
- 测试无法正常收敛，无法得到稳定通过结果。

### 2.2 排查过程

1. 对比第一版与可通过版本（`dp-cdotp_ver2`）的数据搬运策略。
2. 检查第一版内存使用方式，确认其将整块 `x`、`y` 数据同时搬入 L1/TCDM。
3. 结合数据规模计算内存占用：
   - 复数双精度 `cdouble` 为 16 字节。
   - `M=4096` 时，单个向量大小约 `4096 * 16 = 64KB`。
   - `x + y` 同时驻留约 `128KB`，已逼近/挤占 128KB TCDM 预算，叠加其他开销后风险极高。
4. 结合异常表现（反复 misaligned load/store），判断为内存压力/布局破坏引发的访问异常。

### 2.3 验证方式

- 功能验证：运行第一版并观察异常日志；运行 `dp-cdotp_ver2`（分块 DMA）对比是否可通过。
- 性能验证：对通过版本读取 `trace_hart_00000.dasm`、`trace_hart_00001.dasm` 第二列最大周期，比较周期与稳定性。
- 结果一致性验证：`dp-cdotp_ver2` 执行后对输出与 golden（`cdotp_result`）逐项阈值比对。

### 2.4 结论

- 第一版失败主因是大规模输入下 L1/TCDM 容量与运行时开销冲突导致的异常访问风险。
- 采用分块 DMA（`CDOTP_TILE_ELEMS=1024`）后，测试可稳定通过并完成正确性校验。

## 3. dp-cmatmul 第一版（原始实现）

### 3.1 现象

- 在 `M=N=K=64` 的第一版 `dp-cmatmul` 中，仿真反复出现：
  - `[Misaligned Load/Store Core 0] PC: 80000ff4`
  - `[Misaligned Load/Store Core 0] PC: 80000308`
- 用例不能稳定结束，ctest 侧表现为无法可靠通过。

### 3.2 排查过程

1. 阅读第一版 `dp-cmatmul` 主流程，确认其策略为 `A/B/C` 全量搬入 L1/TCDM。
2. 估算内存占用（复数双精度矩阵）：
   - 每个 `64x64` 矩阵大小约 `64 * 64 * 16 = 64KB`。
   - `A + B + C` 同时驻留约 `192KB`。
3. 与 128KB TCDM 预算对比，确认明显超限。
4. 将此结论与异常日志（misaligned load/store）对应，判断根因是内存超预算导致的访存异常。
5. 制作 `dp-cmatmul_ver2`：
   - 固定 `B` 全量驻留；
   - 对 `A/C` 按 `M` 维分块（`CMATMUL_TILE_M=16`）DMA；
   - 每块执行后立即与 `cmatmul_result` 做校验。

### 3.3 验证方式

- 异常复现：对第一版执行短时运行，采集 misaligned 关键日志作为证据。
- 通过性验证：执行 `dp-cmatmul_ver2_M64_N64_K64`，确认 `Verification: SUCCESS`。
- 对比验证：与 `dp-fmatmul_M64_N64_K64` 对比，提取双 hart trace 的最大周期。
- 周期提取规则：以每个 trace 第二列末值近似为该 hart 的 max cycle，最终取两 hart 最大值。

### 3.4 结论

- 第一版 `dp-cmatmul` 失败根因是 `A/B/C` 全量并存导致 L1/TCDM 严重超预算（约 192KB > 128KB）。
- `dp-cmatmul_ver2` 的分块方案解决了可测试性和正确性问题，可稳定完成仿真并通过校验。

## 4. 总结

- 两个第一版问题本质一致：大规模复数数据在 L1/TCDM 的驻留策略不合理，触发访存异常。
- 排查方法核心是“异常日志 + 内存预算 + 版本对比 + trace 验证”四步闭环。
- 工程建议：对复数大数组用例默认采用分块 DMA 与块内校验，避免一次性全量驻留。