#!/usr/bin/env python3
# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path

import matplotlib.pyplot as plt


@dataclass
class FairRow:
    dotp_type: str
    test_name: str
    m: int
    status: str
    test_time_sec: float
    max_cycle: int
    real_work_items: int
    flops: int
    sec_per_real_item: float
    cycle_per_real_item: float
    sec_per_gflop: float
    cycle_per_kflop: float
    complexity_factor_vs_real: float
    adjusted_time_sec: float
    adjusted_cycle: float


def load_complex_rows(csv_path: Path):
    rows = []
    with csv_path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(
                {
                    "test_name": row["test_name"],
                    "M": int(row["M"]),
                    "status": row["status"],
                    "test_time_sec": float(row["test_time_sec"]),
                    "max_cycle": int(row["max_cycle"]),
                }
            )
    rows.sort(key=lambda x: x["M"])
    return rows


def load_compare_rows(csv_path: Path):
    rows = []
    with csv_path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(
                {
                    "type": row["type"],
                    "test_name": row["test_name"],
                    "M": int(row["M"]),
                    "status": row["status"],
                    "test_time_sec": float(row["test_time_sec"]),
                    "max_cycle": int(row["max_cycle"]),
                }
            )
    rows.sort(key=lambda x: (x["M"], x["type"]))
    return rows


def validate_compare_row(row):
    dotp_type = row["type"]
    if dotp_type not in {"non-complex", "complex"}:
        raise ValueError(f"Unsupported dotp type: {dotp_type}")
    if row["M"] <= 0:
        raise ValueError(f"Invalid M value: {row['M']}")
    if row["max_cycle"] <= 0:
        raise ValueError(f"Invalid max_cycle value: {row['max_cycle']}")


def build_fair_rows(compare_rows):
    fair_rows = []
    for row in compare_rows:
        validate_compare_row(row)
        m = row["M"]
        if row["type"] == "non-complex":
            real_work_items = m
            flops = 2 * m
            complexity_factor = 1.0
        else:
            real_work_items = 2 * m
            flops = 8 * m
            complexity_factor = 4.0

        test_time_sec = row["test_time_sec"]
        max_cycle = row["max_cycle"]
        fair_rows.append(
            FairRow(
                dotp_type=row["type"],
                test_name=row["test_name"],
                m=m,
                status=row["status"],
                test_time_sec=test_time_sec,
                max_cycle=max_cycle,
                real_work_items=real_work_items,
                flops=flops,
                sec_per_real_item=test_time_sec / real_work_items,
                cycle_per_real_item=max_cycle / real_work_items,
                sec_per_gflop=test_time_sec / (flops / 1_000_000_000.0),
                cycle_per_kflop=max_cycle / (flops / 1_000.0),
                complexity_factor_vs_real=complexity_factor,
                adjusted_time_sec=test_time_sec * complexity_factor,
                adjusted_cycle=max_cycle * complexity_factor,
            )
        )
    fair_rows.sort(key=lambda x: (x.m, x.dotp_type))
    return fair_rows


def write_fair_csv(rows, csv_path: Path):
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "type",
                "test_name",
                "M",
                "status",
                "test_time_sec",
                "max_cycle",
                "real_work_items",
                "flops",
                "sec_per_real_item",
                "cycle_per_real_item",
                "sec_per_gflop",
                "cycle_per_kflop",
                "complexity_factor_vs_real",
                "adjusted_time_sec",
                "adjusted_cycle",
            ]
        )
        for r in rows:
            writer.writerow(
                [
                    r.dotp_type,
                    r.test_name,
                    r.m,
                    r.status,
                    f"{r.test_time_sec:.6f}",
                    r.max_cycle,
                    r.real_work_items,
                    r.flops,
                    f"{r.sec_per_real_item:.12f}",
                    f"{r.cycle_per_real_item:.12f}",
                    f"{r.sec_per_gflop:.6f}",
                    f"{r.cycle_per_kflop:.6f}",
                    f"{r.complexity_factor_vs_real:.1f}",
                    f"{r.adjusted_time_sec:.6f}",
                    f"{r.adjusted_cycle:.3f}",
                ]
            )


def plot_cdotp_complex(rows, out_time: Path, out_cycle: Path):
    m_values = [r["M"] for r in rows]
    times = [r["test_time_sec"] for r in rows]
    cycles = [r["max_cycle"] for r in rows]

    plt.figure(figsize=(8, 5))
    plt.plot(m_values, times, marker="o", linewidth=2.2, color="#1f77b4")
    plt.title("CDOTP (Complex) Runtime")
    plt.xlabel("M")
    plt.ylabel("Time (sec)")
    plt.grid(alpha=0.3)
    for x, y in zip(m_values, times):
        plt.text(x, y + 0.03, f"{y:.2f}", ha="center", va="bottom")
    plt.tight_layout()
    plt.savefig(out_time, dpi=160)
    plt.close()

    plt.figure(figsize=(8, 5))
    plt.plot(m_values, cycles, marker="o", linewidth=2.2, color="#d62728")
    plt.title("CDOTP (Complex) Cycle")
    plt.xlabel("M")
    plt.ylabel("Cycle")
    plt.grid(alpha=0.3)
    for x, y in zip(m_values, cycles):
        plt.text(x, y + 20, f"{y}", ha="center", va="bottom")
    plt.tight_layout()
    plt.savefig(out_cycle, dpi=160)
    plt.close()


def plot_dotp_compare(rows, out_time: Path, out_cycle: Path):
    m_values = sorted({r["M"] for r in rows})
    grouped = {"non-complex": {}, "complex": {}}
    for row in rows:
        grouped[row["type"]][row["M"]] = row

    x = list(range(len(m_values)))
    width = 0.35

    plt.figure(figsize=(8, 5))
    bars_a = plt.bar(
        [i - width / 2 for i in x],
        [grouped["non-complex"][m]["test_time_sec"] for m in m_values],
        width=width,
        color="#2ca02c",
        label="dp-fdotp (non-complex)",
    )
    bars_b = plt.bar(
        [i + width / 2 for i in x],
        [grouped["complex"][m]["test_time_sec"] for m in m_values],
        width=width,
        color="#ff7f0e",
        label="dp-cdotp_ver2 (complex)",
    )
    plt.title("DOTP Time Comparison (Real vs Complex)")
    plt.xlabel("M")
    plt.ylabel("Time (sec)")
    plt.xticks(x, m_values)
    plt.grid(axis="y", alpha=0.3)
    plt.legend()
    for bar in list(bars_a) + list(bars_b):
        val = bar.get_height()
        plt.text(bar.get_x() + bar.get_width() / 2, val + 0.02, f"{val:.2f}", ha="center", va="bottom")
    plt.tight_layout()
    plt.savefig(out_time, dpi=160)
    plt.close()

    plt.figure(figsize=(8, 5))
    bars_a = plt.bar(
        [i - width / 2 for i in x],
        [grouped["non-complex"][m]["max_cycle"] for m in m_values],
        width=width,
        color="#9467bd",
        label="dp-fdotp (non-complex)",
    )
    bars_b = plt.bar(
        [i + width / 2 for i in x],
        [grouped["complex"][m]["max_cycle"] for m in m_values],
        width=width,
        color="#8c564b",
        label="dp-cdotp_ver2 (complex)",
    )
    plt.title("DOTP Cycle Comparison (Real vs Complex)")
    plt.xlabel("M")
    plt.ylabel("Cycle")
    plt.xticks(x, m_values)
    plt.grid(axis="y", alpha=0.3)
    plt.legend()
    for bar in list(bars_a) + list(bars_b):
        val = bar.get_height()
        plt.text(bar.get_x() + bar.get_width() / 2, val + 20, f"{int(val)}", ha="center", va="bottom")
    plt.tight_layout()
    plt.savefig(out_cycle, dpi=160)
    plt.close()


def plot_dotp_fair_compare(fair_rows, out_dir: Path):
    m_values = sorted({r.m for r in fair_rows})
    grouped = {"non-complex": {}, "complex": {}}
    for row in fair_rows:
        grouped[row.dotp_type][row.m] = row

    x = list(range(len(m_values)))
    width = 0.35

    metric_specs = [
        (
            "sec_per_real_item",
            "DOTP Fair Time Comparison (Time per Real Item)",
            "Second / Real Item",
            out_dir / "dotp_compare_fair_time_per_real_item.png",
            "{:.6f}",
        ),
        (
            "cycle_per_real_item",
            "DOTP Fair Cycle Comparison (Cycle per Real Item)",
            "Cycle / Real Item",
            out_dir / "dotp_compare_fair_cycle_per_real_item.png",
            "{:.3f}",
        ),
        (
            "sec_per_gflop",
            "DOTP Fair Time Comparison (Second per GFLOP)",
            "Second / GFLOP",
            out_dir / "dotp_compare_fair_time_per_gflop.png",
            "{:.0f}",
        ),
        (
            "cycle_per_kflop",
            "DOTP Fair Cycle Comparison (Cycle per KFLOP)",
            "Cycle / KFLOP",
            out_dir / "dotp_compare_fair_cycle_per_kflop.png",
            "{:.2f}",
        ),
    ]

    for metric, title, ylabel, out_path, label_fmt in metric_specs:
        plt.figure(figsize=(8, 5))
        vals_a = [getattr(grouped["non-complex"][m], metric) for m in m_values]
        vals_b = [getattr(grouped["complex"][m], metric) for m in m_values]
        bars_a = plt.bar(
            [i - width / 2 for i in x],
            vals_a,
            width=width,
            color="#1f77b4",
            label="dp-fdotp (non-complex)",
        )
        bars_b = plt.bar(
            [i + width / 2 for i in x],
            vals_b,
            width=width,
            color="#ff7f0e",
            label="dp-cdotp_ver2 (complex)",
        )
        plt.title(title)
        plt.xlabel("M")
        plt.ylabel(ylabel)
        plt.xticks(x, m_values)
        plt.grid(axis="y", alpha=0.3)
        plt.legend()

        y_max = max(vals_a + vals_b)
        offset = y_max * 0.015 if y_max > 0 else 0.01
        for bar in list(bars_a) + list(bars_b):
            val = bar.get_height()
            plt.text(
                bar.get_x() + bar.get_width() / 2,
                val + offset,
                label_fmt.format(val),
                ha="center",
                va="bottom",
                fontsize=8,
            )
        plt.tight_layout()
        plt.savefig(out_path, dpi=160)
        plt.close()


def plot_dotp_complexity_adjusted_compare(fair_rows, out_dir: Path):
    m_values = sorted({r.m for r in fair_rows})
    grouped = {"non-complex": {}, "complex": {}}
    for row in fair_rows:
        grouped[row.dotp_type][row.m] = row

    x = list(range(len(m_values)))
    width = 0.35

    specs = [
        (
            "adjusted_time_sec",
            "DOTP Corrected Time Comparison (Complexity Adjusted)",
            "Adjusted Time (sec)",
            out_dir / "dotp_compare_fair_time_adjusted.png",
            "{:.2f}",
        ),
        (
            "adjusted_cycle",
            "DOTP Corrected Cycle Comparison (Complexity Adjusted)",
            "Adjusted Cycle",
            out_dir / "dotp_compare_fair_cycles_adjusted.png",
            "{:.0f}",
        ),
    ]

    for metric, title, ylabel, out_path, fmt in specs:
        plt.figure(figsize=(8, 5))
        vals_a = [getattr(grouped["non-complex"][m], metric) for m in m_values]
        vals_b = [getattr(grouped["complex"][m], metric) for m in m_values]
        bars_a = plt.bar(
            [i - width / 2 for i in x],
            vals_a,
            width=width,
            color="#2ca02c",
            label="dp-fdotp (non-complex)",
        )
        bars_b = plt.bar(
            [i + width / 2 for i in x],
            vals_b,
            width=width,
            color="#d62728",
            label="dp-cdotp_ver2 (complex, x4 adjusted)",
        )
        plt.title(title)
        plt.xlabel("M")
        plt.ylabel(ylabel)
        plt.xticks(x, m_values)
        plt.grid(axis="y", alpha=0.3)
        plt.legend()
        y_max = max(vals_a + vals_b)
        offset = y_max * 0.015 if y_max > 0 else 0.01
        for bar in list(bars_a) + list(bars_b):
            val = bar.get_height()
            plt.text(
                bar.get_x() + bar.get_width() / 2,
                val + offset,
                fmt.format(val),
                ha="center",
                va="bottom",
                fontsize=8,
            )
        plt.tight_layout()
        plt.savefig(out_path, dpi=160)
        plt.close()


def main():
    parser = argparse.ArgumentParser(description="Generate DOTP comparison plots.")
    parser.add_argument(
        "--with-legacy",
        action="store_true",
        help="Also regenerate legacy dotp plot files.",
    )
    args = parser.parse_args()

    base_dir = Path(__file__).resolve().parent
    data_dir = base_dir.parent / "data"
    out_dir = base_dir.parent / "pic"
    out_dir.mkdir(parents=True, exist_ok=True)

    compare_rows = load_compare_rows(data_dir / "results_dotp_compare.csv")
    fair_rows = build_fair_rows(compare_rows)
    write_fair_csv(fair_rows, data_dir / "results_dotp_compare_fair.csv")
    plot_dotp_fair_compare(fair_rows, out_dir)
    plot_dotp_complexity_adjusted_compare(fair_rows, out_dir)

    if args.with_legacy:
        complex_rows = load_complex_rows(data_dir / "results_cdotp.csv")
        plot_cdotp_complex(
            complex_rows,
            out_dir / "cdotp_time.png",
            out_dir / "cdotp_cycles.png",
        )
        plot_dotp_compare(
            compare_rows,
            out_dir / "dotp_compare_time.png",
            out_dir / "dotp_compare_cycles.png",
        )

    print(f"Generated plots in: {out_dir}")


if __name__ == "__main__":
    main()
