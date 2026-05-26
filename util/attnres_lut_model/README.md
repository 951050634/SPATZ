# AttnRes LUT Error Model

This host-side utility sweeps LUT entries, fractional bits, and accumulator bits
for the same base-2 LUT exponential and online softmax merge structure used by
`sw/spatzBenchmarks/attnres-baselines`.

Run from the repository root:

```sh
python3 util/attnres_lut_model/sweep.py
```

The script builds `model.cpp` into a temporary host binary, runs the sweep, and
prints machine-readable `attnres-lut-sweep` rows plus one `attnres-lut-recommend`
row for the lowest-cost configuration that reaches the requested error target.
