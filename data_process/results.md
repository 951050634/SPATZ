# Spatz AXPY Comparison Report

## Scope

- Comparison target: complex dp-caxpy vs non-complex dp-faxpy.
- Command basis: single-test runs via ctest for four axpy tests.

## Run Timestamp

- 2026-03-13 21:01:12 CST

## Test Results

| Type        | Test                                                   |    M | Status | Time (sec) | Max Cycle (hart0) | Max Cycle (hart1) | Max Cycle |
| ----------- | ------------------------------------------------------ | ---: | ------ | ---------: | ----------------: | ----------------: | --------: |
| non-complex | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-faxpy_M256  |  256 | PASS   |       1.37 |              3452 |              3447 |      3452 |
| non-complex | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-faxpy_M1024 | 1024 | PASS   |       1.58 |              3976 |              3970 |      3976 |
| complex     | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-caxpy_M256  |  256 | PASS   |       1.43 |              3684 |              3679 |      3684 |
| complex     | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-caxpy_M1024 | 1024 | PASS   |       2.33 |              4580 |              4575 |      4580 |

## Comparison Summary

- At M=256: dp-faxpy is faster by 0.06 sec and lower by 232 cycles.
- At M=1024: dp-faxpy is faster by 0.75 sec and lower by 604 cycles.
- Conclusion: in this RTL run, non-complex axpy (faxpy) is consistently faster and uses fewer cycles than complex axpy (caxpy).

## Metric Notes

- Time metric source: ctest verbose output line ending with `Passed ... sec`.
- Cycle metric source: `logs/trace_hart_00000.dasm` and `logs/trace_hart_00001.dasm`, using the second numeric column maximum as max cycle per hart.
- Reported `Max Cycle` is `max(max_cycle_hart0, max_cycle_hart1)`.

## Global sw.test.vlt Context

- In this comparison, four axpy tests were run individually with ctest.
- The unrelated failure in full sw.test.vlt (`hp-fmatmul_M64_N64_K64`) does not affect this axpy comparison.

## Data and Plot Files

- Raw data: `data_process/results_axpy_compare.csv`
- Plot script: `data_process/plot_caxpy_results.py`
- Output figures:
  - `data_process/pic/axpy_compare_time.png`
  - `data_process/pic/axpy_compare_cycles.png`

# Spatz DOTP Comparison Report

## Scope

- Comparison target: complex `dp-cdotp_ver2` vs non-complex `dp-fdotp`.
- Baseline failure check: original `dp-cdotp_M4096`.

## Original cdotp Failure (M=4096)

- Direct RTL run of original `dp-cdotp_M4096` repeatedly reports:
  - `[Misaligned Load/Store Core 0] PC: 80000c88`
  - `[Misaligned Load/Store Core 0] PC: 80000308`
- Root cause judgment: original implementation allocates full `x` and `y` (each 64KB for M=4096 complex double) in L1/TCDM simultaneously, causing severe memory pressure/corruption on 128KB TCDM setup.
- `dp-cdotp_ver2` fix: tile-based DMA (`CDOTP_TILE_ELEMS=1024`) and per-tile vector accumulation, reusing the original vector kernel.

## Ver2 Test Results

| Type        | Test                                                        |    M | Status | Time (sec) | Max Cycle (hart0) | Max Cycle (hart1) | Max Cycle |
| ----------- | ----------------------------------------------------------- | ---: | ------ | ---------: | ----------------: | ----------------: | --------: |
| non-complex | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-fdotp_M128       |  128 | PASS   |       1.57 |              3697 |              3691 |      3697 |
| non-complex | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-fdotp_M4096      | 4096 | PASS   |       2.75 |              5784 |              5778 |      5784 |
| complex     | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-cdotp_ver2_M128  |  128 | PASS   |       1.45 |              3404 |              3392 |      3404 |
| complex     | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-cdotp_ver2_M4096 | 4096 | PASS   |       2.41 |              5040 |              5070 |      5070 |

## Comparison Summary

- At M=128: `dp-cdotp_ver2` uses fewer cycles than `dp-fdotp` (3404 vs 3697).
- At M=4096: `dp-cdotp_ver2` uses fewer cycles than `dp-fdotp` (5070 vs 5784).
- In this run, tiled complex dot-product (`cdotp_ver2`) is stable and cycle-competitive.

## Metric Notes

- Cycle metric source: `logs/trace_hart_00000.dasm` and `logs/trace_hart_00001.dasm` second-column maximum.
- `Max Cycle = max(hart0, hart1)`.
- `dp-fdotp` time uses prior successful ctest wall-time records.
- `dp-cdotp_ver2` wall time is estimated by cycle-ratio scaling against `dp-fdotp`, because the standalone simulator process in this environment does not terminate cleanly after printing `[SUCCESS]`.

## Data and Plot Files (DOTP)

- Raw data:
  - `data_process/results_cdotp.csv`
  - `data_process/results_dotp_compare.csv`
- Plot script:
  - `data_process/plot_cdotp_results.py`
- Output figures:
  - `data_process/pic/cdotp_time.png`
  - `data_process/pic/cdotp_cycles.png`
  - `data_process/pic/dotp_compare_time.png`
  - `data_process/pic/dotp_compare_cycles.png`

# Spatz MATMUL Comparison Report

## Scope

- Comparison target: complex `dp-cmatmul_ver2` vs non-complex `dp-fmatmul`.
- Baseline failure check: original `dp-cmatmul_M64_N64_K64`.

## Original cmatmul Failure (M=N=K=64)

- Direct RTL run of original `dp-cmatmul_M64_N64_K64` repeatedly reports:
  - `[Misaligned Load/Store Core 0] PC: 80000ff4`
  - `[Misaligned Load/Store Core 0] PC: 80000308`
- Root cause judgment: original implementation allocates full `A/B/C` complex matrices in L1/TCDM simultaneously (`~192KB`), which exceeds the 128KB TCDM budget and leads to unstable memory behavior.
- `dp-cmatmul_ver2` fix: tile-based DMA in M dimension (`CMATMUL_TILE_M=16`), keep full `B` in L1, process `A/C` tile by tile, and verify each tile against golden output.

## Ver2 Test Results

| Type        | Test                                                                |   M |   N |   K | Status | Time (sec) | Max Cycle (hart0) | Max Cycle (hart1) | Max Cycle |
| ----------- | ------------------------------------------------------------------- | --: | --: | --: | ------ | ---------: | ----------------: | ----------------: | --------: |
| non-complex | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-fmatmul_M64_N64_K64      |  64 |  64 |  64 | PASS   |      34.12 |             70196 |             70190 |     70196 |
| complex     | spatzBenchmarks-rtl-spatzBenchmarks-rtl-dp-cmatmul_ver2_M64_N64_K64 |  64 |  64 |  64 | PASS   |      45.34 |             93283 |             43552 |     93283 |

## Comparison Summary

- At M=N=K=64: `dp-fmatmul` is lower by 23087 cycles (70196 vs 93283).
- At M=N=K=64: `dp-fmatmul` is faster by 11.22 sec using current wall-time estimation method.
- Conclusion: `dp-cmatmul_ver2` resolves the original baseline instability and produces correct output, while still carrying expected complex-arithmetic overhead vs real matmul.

## Metric Notes

- Cycle metric source: `logs/trace_hart_00000.dasm` and `logs/trace_hart_00001.dasm` second-column maximum.
- `Max Cycle = max(hart0, hart1)`.
- `dp-fmatmul` time uses direct ctest wall time (`Passed 34.12 sec`).
- `dp-cmatmul_ver2` wall time is estimated by cycle-ratio scaling against `dp-fmatmul` in this environment, because the standalone simulator process does not terminate cleanly after printing `[SUCCESS]` for this complex benchmark.

## Data and Plot Files (MATMUL)

- Raw data:
  - `data_process/results_cmatmul.csv`
  - `data_process/results_matmul_compare.csv`
- Plot script:
  - `data_process/plot_cmatmul_results.py`
- Output figures:
  - `data_process/pic/cmatmul_time.png`
  - `data_process/pic/cmatmul_cycles.png`
  - `data_process/pic/matmul_compare_time.png`
  - `data_process/pic/matmul_compare_cycles.png`
