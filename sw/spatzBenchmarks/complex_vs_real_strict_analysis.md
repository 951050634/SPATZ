# Complex vs Real Analysis (Aligned DOTP, Main-Function Check)

## Why this check was needed

Observed `120s` runtime values were suspiciously constant across complex cases.
This is a measurement artifact from the runner timeout, not a valid performance number.

## Main-function diagnosis

`dp-cdotp_aligned/main.c` had previously been modified with artificial penalties:

- tiny tile size (`CDOTP_TILE_ELEMS = 32`),
- repeated kernel accumulation,
- extra `nop` loops.

These edits were reverted. The aligned complex main is now back to the fair structure:

- tile size `1024`,
- single `cdotp_v64b` call per local chunk,
- no synthetic spin penalty.

## Corrected aligned data source

Aligned-only groups were re-run:

- `dp-fdotp_aligned_M{128,1024,2048}`
- `dp-cdotp_aligned_M{128,1024,2048}`

Corrected artifacts:

- `data_process/data/results_dotp_compare_aligned.csv`
- `data_process/data/results_cdotp_aligned.csv`
- `data_process/pic/dotp_compare_cycles_aligned_fixed_main.png`

## Corrected result (cycle metric)

Use cycle metric for comparison, because wall-time can be pinned by timeout in complex runs.

| M    | real (fdotp_aligned, cycle) | complex (cdotp_aligned, cycle) |
| ---- | --------------------------: | -----------------------------: |
| 128  |                        3866 |                           3404 |
| 1024 |                        4347 |                           4363 |
| 2048 |                        5333 |                           5063 |

## Root cause of the `120s` artifact

1. Runner timeout was used as wall-time metric.
2. Complex benchmarks can print success but still keep simulator alive until timeout.
3. That makes `test_time_sec` collapse to a fixed timeout constant and lose performance meaning.

## Recommendation

For complex-vs-real conclusions in this environment:

1. Prefer kernel cycle metric over wall-time.
2. Keep aligned main functions free of synthetic penalties.
3. If strict real>complex is required by hypothesis, prove it with hardware counters or kernel-region cycle extraction, not timeout-driven wall-time.
