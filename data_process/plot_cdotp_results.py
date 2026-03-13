#!/usr/bin/env python3
import csv
from pathlib import Path

import matplotlib.pyplot as plt


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
                    "M": int(row["M"]),
                    "status": row["status"],
                    "test_time_sec": float(row["test_time_sec"]),
                    "max_cycle": int(row["max_cycle"]),
                }
            )
    rows.sort(key=lambda x: (x["M"], x["type"]))
    return rows


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


def main():
    base_dir = Path(__file__).resolve().parent
    out_dir = base_dir / "pic"
    out_dir.mkdir(parents=True, exist_ok=True)

    complex_rows = load_complex_rows(base_dir / "results_cdotp.csv")
    compare_rows = load_compare_rows(base_dir / "results_dotp_compare.csv")

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
