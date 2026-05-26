#!/usr/bin/env python3
# Copyright 2026 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

import pathlib
import re
import subprocess
import tempfile


LUT_ENTRIES = (16, 32, 64)
FRACTION_BITS = (8, 10, 12, 16)
ACCUMULATOR_BITS = (16, 24, 32)
TARGET_REL = 1.0e-3


def parse_metric(output: str, name: str) -> float:
    match = re.search(rf"{name}=([0-9.eE+-]+)", output)
    if not match:
        raise RuntimeError(f"missing {name} in model output: {output!r}")
    return float(match.group(1))


def main() -> int:
    root = pathlib.Path(__file__).resolve().parent
    source = root / "model.cpp"

    with tempfile.TemporaryDirectory(prefix="attnres-lut-model-") as tmp:
        binary = pathlib.Path(tmp) / "attnres_lut_model"
        subprocess.run(
            ["c++", "-std=c++17", "-O2", str(source), "-o", str(binary)],
            check=True,
        )

        rows = []
        for entries in LUT_ENTRIES:
            for frac_bits in FRACTION_BITS:
                for acc_bits in ACCUMULATOR_BITS:
                    if acc_bits <= frac_bits + 1:
                        continue
                    proc = subprocess.run(
                        [str(binary), str(entries), str(frac_bits), str(acc_bits)],
                        check=True,
                        text=True,
                        stdout=subprocess.PIPE,
                    )
                    max_abs = parse_metric(proc.stdout, "max_abs")
                    max_rel = parse_metric(proc.stdout, "max_rel")
                    rows.append((entries, frac_bits, acc_bits, max_abs, max_rel))
                    print(
                        "attnres-lut-sweep "
                        f"entries={entries} fraction_bits={frac_bits} "
                        f"accumulator_bits={acc_bits} max_abs={max_abs:.6e} "
                        f"max_rel={max_rel:.6e}"
                    )

        passing = [row for row in rows if row[4] <= TARGET_REL]
        if not passing:
            print(f"attnres-lut-recommend target_rel={TARGET_REL:.1e} status=none")
            return 1

        best = min(passing, key=lambda row: (row[0] * row[1] * row[2], row[4]))
        print(
            "attnres-lut-recommend "
            f"target_rel={TARGET_REL:.1e} entries={best[0]} "
            f"fraction_bits={best[1]} accumulator_bits={best[2]} "
            f"max_abs={best[3]:.6e} max_rel={best[4]:.6e}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
