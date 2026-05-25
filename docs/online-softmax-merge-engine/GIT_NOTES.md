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
