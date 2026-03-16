# CDOTP 与 FDOTP 对比中 complex 同时在 cycle/time 更优的原因分析

## 1. 现象与数据

对比数据来自 [results_dotp_compare.csv](data/results_dotp_compare.csv)：

- M=128: non-complex `3697 cycles, 1.57s`；complex `3404 cycles, 1.45s`
- M=4096: non-complex `5784 cycles, 2.75s`；complex `5070 cycles, 2.41s`

在当前两组样本下，complex（`dp-cdotp_ver2`）在周期和时间两个指标都优于 non-complex（`dp-fdotp`）。

## 2. 为什么 cycle 会更优

### 2.1 指令组织与访存模式更“成对”

在 `dp-cdotp` 核函数中，复数输入通过 `vlseg2e*.v` 一次分段加载实部/虚部，然后用 4 条融合乘加相关指令完成复数乘加累积：

- `vlseg2e64.v` / `vlseg2e32.v` / `vlseg2e16.v`
- `vfmacc.vv`、`vfnmsac.vv`、`vfmacc.vv`、`vfmacc.vv`

见 [sw/spatzBenchmarks/dp-cdotp/kernel/cdotp.c](../sw/spatzBenchmarks/dp-cdotp/kernel/cdotp.c)。

`dp-fdotp` 则是标量实数点积模式，每轮主要是 `vle*` 加上 `vfmul/vfmacc` 的线性累加，见 [sw/spatzBenchmarks/dp-fdotp/kernel/fdotp.c](../sw/spatzBenchmarks/dp-fdotp/kernel/fdotp.c)。

在该平台和这两组尺寸上，complex kernel 的向量流水组织与访存/计算配比更匹配，因此实测总周期更低。

### 2.2 ver2 分块方案避免了原始实现的内存压力问题

已有排查记录指出，原始 `dp-cdotp` 在大尺寸下会出现内存压力引发的不稳定访问；`dp-cdotp_ver2` 通过分块 DMA（tiling）修复后，既稳定又保留了向量核效率。

见 [sw/spatzBenchmarks/dp*cdotp_dp_cmatmul_v1*排查与验证记录.md](../sw/spatzBenchmarks/dp_cdotp_dp_cmatmul_v1_排查与验证记录.md)。

这意味着当前拿来比较的是“修复后的 complex 实现”对“baseline non-complex 实现”，complex 路径已经过针对性优化，因此周期优势并不意外。

## 3. 为什么 time 也会更优

time 与 cycle 在本组数据中同向变化，核心原因是两者测量口径高度一致：

- 两个类型都在同一仿真环境和同一套运行流程下采集。
- `results.md` 里明确说明了当前 complex 用例在部分场景下使用了“按周期比例估算 wall-time”的口径。

见 [data_process/results.md](results.md)。

因此，当 complex 的周期已经更小，time 往往也会同步更小；这不是独立矛盾现象，而是测量口径与性能趋势一致导致的结果。

## 4. 边界与注意事项

- 当前结论只覆盖 M=128、M=4096 两个点，不能外推为“complex 永远优于 non-complex”。
- 若后续扩大尺寸、改变精度、修改 LMUL 或访存策略，结果可能反转。
- 若要做更严谨结论，建议补充更多 M 点位，并统一以 trace 周期作为主指标，再辅以统一口径的时间统计。

## 5. 结论

在当前仓库这组 dotp 对比数据里，complex 同时优于 non-complex 的主要原因是：

1. `dp-cdotp_ver2` 的向量访存与复数 FMA 序列在该平台上效率更高；
2. ver2 分块策略修复了大尺寸内存压力问题，使 kernel 优势能稳定体现；
3. time 指标与 cycle 指标口径一致，因而随周期优势同步体现。
