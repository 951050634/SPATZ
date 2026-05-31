# Figure Sources

The papers use generated figures from the repository instead of hand-edited
plots. Regenerate them from the repository root with:

```bash
source .venv/bin/activate
python data_process/attnres/code/plot_attnres_results.py
```

Current paper A figures are referenced from the LaTeX workspace root as:

```text
../data_process/attnres/pic/online_softmax_merge_bypass_cycles.png
../data_process/attnres/pic/online_softmax_merge_bypass_speedup.png
../data_process/attnres/pic/online_softmax_merge_bypass_break_even.png
../data_process/attnres/pic/online_softmax_merge_bypass_tcdm.png
../data_process/attnres/pic/online_softmax_merge_engine_flow.png
../data_process/attnres/pic/online_softmax_merge_cluster_integration.png
```

If a LaTeX build environment cannot follow relative paths outside this
directory, copy those files into `latex/common/figures/generated/` on the build
machine and update the relevant paper source paths accordingly.
