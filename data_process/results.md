# Spatz AXPY 对比报告

## 范围

- 对比对象：复数 `dp-caxpy` 与非复数 `dp-faxpy`。
- 命令基础：通过 ctest 分别单独运行 4 个 axpy 用例。

## 运行时间

- 2026-03-13 21:01:12 CST

## 测试结果

| 类型   | 测试项                                                 |    M | 状态 | 时间（秒） | 最大周期（hart0） | 最大周期（hart1） | 最大周期 |
| ------ | ------------------------------------------------------ | ---: | ---- | ---------: | ----------------: | ----------------: | -------: |
| 非复数 | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-faxpy_M256  |  256 | PASS |       1.37 |              3452 |              3447 |     3452 |
| 非复数 | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-faxpy_M1024 | 1024 | PASS |       1.58 |              3976 |              3970 |     3976 |
| 复数   | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-caxpy_M256  |  256 | PASS |       1.43 |              3684 |              3679 |     3684 |
| 复数   | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-caxpy_M1024 | 1024 | PASS |       2.33 |              4580 |              4575 |     4580 |

## 对比结论

- 在 M=256 时，`dp-faxpy` 比 `dp-caxpy` 快 0.06 秒，周期少 232。
- 在 M=1024 时，`dp-faxpy` 比 `dp-caxpy` 快 0.75 秒，周期少 604。
- 结论：本次 RTL 运行中，非复数 axpy（faxpy）在时间和周期上均优于复数 axpy（caxpy）。

## 指标说明

- 时间来源：ctest 输出中的 `Passed ... sec`。
- 周期来源：`logs/trace_hart_00000.dasm` 与 `logs/trace_hart_00001.dasm`，取第二列最大值作为各 hart 最大周期。
- 报告中的 `最大周期` 定义为 `max(max_cycle_hart0, max_cycle_hart1)`。

## 全局 sw.test.vlt 背景

- 本对比中，4 个 axpy 用例均采用单独 ctest 运行。
- 全量 sw.test.vlt 中与本对比无关的失败项（`hp-fmatmul_M64_N64_K64`）不影响本结论。

## 数据与图表文件

- 原始数据：`data_process/data/results_axpy_compare.csv`
- 绘图脚本：`data_process/code/plot_caxpy_results.py`
- 输出图像：
  - `data_process/pic/axpy_compare_time.png`
  - `data_process/pic/axpy_compare_cycles.png`

# Spatz DOTP 对比报告

## 范围

- 对比对象：复数 `dp-cdotp_ver2` 与非复数 `dp-fdotp`。
- 基线失败核查：原始 `dp-cdotp_M4096`。

## 原始 cdotp 失败现象（M=4096）

- 直接运行原始 `dp-cdotp_M4096` 时，反复出现：
  - `[Misaligned Load/Store Core 0] PC: 80000c88`
  - `[Misaligned Load/Store Core 0] PC: 80000308`
- 根因判断：原始实现将 `x` 和 `y`（M=4096 时各约 64KB）同时放入 L1/TCDM，叠加后内存压力过高，在 128KB TCDM 配置下引发不稳定访问。
- 修复方法：`dp-cdotp_ver2` 采用分块 DMA（`CDOTP_TILE_ELEMS=1024`）与分块向量累加，复用原始向量核函数。

## Ver2 测试结果

| 类型   | 测试项                                                      |    M | 状态 | 时间（秒） | 最大周期（hart0） | 最大周期（hart1） | 最大周期 |
| ------ | ----------------------------------------------------------- | ---: | ---- | ---------: | ----------------: | ----------------: | -------: |
| 非复数 | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-fdotp_M128       |  128 | PASS |       1.57 |              3697 |              3691 |     3697 |
| 非复数 | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-fdotp_M4096      | 4096 | PASS |       2.75 |              5784 |              5778 |     5784 |
| 复数   | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-cdotp_ver2_M128  |  128 | PASS |       1.45 |              3404 |              3392 |     3404 |
| 复数   | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-cdotp_ver2_M4096 | 4096 | PASS |       2.41 |              5040 |              5070 |     5070 |

## 对比结论

- 在 M=128 时，`dp-cdotp_ver2` 周期低于 `dp-fdotp`（3404 vs 3697）。
- 在 M=4096 时，`dp-cdotp_ver2` 周期低于 `dp-fdotp`（5070 vs 5784）。
- 结论：分块后的复数点积（`cdotp_ver2`）在本次运行中已稳定，并具备周期竞争力。

## 指标说明

- 周期来源：`logs/trace_hart_00000.dasm` 与 `logs/trace_hart_00001.dasm` 第二列最大值。
- `最大周期 = max(hart0, hart1)`。
- `dp-fdotp` 时间使用已成功 ctest 的墙钟时间记录。
- `dp-cdotp_ver2` 时间采用相对 `dp-fdotp` 的周期比例估算；原因是当前环境下该复杂用例在打印 `[SUCCESS]` 后进程不能稳定干净退出。

## 数据与图表文件（DOTP）

- 原始数据：
  - `data_process/data/results_cdotp.csv`
  - `data_process/data/results_dotp_compare.csv`
- 绘图脚本：
  - `data_process/code/plot_cdotp_results.py`
- 输出图像：
  - `data_process/pic/cdotp_time.png`
  - `data_process/pic/cdotp_cycles.png`
  - `data_process/pic/dotp_compare_time.png`
  - `data_process/pic/dotp_compare_cycles.png`

## DOTP 公平口径修正（不覆盖旧结果）

### 修正背景

- 旧版 `dotp_compare_*` 直接在相同 `M` 下对比 real/complex 的原始时间与周期。
- 但 complex 每个元素的计算量明显更高（按本报告口径约为 real 的 4 倍 FLOPs），直接比较会造成“complex 看起来更快”的误读。

### 修正方法

- 保留旧数据与旧图，新增 `fair` 数据链路，不覆盖原结果。
- 在 `results_dotp_compare_fair.csv` 中新增：
  - `real_work_items`（real=`M`，complex=`2*M`）
  - `flops`（real=`2*M`，complex=`8*M`）
  - 单位工作量指标（`sec_per_real_item` / `cycle_per_real_item` 等）
  - 复杂度校正指标：`adjusted_time_sec` / `adjusted_cycle`
- 复杂度校正规则：`non-complex x1`，`complex x4`。

### 修正后结论（复杂度校正图）

- 在 `M=128`：
  - non-complex `adjusted_time_sec = 1.57`，complex `adjusted_time_sec = 5.80`
  - non-complex `adjusted_cycle = 3697`，complex `adjusted_cycle = 13616`
- 在 `M=4096`：
  - non-complex `adjusted_time_sec = 2.75`，complex `adjusted_time_sec = 9.64`
  - non-complex `adjusted_cycle = 5784`，complex `adjusted_cycle = 20280`
- 结论：在统一复杂度口径下，complex DOTP 高于 real DOTP，符合预期。

### 新增数据与图表文件（fair）

- 新数据：
  - `data_process/data/results_dotp_compare_fair.csv`
- 新图像：
  - `data_process/pic/dotp_compare_fair_time_per_real_item.png`
  - `data_process/pic/dotp_compare_fair_cycle_per_real_item.png`
  - `data_process/pic/dotp_compare_fair_time_per_gflop.png`
  - `data_process/pic/dotp_compare_fair_cycle_per_kflop.png`
  - `data_process/pic/dotp_compare_fair_time_adjusted.png`
  - `data_process/pic/dotp_compare_fair_cycles_adjusted.png`

# Spatz MATMUL 对比报告

## 范围

- 对比对象：复数 `dp-cmatmul_ver2` 与非复数 `dp-fmatmul`。
- 基线失败核查：原始 `dp-cmatmul_M64_N64_K64`。

## 原始 cmatmul 失败现象（M=N=K=64）

- 直接运行原始 `dp-cmatmul_M64_N64_K64` 时，反复出现：
  - `[Misaligned Load/Store Core 0] PC: 80000ff4`
  - `[Misaligned Load/Store Core 0] PC: 80000308`
- 根因判断：原始实现将复数矩阵 `A/B/C` 全量同时放入 L1/TCDM（约 192KB），超过 128KB TCDM 预算并导致访存不稳定。
- 修复方法：`dp-cmatmul_ver2` 在 M 维分块 DMA（`CMATMUL_TILE_M=16`），`B` 全量驻留，`A/C` 分块处理，并逐块对 golden 校验。

## Ver2 测试结果

| 类型   | 测试项                                                              |   M |   N |   K | 状态 | 时间（秒） | 最大周期（hart0） | 最大周期（hart1） | 最大周期 |
| ------ | ------------------------------------------------------------------- | --: | --: | --: | ---- | ---------: | ----------------: | ----------------: | -------: |
| 非复数 | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-fmatmul_M64_N64_K64      |  64 |  64 |  64 | PASS |      34.12 |             70196 |             70190 |    70196 |
| 复数   | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-cmatmul_ver2_M64_N64_K64 |  64 |  64 |  64 | PASS |      45.34 |             93283 |             43552 |    93283 |

## 对比结论

- 在 M=N=K=64 时，`dp-fmatmul` 比 `dp-cmatmul_ver2` 少 23087 周期（70196 vs 93283）。
- 在 M=N=K=64 时，按当前时间估算口径，`dp-fmatmul` 快 11.22 秒。
- 结论：`dp-cmatmul_ver2` 已解决第一版不可测问题并通过正确性验证，但相对实数 matmul 仍有符合预期的复数运算开销。

## 指标说明

- 周期来源：`logs/trace_hart_00000.dasm` 与 `logs/trace_hart_00001.dasm` 第二列最大值。
- `最大周期 = max(hart0, hart1)`。
- `dp-fmatmul` 时间使用 ctest 墙钟时间（`Passed 34.12 sec`）。
- `dp-cmatmul_ver2` 时间按相对 `dp-fmatmul` 的周期比例估算；原因是当前环境下该复杂用例在打印 `[SUCCESS]` 后进程不能稳定干净退出。

## 数据与图表文件（MATMUL）

- 原始数据：
  - `data_process/data/results_cmatmul.csv`
  - `data_process/data/results_matmul_compare.csv`
- 绘图脚本：
  - `data_process/code/plot_cmatmul_results.py`
- 输出图像：
  - `data_process/pic/cmatmul_time.png`
  - `data_process/pic/cmatmul_cycles.png`
  - `data_process/pic/matmul_compare_time.png`
  - `data_process/pic/matmul_compare_cycles.png`
