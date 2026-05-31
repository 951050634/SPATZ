# Paper B: Complete Online Softmax / Attention Extension

This directory is reserved for the future paper B source:

```text
paper_b.tex
```

Planned scope:

- Complete online softmax merge datapath.
- Mixed-scalar correctness instead of expected-error probes.
- Approximation analysis for `exp`, scaling, and reciprocal/division.
- Attention-like or AttnRes end-to-end evaluation.
- Hardware cost/resource discussion.

Paper B should reuse shared references from:

```text
../../common/bib/refs.bib
```

It should not reuse paper A's restricted-semantics speedup as evidence for
complete attention acceleration.
