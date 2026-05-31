#!/usr/bin/env python3
import csv
from pathlib import Path

import matplotlib.pyplot as plt


COLORS = {
    "naive-full-recompute": "#4c78a8",
    "paper-two-phase": "#f58518",
    "software-fusion": "#54a24b",
    "cpu": "#4c78a8",
    "engine": "#e45756",
    "speedup": "#72b7b2",
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

    print(f"Generated AttnRes plots in: {out_dir}")


if __name__ == "__main__":
    main()
