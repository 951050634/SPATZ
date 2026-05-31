# LaTeX 工作区

linux环境下没有latex环境，所以当前文件夹下的内容只用构建到内容，不用执行编译过程。

本目录保存论文 A 和后续论文 B 的 LaTeX 工作区。上面的原始要求保留为历史约束；
当前系统已经在用户目录中配置了 TeX Live，可以直接编译本文档。

## 目录结构

```text
latex/
├── common/
│   ├── bib/
│   │   └── refs.bib
│   └── figures/
│       └── README.md
├── papers/
│   ├── paper-a/
│   │   ├── README.md
│   │   └── paper_a.tex
│   └── paper-b/
│       ├── README.md
│       └── paper_b.tex
├── paper_a.tex
├── paper_b.tex
└── texlive.profile
```

根目录的 `paper_a.tex` 和 `paper_b.tex` 是兼容入口；实际正文分别在
`papers/paper-a/` 和 `papers/paper-b/` 下。

## 数据来源

论文 A 的性能数据来自：

```text
../docs/online-softmax-merge-engine/COMPARISON_EXPERIMENT.md
../data_process/attnres/data/online_softmax_merge_bypass.csv
../data_process/attnres/data/online_softmax_merge_bypass_stability.csv
```

图表由以下脚本生成：

```bash
source ../.venv/bin/activate
python ../data_process/attnres/code/plot_attnres_results.py
```

## 当前 Linux LaTeX 环境

已安装用户级 TeX Live：

```text
/home/wxt/.texlive/2025
```

`~/.bashrc` 已加入：

```bash
export PATH=/home/wxt/.texlive/2025/bin/x86_64-linux:$PATH
```

已验证的工具：

```text
pdflatex
bibtex
latexmk
tlmgr
```

补装过的 TeX Live 包：

```text
latexmk
ieeetran
booktabs
xcolor
times
courier
```

编译论文 A：

```bash
cd ~/spatz/latex
latexmk -pdf -interaction=nonstopmode -halt-on-error paper_a.tex
```

编译论文 B 骨架：

```bash
cd ~/spatz/latex
latexmk -pdf -interaction=nonstopmode -halt-on-error paper_b.tex
```

清理生成文件：

```bash
latexmk -C paper_a.tex
latexmk -C paper_b.tex
```

当前验证结果：

```text
paper_a.pdf 已成功生成，4 页。
```
