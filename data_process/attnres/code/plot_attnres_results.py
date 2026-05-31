#!/usr/bin/env python3
# Copyright 2023 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

import csv
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch
from matplotlib.patches import Rectangle


COLORS = {
    "naive-full-recompute": "#4c78a8",
    "paper-two-phase": "#f58518",
    "software-fusion": "#54a24b",
    "cpu": "#4c78a8",
    "engine": "#e45756",
    "speedup": "#72b7b2",
    "accessed": "#59a14f",
    "congested": "#b07aa1",
}


def load_software_rows(path: Path):
    rows = []
    with path.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            rows.append(
                {
                    "name": row["name"],
                    "L": int(row["L"]),
                    "Q": int(row["Q"]),
                    "D": int(row["D"]),
                    "hist_blocks": int(row["hist_blocks"]),
                    "hist_bytes": int(row["hist_bytes"]),
                    "partial_bytes": int(row["partial_bytes"]),
                    "state_bytes": int(row["state_bytes"]),
                    "max_abs_err": float(row["max_abs_err"]),
                    "max_rel_err": float(row["max_rel_err"]),
                }
            )
    return rows


def load_bypass_rows(path: Path):
    rows = []
    with path.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            cpu = int(row["cpu_cycles"])
            engine = int(row["engine_cycles"])
            rows.append(
                {
                    "N": int(row["N"]),
                    "D": int(row["D"]),
                    "case_id": int(row["case_id"]),
                    "cpu_cycles": cpu,
                    "engine_cycles": engine,
                    "tcdm_accessed": int(row["tcdm_accessed"]),
                    "tcdm_congested": int(row["tcdm_congested"]),
                    "speedup": cpu / engine,
                    "cycle_reduction": (cpu - engine) / cpu,
                }
            )
    return rows


def software_case_label(row):
    return f"L{row['L']} D{row['D']} H{row['hist_blocks']}"


def bypass_case_label(row):
    return f"N{row['N']} D{row['D']} c{row['case_id']}"


def plot_software_traffic(rows, out_path: Path):
    names = ["naive-full-recompute", "paper-two-phase", "software-fusion"]
    cases = []
    by_case = {}
    for row in rows:
        key = (row["L"], row["D"], row["hist_blocks"])
        if key not in by_case:
            by_case[key] = {}
            cases.append(key)
        by_case[key][row["name"]] = row

    labels = [software_case_label(by_case[key][names[0]]) for key in cases]
    x = list(range(len(labels)))
    width = 0.24

    plt.figure(figsize=(11, 5.8))
    for offset, name in zip((-width, 0, width), names):
        values = []
        for key in cases:
            row = by_case[key][name]
            total_bytes = row["hist_bytes"] + row["partial_bytes"] + row["state_bytes"]
            values.append(total_bytes / 1024.0)
        bars = plt.bar(
            [i + offset for i in x],
            values,
            width=width,
            label=name,
            color=COLORS[name],
        )
        for bar, val in zip(bars, values):
            plt.text(
                bar.get_x() + bar.get_width() / 2,
                val + 4,
                f"{val:.0f}",
                ha="center",
                va="bottom",
                fontsize=8,
            )

    plt.title("AttnRes Software Baselines: Estimated Traffic")
    plt.xlabel("Synthetic configuration")
    plt.ylabel("Total estimated traffic (KiB)")
    plt.xticks(x, labels, rotation=30, ha="right")
    plt.grid(axis="y", alpha=0.25)
    plt.legend(ncol=3, fontsize=9)
    plt.tight_layout()
    plt.savefig(out_path, dpi=180)
    plt.close()


def plot_software_error(rows, out_path: Path):
    names = ["paper-two-phase", "software-fusion"]
    filtered = [row for row in rows if row["name"] in names]
    labels = [software_case_label(row) for row in filtered if row["name"] == names[0]]
    cases = sorted({(row["L"], row["D"], row["hist_blocks"]) for row in filtered})
    by_case = {(row["L"], row["D"], row["hist_blocks"], row["name"]): row for row in filtered}

    x = list(range(len(labels)))
    width = 0.35
    plt.figure(figsize=(11, 5.4))
    for offset, name in zip((-width / 2, width / 2), names):
        values = [by_case[(key[0], key[1], key[2], name)]["max_rel_err"] for key in cases]
        bars = plt.bar(
            [i + offset for i in x],
            values,
            width=width,
            label=name,
            color=COLORS[name],
        )
        for bar, val in zip(bars, values):
            plt.text(
                bar.get_x() + bar.get_width() / 2,
                val + 0.08e-6,
                f"{val:.1e}",
                ha="center",
                va="bottom",
                fontsize=8,
            )

    plt.axhline(1.0e-3, color="#444444", linestyle="--", linewidth=1.2, label="threshold 1e-3")
    plt.yscale("log")
    plt.title("AttnRes Software Baselines: Error vs Naive Reference")
    plt.xlabel("Synthetic configuration")
    plt.ylabel("Max relative error (log scale)")
    plt.xticks(x, labels, rotation=30, ha="right")
    plt.grid(axis="y", which="both", alpha=0.25)
    plt.legend(ncol=3, fontsize=9)
    plt.tight_layout()
    plt.savefig(out_path, dpi=180)
    plt.close()


def plot_bypass_cycles(rows, out_path: Path):
    labels = [bypass_case_label(row) for row in rows]
    x = list(range(len(labels)))
    width = 0.36

    plt.figure(figsize=(10.8, 5.6))
    cpu_bars = plt.bar(
        [i - width / 2 for i in x],
        [row["cpu_cycles"] for row in rows],
        width=width,
        label="without bypass: CPU scalar",
        color=COLORS["cpu"],
    )
    engine_bars = plt.bar(
        [i + width / 2 for i in x],
        [row["engine_cycles"] for row in rows],
        width=width,
        label="with bypass: merge engine",
        color=COLORS["engine"],
    )
    for bar in list(cpu_bars) + list(engine_bars):
        val = bar.get_height()
        plt.text(
            bar.get_x() + bar.get_width() / 2,
            val + 300,
            f"{int(val)}",
            ha="center",
            va="bottom",
            fontsize=8,
            rotation=90,
        )

    plt.title("Online Softmax Merge: Bypass vs No Bypass Cycles")
    plt.xlabel("Benchmark case")
    plt.ylabel("Cycles")
    plt.xticks(x, labels, rotation=30, ha="right")
    plt.grid(axis="y", alpha=0.25)
    plt.legend(fontsize=9)
    plt.tight_layout()
    plt.savefig(out_path, dpi=180)
    plt.close()


def plot_bypass_speedup(rows, out_path: Path):
    labels = [bypass_case_label(row) for row in rows]
    x = list(range(len(labels)))
    speedups = [row["speedup"] for row in rows]

    plt.figure(figsize=(10.8, 5.4))
    bars = plt.bar(x, speedups, color=COLORS["speedup"], width=0.56)
    plt.axhline(1.0, color="#444444", linestyle="--", linewidth=1.2, label="break-even")
    for bar, speedup in zip(bars, speedups):
        plt.text(
            bar.get_x() + bar.get_width() / 2,
            speedup + 0.05,
            f"{speedup:.2f}x",
            ha="center",
            va="bottom",
            fontsize=9,
        )

    plt.title("Online Softmax Merge: Bypass Speedup")
    plt.xlabel("Benchmark case")
    plt.ylabel("Speedup = CPU cycles / engine cycles")
    plt.xticks(x, labels, rotation=30, ha="right")
    plt.grid(axis="y", alpha=0.25)
    plt.legend(fontsize=9)
    plt.tight_layout()
    plt.savefig(out_path, dpi=180)
    plt.close()


def plot_bypass_runtime_proxy(rows, out_path: Path):
    labels = [bypass_case_label(row) for row in rows]
    x = list(range(len(labels)))
    width = 0.36
    cpu_norm = [1.0 for _ in rows]
    engine_norm = [row["engine_cycles"] / row["cpu_cycles"] for row in rows]

    plt.figure(figsize=(10.8, 5.4))
    plt.bar(
        [i - width / 2 for i in x],
        cpu_norm,
        width=width,
        label="without bypass",
        color=COLORS["cpu"],
    )
    bars = plt.bar(
        [i + width / 2 for i in x],
        engine_norm,
        width=width,
        label="with bypass",
        color=COLORS["engine"],
    )
    for bar, val in zip(bars, engine_norm):
        plt.text(
            bar.get_x() + bar.get_width() / 2,
            val + 0.03,
            f"{val:.2f}",
            ha="center",
            va="bottom",
            fontsize=9,
        )

    plt.title("Online Softmax Merge: Runtime Proxy at Fixed Frequency")
    plt.xlabel("Benchmark case")
    plt.ylabel("Normalized runtime, no bypass = 1.0")
    plt.xticks(x, labels, rotation=30, ha="right")
    plt.grid(axis="y", alpha=0.25)
    plt.legend(fontsize=9)
    plt.tight_layout()
    plt.savefig(out_path, dpi=180)
    plt.close()


def plot_bypass_tcdm(rows, out_path: Path):
    labels = [bypass_case_label(row) for row in rows]
    x = list(range(len(labels)))
    width = 0.36

    plt.figure(figsize=(11.2, 5.6))
    accessed_bars = plt.bar(
        [i - width / 2 for i in x],
        [row["tcdm_accessed"] for row in rows],
        width=width,
        label="TCDM accessed",
        color=COLORS["accessed"],
    )
    congested_bars = plt.bar(
        [i + width / 2 for i in x],
        [row["tcdm_congested"] for row in rows],
        width=width,
        label="TCDM congested",
        color=COLORS["congested"],
    )
    for bar in list(accessed_bars) + list(congested_bars):
        val = bar.get_height()
        if val == 0:
            continue
        plt.text(
            bar.get_x() + bar.get_width() / 2,
            val + 70,
            f"{int(val)}",
            ha="center",
            va="bottom",
            fontsize=8,
            rotation=90,
        )

    plt.title("Online Softmax Merge: Engine TCDM Counters")
    plt.xlabel("Benchmark case")
    plt.ylabel("Counter value")
    plt.xticks(x, labels, rotation=30, ha="right")
    plt.grid(axis="y", alpha=0.25)
    plt.legend(fontsize=9)
    plt.tight_layout()
    plt.savefig(out_path, dpi=180)
    plt.close()


def plot_bypass_break_even(rows, out_path: Path):
    filtered = sorted(
        [row for row in rows if row["case_id"] == 2],
        key=lambda row: (row["N"] * row["D"], row["N"], row["D"]),
    )
    x = [row["N"] * row["D"] for row in filtered]
    speedups = [row["speedup"] for row in filtered]
    labels = [f"N{row['N']} D{row['D']}" for row in filtered]

    plt.figure(figsize=(8.8, 5.2))
    plt.plot(x, speedups, marker="o", linewidth=2.0, color=COLORS["speedup"])
    plt.axhline(1.0, color="#444444", linestyle="--", linewidth=1.2, label="break-even")
    for xpos, speedup, label in zip(x, speedups, labels):
        plt.text(
            xpos,
            speedup + 0.05,
            f"{label}\n{speedup:.2f}x",
            ha="center",
            va="bottom",
            fontsize=8,
        )

    plt.title("Online Softmax Merge: Break-Even Sweep for Case 2")
    plt.xlabel("Vector elements processed (N x D)")
    plt.ylabel("Speedup = CPU cycles / engine cycles")
    plt.grid(alpha=0.25)
    plt.legend(fontsize=9)
    plt.tight_layout()
    plt.savefig(out_path, dpi=180)
    plt.close()


def draw_box(ax, xy, width, height, text, facecolor):
    box = Rectangle(
        xy,
        width,
        height,
        linewidth=1.4,
        edgecolor="#333333",
        facecolor=facecolor,
    )
    ax.add_patch(box)
    ax.text(
        xy[0] + width / 2,
        xy[1] + height / 2,
        text,
        ha="center",
        va="center",
        fontsize=10,
        wrap=True,
    )


def draw_arrow(ax, start, end):
    arrow = FancyArrowPatch(
        start,
        end,
        arrowstyle="-|>",
        mutation_scale=14,
        linewidth=1.4,
        color="#333333",
    )
    ax.add_patch(arrow)


def plot_engine_flow(out_path: Path):
    states = [
        "IDLE",
        "LOAD\nSCALAR",
        "COMPUTE\nSCALAR",
        "STORE\nSCALAR",
        "UPDATE\nVECTOR",
        "DONE",
    ]
    plt.figure(figsize=(11.4, 3.0))
    ax = plt.gca()
    ax.set_xlim(0, 12)
    ax.set_ylim(0, 3)
    ax.axis("off")

    x0 = 0.3
    width = 1.55
    gap = 0.38
    for i, state in enumerate(states):
        x = x0 + i * (width + gap)
        draw_box(ax, (x, 1.45), width, 0.75, state, "#dbe9f6")
        if i > 0:
            prev_x = x0 + (i - 1) * (width + gap)
            draw_arrow(ax, (prev_x + width, 1.82), (x, 1.82))

    draw_box(ax, (4.55, 0.28), 2.9, 0.65, "ERROR", "#f4cccc")
    ax.text(
        6.0,
        1.08,
        "invalid config or unsupported scalar merge",
        ha="center",
        va="center",
        fontsize=9,
        color="#555555",
    )
    draw_arrow(ax, (2.9, 1.45), (5.2, 0.93))
    draw_arrow(ax, (6.1, 1.45), (6.25, 0.93))

    plt.title("Streaming Merge-Update Engine Control Flow")
    plt.tight_layout()
    plt.savefig(out_path, dpi=180)
    plt.close()


def plot_cluster_integration(out_path: Path):
    plt.figure(figsize=(10.8, 5.4))
    ax = plt.gca()
    ax.set_xlim(0, 10)
    ax.set_ylim(0, 6)
    ax.axis("off")

    draw_box(ax, (0.55, 4.55), 2.0, 0.75, "Core TCDM\nports", "#dbe9f6")
    draw_box(ax, (4.0, 4.55), 2.0, 0.75, "AXI-to-TCDM\nport", "#e2f0d9")
    draw_box(ax, (7.2, 4.55), 2.25, 0.75, "Merge engine\nTCDM master", "#fce4d6")
    draw_box(ax, (3.0, 2.75), 4.0, 0.82, "Spatz narrow TCDM interconnect", "#fff2cc")
    draw_box(ax, (3.15, 1.0), 3.7, 0.82, "Cluster TCDM banks", "#eadcf8")
    draw_box(ax, (7.2, 2.2), 2.25, 0.75, "MMIO MERGE_*\nregisters", "#eeeeee")

    draw_arrow(ax, (1.55, 4.55), (3.35, 3.57))
    draw_arrow(ax, (5.0, 4.55), (5.0, 3.57))
    draw_arrow(ax, (8.32, 4.55), (6.65, 3.57))
    draw_arrow(ax, (5.0, 2.75), (5.0, 1.82))
    draw_arrow(ax, (8.32, 2.95), (8.32, 4.55))

    ax.text(
        8.75,
        3.65,
        "start,\nclear_done,\nbusy/done/error",
        ha="center",
        va="center",
        fontsize=9,
        color="#555555",
    )

    plt.title("Cluster-Local Integration of the Merge Engine")
    plt.tight_layout()
    plt.savefig(out_path, dpi=180)
    plt.close()


def main():
    base_dir = Path(__file__).resolve().parent.parent
    data_dir = base_dir / "data"
    out_dir = base_dir / "pic"
    out_dir.mkdir(parents=True, exist_ok=True)

    software_rows = load_software_rows(data_dir / "attnres_software_baselines.csv")
    bypass_rows = load_bypass_rows(data_dir / "online_softmax_merge_bypass.csv")

    plot_software_traffic(software_rows, out_dir / "attnres_software_traffic.png")
    plot_software_error(software_rows, out_dir / "attnres_software_error.png")
    plot_bypass_cycles(bypass_rows, out_dir / "online_softmax_merge_bypass_cycles.png")
    plot_bypass_speedup(bypass_rows, out_dir / "online_softmax_merge_bypass_speedup.png")
    plot_bypass_runtime_proxy(
        bypass_rows,
        out_dir / "online_softmax_merge_bypass_runtime_proxy.png",
    )
    plot_bypass_tcdm(bypass_rows, out_dir / "online_softmax_merge_bypass_tcdm.png")
    plot_bypass_break_even(
        bypass_rows,
        out_dir / "online_softmax_merge_bypass_break_even.png",
    )
    plot_engine_flow(out_dir / "online_softmax_merge_engine_flow.png")
    plot_cluster_integration(out_dir / "online_softmax_merge_cluster_integration.png")

    print(f"Generated AttnRes plots in: {out_dir}")


if __name__ == "__main__":
    main()
