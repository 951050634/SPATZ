# Paper A: Restricted Merge-Update Prototype

This directory contains the paper A source:

```text
paper_a.tex
```

Scope:

- Restricted merge-update prototype.
- Cluster-local MMIO plus TCDM master offload.
- Verilator microbenchmark, break-even, and TCDM counters.

Build from this directory:

```bash
latexmk -pdf -interaction=nonstopmode -halt-on-error paper_a.tex
```

Or build through the compatibility wrapper from `latex/`:

```bash
cd ~/spatz/latex
latexmk -pdf -interaction=nonstopmode -halt-on-error paper_a.tex
```
