# Git 记录

## 初始状态

以下状态在创建文档目录时记录。

```text
## feature/online-softmax-merge-engine
```

创建本目录前，`git status --short --branch` 未报告 dirty tracked files 或 untracked files。

## 分支策略

主开发分支：

```text
feature/online-softmax-merge-engine
```

建议提交顺序：

```text
[smu] Add online merge register interface
[smu] Add streaming merge-update engine RTL
[smu] Wire merge engine into cluster TCDM
[smu] Add runtime wrapper and benchmark
[smu] Add online merge reference tests
```

## 网络相关 Git 操作记录

如果后续执行依赖网络的 Git 操作失败，在这里记录即可。该项目任务不要求修复网络配置。

记录模板：

```text
日期：
命令：
结果：
错误摘要：
后续处理：
```

当前记录：

```text
暂无。
```

## 2026-05-25 本轮本地记录

分支：

```text
feature/online-softmax-merge-engine
```

本轮复查到的相关改动：

```text
M  Bender.yml
M  hw/system/spatz_cluster/src/spatz_cluster.sv
M  hw/system/spatz_cluster/src/spatz_cluster_peripheral/spatz_cluster_peripheral.sv
M  hw/system/spatz_cluster/src/spatz_cluster_peripheral/spatz_cluster_peripheral_reg.hjson
M  hw/system/spatz_cluster/src/spatz_cluster_peripheral/spatz_cluster_peripheral_reg_pkg.sv
M  hw/system/spatz_cluster/src/spatz_cluster_peripheral/spatz_cluster_peripheral_reg_top.sv
M  sw/snRuntime/include/spatz_cluster_peripheral.h
M  sw/spatzBenchmarks/CMakeLists.txt
?? docs/online-softmax-merge-engine/
?? hw/ip/online_merge/
?? sw/spatzBenchmarks/online-softmax-merge/
```

备注：

- 上述改动均与 online softmax merge engine 任务相关。
- `.codexignore` 和 `ARCHITECTURE.md` 也处于 untracked 状态，但本轮未确认与
  当前任务直接相关，暂不纳入阶段提交。
- 本轮文档更新用于把阶段记录校正为当前真实状态，并记录 RTL/benchmark
  仍是受限语义原型的事实。
- 尚未执行远端 Git 操作；没有网络相关 Git 失败需要记录。

## 2026-05-25 Invalid-config 测试记录

本轮计划提交：

```text
[smu] Cover merge engine invalid configs
```

范围：

```text
M  docs/online-softmax-merge-engine/PHASE_RESULTS.md
M  docs/online-softmax-merge-engine/GIT_NOTES.md
M  sw/spatzBenchmarks/online-softmax-merge/main.c
```

目的：

- 在 benchmark 中保留现有有效输入 smoke/performance cases。
- 新增 `N=0`、`D=0`、misaligned address、misaligned stride 四个非法配置
  case，验证 engine 报告 `MERGE_STATUS.error` 且不保持 busy。
- 补强 Node 3 的 error-path 验收证据。

网络相关 Git 操作：

```text
暂无。
```

## 2026-05-31 Paper A and LaTeX workspace 记录

本轮计划提交：

```text
[paper] Prepare restricted merge paper workspace
```

范围：

```text
M  sw/spatzBenchmarks/online-softmax-merge/main.c
M  docs/online-softmax-merge-engine/README.md
M  docs/online-softmax-merge-engine/PHASE_RESULTS.md
M  docs/online-softmax-merge-engine/COMPARISON_EXPERIMENT.md
A  docs/online-softmax-merge-engine/PAPER_ROADMAP.md
M  data_process/attnres/README.md
M  data_process/attnres/code/plot_attnres_results.py
M  data_process/attnres/data/online_softmax_merge_bypass.csv
A  data_process/attnres/data/online_softmax_merge_bypass_stability.csv
A  data_process/attnres/pic/online_softmax_merge_bypass_break_even.png
A  data_process/attnres/pic/online_softmax_merge_bypass_tcdm.png
A  data_process/attnres/pic/online_softmax_merge_cluster_integration.png
A  data_process/attnres/pic/online_softmax_merge_engine_flow.png
A  latex/
```

目的：

- 将当前工作规划为论文 A 和论文 B 两条路线。论文 A 收敛为受限语义原型论文；
  论文 B 预留完整 online softmax/attention 加速方向。
- 为论文 A 补齐 break-even 附近 sweep：
  `N=8,D=8`、`N=8,D=12`、`N=4,D=16`、`N=8,D=24`。
- 连续三次运行 verbose CTest，确认有效 case 的 cycle/counter 输出逐项一致。
- 更新 CSV、实验文档和绘图脚本，新增 break-even、TCDM counter、engine flow、
  cluster integration 图。
- 建立 `latex/` 工作区，并重构为 `common/` 与 `papers/paper-a`、
  `papers/paper-b` 布局；保留 `paper_a.tex` 和 `paper_b.tex` 兼容入口。
- 在当前 Linux 用户目录安装 TeX Live，并验证 `paper_a.tex` 与 `paper_b.tex`
  都可从 `latex/` 根目录用 `latexmk` 编译。
- `latex/.gitignore` 忽略 LaTeX 编译产物和生成 PDF，只保留源码、共享 bib、
  README 和 TeX Live profile。

验证：

```text
make -C hw/system/spatz_cluster sw.vlt
ctest -R online-softmax-merge -V
ctest -R online-softmax-merge -V
ctest -R online-softmax-merge -V
source .venv/bin/activate
python data_process/attnres/code/plot_attnres_results.py
cd latex
latexmk -pdf -interaction=nonstopmode -halt-on-error paper_a.tex
latexmk -pdf -interaction=nonstopmode -halt-on-error paper_b.tex
```

结果：

```text
sw.vlt 软件全量构建完成，退出码 0。
三次 online-softmax-merge verbose CTest 均 1/1 通过，总耗时分别为
229.02 秒、232.16 秒和 219.20 秒。
三次有效 case 的 cycle/counter 数据逐项一致。
N=16,D=64 达到 2.40x speedup，cycle reduction 约 58.3%。
受限等权 case 的 break-even 位于约 64 到 96 个 vector elements 之间。
绘图脚本成功生成 AttnRes/SMU 图表。
paper_a.pdf 成功生成，4 页；paper_b.pdf 成功生成，1 页。
```

网络相关 Git 操作：

```text
安装 TeX Live 时 TinyTeX GitHub 下载失败，改用 CTAN/SJTU 镜像完成用户级安装。
未执行远端 Git push/pull。
```

## 2026-05-31 Paper A sweep and draft 记录

本轮计划提交：

```text
[smu] Prepare restricted merge-update paper draft
```

范围：

```text
M  sw/spatzBenchmarks/online-softmax-merge/main.c
M  docs/online-softmax-merge-engine/COMPARISON_EXPERIMENT.md
M  docs/online-softmax-merge-engine/PHASE_RESULTS.md
M  data_process/attnres/README.md
M  data_process/attnres/code/plot_attnres_results.py
M  data_process/attnres/data/online_softmax_merge_bypass.csv
A  data_process/attnres/data/online_softmax_merge_bypass_stability.csv
A  data_process/attnres/pic/online_softmax_merge_bypass_break_even.png
A  data_process/attnres/pic/online_softmax_merge_bypass_tcdm.png
A  latex/
```

目的：

- 为论文 A 补齐 break-even 附近 sweep 点：
  `N=8,D=8`、`N=8,D=12`、`N=4,D=16`、`N=8,D=24`。
- 连续三次运行 verbose CTest，确认有效 case 的 cycle/counter 输出逐项一致。
- 更新 CSV、可视化脚本和图表，新增 break-even 图与 TCDM counter 图。
- 在 `latex/` 中建立论文 A 初稿，保留 README 中“不要求本机编译 LaTeX”的要求。

验证：

```text
make -C hw/system/spatz_cluster sw.vlt
ctest -R online-softmax-merge -V
ctest -R online-softmax-merge -V
ctest -R online-softmax-merge -V
source .venv/bin/activate
python data_process/attnres/code/plot_attnres_results.py
```

结果：

```text
sw.vlt 软件全量构建完成，退出码 0。
三次 online-softmax-merge verbose CTest 均 1/1 通过，总耗时分别为
229.02 秒、232.16 秒和 219.20 秒。
三次有效 case 的 cycle/counter 数据逐项一致。
N=16,D=64 达到 2.40x speedup，cycle reduction 约 58.3%。
受限等权 case 的 break-even 位于约 64 到 96 个 vector elements 之间。
绘图脚本使用 .venv 成功生成 AttnRes/SMU 图表。
```

网络相关 Git 操作：

```text
暂无。
```

## 2026-05-25 Unsupported mixed-scalar 记录

本轮计划提交：

```text
[smu] Reject unsupported merge cases
```

范围：

```text
M  docs/online-softmax-merge-engine/PHASE_RESULTS.md
M  docs/online-softmax-merge-engine/GIT_NOTES.md
M  hw/ip/online_merge/src/online_merge_update_engine.sv
M  sw/spatzBenchmarks/online-softmax-merge/main.c
```

目的：

- 将当前受限 datapath 的支持范围显式化：允许 `l_old=0`、`l_tile=0`，或
  `m_old==m_tile && l_old==l_tile` 的等权特例。
- 对合法但当前未支持的 mixed-scalar 输入返回 `MERGE_STATUS.error`，避免
  benchmark 之外的输入静默得到近似错误结果。
- 在 benchmark 中新增 `mixed-m` 和 `unequal-l` unsupported cases。

网络相关 Git 操作：

```text
暂无。
```


## 2026-05-25 Zero-stride packed-layout 记录

本轮计划提交：

```text
[smu] Cover merge engine zero stride
```

范围：

```text
M  docs/online-softmax-merge-engine/PHASE_RESULTS.md
M  docs/online-softmax-merge-engine/GIT_NOTES.md
M  sw/spatzBenchmarks/online-softmax-merge/main.c
```

目的：

- 在 benchmark 中新增 packed vector buffers。
- 使用 `MERGE_STRIDE=0` 运行 `N=4, D=8` 有效 case，覆盖 RTL 中
  `stride==0` 自动使用 `D * sizeof(float)` 的默认 packed-layout 语义。
- 继续保持当前受限 merge 语义边界，不把该 case 解释为完整 online softmax
  方程覆盖。

验证：

```text
make -C hw/system/spatz_cluster sw.vlt
ctest -R online-softmax-merge --output-on-failure
```

结果：

```text
sw.vlt 软件全量构建完成，退出码 0。
online-softmax-merge 单项 CTest 1/1 通过，总耗时 116.51 秒。
```

网络相关 Git 操作：

```text
暂无。
```

## 2026-05-25 Busy-status 观测记录

本轮计划提交：

```text
[smu] Check merge engine busy status
```

范围：

```text
M  docs/online-softmax-merge-engine/PHASE_RESULTS.md
M  docs/online-softmax-merge-engine/GIT_NOTES.md
M  sw/spatzBenchmarks/online-softmax-merge/main.c
```

目的：

- 保留 `smu_wait(void)` 软件 API。
- 新增内部等待 helper，在有效 engine run 完成前记录是否观测到
  `MERGE_STATUS.busy=1`。
- 对 regular stride 和 zero-stride packed-layout 有效 case 均要求 busy 至少被
  观测到一次，补强 PLAN Node 3 的 `start` 后 busy 可见性验收证据。

验证：

```text
make -C hw/system/spatz_cluster sw.vlt
ctest -R online-softmax-merge --output-on-failure
```

结果：

```text
sw.vlt 软件全量构建完成，退出码 0。
online-softmax-merge 单项 CTest 1/1 通过，总耗时 116.35 秒。
```

网络相关 Git 操作：

```text
暂无。
```

## 2026-05-25 Verbose performance/counter 记录

本轮计划提交：

```text
[smu] Record merge benchmark counters
```

范围：

```text
M  docs/online-softmax-merge-engine/PHASE_RESULTS.md
M  docs/online-softmax-merge-engine/GIT_NOTES.md
M  sw/spatzBenchmarks/online-softmax-merge/main.c
```

目的：

- 将 busy-observed 硬性检查收敛到 `N=16, D=64` 长运行 case，避免短 case 中
  软件轮询错过短暂 busy 后误报失败。
- 运行 verbose CTest，保留 benchmark stdout 中的 `cpu`、`engine`、
  `tcdm_accessed`、`tcdm_congested` 数值。
- 为 Node 6 的“至少一个目标 case 中 engine path 快于 CPU scalar reference”
  和 counter 记录要求补充证据。

验证：

```text
make -C hw/system/spatz_cluster sw.vlt
ctest -R online-softmax-merge -V
```

结果：

```text
sw.vlt 软件全量构建完成，退出码 0。
online-softmax-merge verbose CTest 1/1 通过，总耗时 114.31 秒。
N=16, D=64, case=2: cpu=23017, engine=9629, tcdm_accessed=0, tcdm_congested=0。
```

备注：

- verbose run 之前曾观察到 `N=1, D=1` 短 case 可能因软件轮询粒度错过 busy，
  因此本轮修正 benchmark 检查范围；该现象没有作为远端 Git 或网络问题记录。
- 当前 TCDM counter 输出均为 0，后续应复核 counter event 是否覆盖 merge engine
  port 或当前 counter 选择是否适用于该路径。

网络相关 Git 操作：

```text
暂无。
```

## 2026-05-25 Perf-counter runtime layout 记录

本轮计划提交：

```text
[smu] Fix runtime perf counter layout
```

范围：

```text
M  docs/online-softmax-merge-engine/PHASE_RESULTS.md
M  docs/online-softmax-merge-engine/GIT_NOTES.md
M  sw/snRuntime/include/perf_cnt.h
```

目的：

- 复核 verbose benchmark 中 `tcdm_accessed=0` / `tcdm_congested=0` 的原因。
- 确认 RTL/peripheral 已经把 merge engine TCDM port 纳入 `tcdm_events`。
- 修正 runtime `perf_reg_t` 使用的 counter 数量，使其匹配生成寄存器 header 的
  `SPATZ_CLUSTER_PERIPHERAL_PARAM_NUM_PERF_COUNTERS`，避免软件按 16 个 counter
  计算 `hart_select` / `perf_counter` MMIO offset。

验证：

```text
make -C hw/system/spatz_cluster sw.vlt
ctest -R online-softmax-merge -V
```

结果：

```text
sw.vlt 软件全量构建完成，退出码 0。
online-softmax-merge verbose CTest 1/1 通过，总耗时 116.14 秒。
N=16, D=64, case=2: cpu=23017, engine=9629, tcdm_accessed=5218, tcdm_congested=16。
```

网络相关 Git 操作：

```text
暂无。
```

## 2026-05-26 Full-reference probe 记录

本轮计划提交：

```text
[smu] Add full merge reference probe
```

范围：

```text
M  docs/online-softmax-merge-engine/PHASE_RESULTS.md
M  docs/online-softmax-merge-engine/GIT_NOTES.md
M  sw/spatzBenchmarks/online-softmax-merge/main.c
```

目的：

- 阅读并复核 `docs/online-softmax-merge-engine/` 后，确认当前实现仍是受限
  merge 语义原型，完整 `exp`、乘法缩放和除法归一化 datapath 尚未接入。
- 在 benchmark 中新增合法通用 mixed-scalar probe，覆盖后续完整 online
  softmax merge 方程需要处理的 `m_old > m_tile`、`m_old < m_tile`、
  `m_old == m_tile`、unequal `l`、small `l` 和 mixed `O` 输入组合。
- 当前 RTL 对该 probe 仍应返回 `MERGE_STATUS.error`，避免静默输出近似错误；
  probe 同时记录固定完整方程 golden 样本，供后续 datapath 实现后切换为
  输出比对使用。
- 曾尝试在 probe 中运行 `expf` 和运行时浮点除法，但 Verilator/bare-metal
  测试耗时不可接受；最终改为固定 golden 样本，保持测试目的并恢复单测时长。

验证：

```text
make -C hw/system/spatz_cluster sw.vlt
ctest -R online-softmax-merge -V
```

结果：

```text
sw.vlt 软件全量构建完成，退出码 0。
online-softmax-merge verbose CTest 1/1 通过，总耗时 148.11 秒。
full-ref-probe generic-mixed status=0x4 ref_l0=0x3f5e3b41 ref_o00=0xbe567a2b。
```

网络相关 Git 操作：

```text
暂无。
```

## 2026-05-26 Comparison experiment 记录

本轮计划提交：

```text
[smu] Record merge engine comparison experiment
```

范围：

```text
M  docs/online-softmax-merge-engine/README.md
M  docs/online-softmax-merge-engine/PHASE_RESULTS.md
A  docs/online-softmax-merge-engine/COMPARISON_EXPERIMENT.md
M  sw/spatzBenchmarks/online-softmax-merge/main.c
```

目的：

- 回答“不采用模块”和“采用模块”的差距。这里将“不采用模块”定义为
  benchmark 中的 CPU scalar reference path，将“采用模块”定义为 MMIO 启动
  cluster-local merge engine path。
- 在 benchmark 中增加 `N=8,D=16`、`N=8,D=32`、`N=16,D=32` 三个中间规模
  sweep 点，定位当前受限语义原型的 break-even 区间。
- 新增 `COMPARISON_EXPERIMENT.md`，记录实验目的、原始数据、speedup 计算、
  评估方法和结论边界。

验证：

```text
make -C hw/system/spatz_cluster sw.vlt
ctest -R online-softmax-merge -V
```

结果：

```text
sw.vlt 软件全量构建完成，退出码 0。
online-softmax-merge verbose CTest 1/1 通过，总耗时 231.66 秒。
N=4,D=8 仍慢于 CPU；N=8,D=16 已达到 1.37x；N=16,D=64 达到 2.39x。
当前受限语义下 break-even 位于 32 到 128 个 vector 元素之间。
```

网络相关 Git 操作：

```text
暂无。
```
