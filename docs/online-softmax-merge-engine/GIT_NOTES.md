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

