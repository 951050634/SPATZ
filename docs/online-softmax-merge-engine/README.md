# Online Softmax Merge-Update Engine

本目录用于记录在 Spatz 平台上新增 online softmax merge 硬件旁路引擎的工程计划、阶段成果和 Git 状态。

目标分支：

```text
feature/online-softmax-merge-engine
```

文档说明：

- [PLAN.md](PLAN.md)：完整实施流程、阶段节点、目标和验收标准。
- [PHASE_RESULTS.md](PHASE_RESULTS.md)：各阶段执行结果、证据和备注记录。
- [GIT_NOTES.md](GIT_NOTES.md)：分支状态、提交策略和 Git 操作记录。

v1 的默认方向是在 cluster 内新增一个由 MMIO 寄存器控制、带 TCDM master 端口的 Streaming Merge-Update Engine。软件负责配置地址和维度并启动引擎，硬件直接在 TCDM 中流式读取和更新 online softmax merge 状态。

## 当前实现边界

截至 2026-05-25，本分支已经加入 MMIO 寄存器、cluster TCDM master 集成、
独立 engine RTL 和一个 benchmark 目标。当前 RTL 是可集成的受限语义原型：
它覆盖 TCDM 搬运、状态机、配置校验、零长度状态和等权重特例，但还没有完整
实现 PLAN 中的 `exp()`、乘法缩放和除法归一化 datapath。

因此，当前 benchmark 的 reference 和输入 case 只验证该受限语义。继续开发
前应先决定下一阶段方向：接入 FPnew/专用近似单元实现完整 online softmax
merge 方程，或把 v1 明确收敛为受限 merge-update 原型并同步收窄验收标准。

