#!/usr/bin/env python3
import csv
from pathlib import Path

import matplotlib.pyplot as plt


TYPE_LABELS = {
    "complex": "dp-caxpy (complex)",
    "non-complex": "dp-faxpy (non-complex)",
}


def load_rows(csv_path: Path):
    rows = []
    with csv_path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(
                {
                    "type": row["type"],
                    "test_name": row["test_name"],
                    "M": int(row["M"]),
                    "test_time_sec": float(row["test_time_sec"]),
                    "max_cycle": int(row["max_cycle"]),
                }
            )
    rows.sort(key=lambda x: (x["M"], x["type"]))
    return rows


def split_by_type(rows):
    grouped = {}
    for row in rows:
        grouped.setdefault(row["type"], {})[row["M"]] = row
    return grouped


def plot_time(rows, out_path: Path):
    grouped = split_by_type(rows)
    m_values = sorted({r["M"] for r in rows})
    x = list(range(len(m_values)))
    width = 0.35

    plt.figure(figsize=(8, 5))
    bars_a = plt.bar(
        [i - width / 2 for i in x],
        [grouped["non-complex"][m]["test_time_sec"] for m in m_values],
        width=width,
        color="#1f77b4",
        label=TYPE_LABELS["non-complex"],
    )
    bars_b = plt.bar(
        [i + width / 2 for i in x],
        [grouped["complex"][m]["test_time_sec"] for m in m_values],
        width=width,
        color="#ff7f0e",
        label=TYPE_LABELS["complex"],
    )
    plt.title("AXPY Time Comparison (Complex vs Non-Complex)")
    plt.xlabel("M")
    plt.ylabel("Time (sec)")
    plt.xticks(x, m_values)
    plt.grid(axis="y", alpha=0.3)
    plt.legend()
    for bar in list(bars_a) + list(bars_b):
        val = bar.get_height()
        plt.text(bar.get_x() + bar.get_width() / 2, val + 0.01, f"{val:.2f}", ha="center", va="bottom")
    plt.tight_layout()
    plt.savefig(out_path, dpi=160)
    plt.close()


def plot_cycles(rows, out_path: Path):
    grouped = split_by_type(rows)
    m_values = sorted({r["M"] for r in rows})
    x = list(range(len(m_values)))
    width = 0.35

    plt.figure(figsize=(8, 5))
    bars_a = plt.bar(
        [i - width / 2 for i in x],
        [grouped["non-complex"][m]["max_cycle"] for m in m_values],
        width=width,
        color="#2ca02c",
        label=TYPE_LABELS["non-complex"],
    )
    bars_b = plt.bar(
        [i + width / 2 for i in x],
        [grouped["complex"][m]["max_cycle"] for m in m_values],
        width=width,
        color="#d62728",
        label=TYPE_LABELS["complex"],
    )
    plt.title("AXPY Cycle Comparison (Complex vs Non-Complex)")
    plt.xlabel("M")
    plt.ylabel("Cycle")
    plt.xticks(x, m_values)
    plt.grid(axis="y", alpha=0.3)
    plt.legend()
    for bar in list(bars_a) + list(bars_b):
        val = bar.get_height()
        plt.text(bar.get_x() + bar.get_width() / 2, val + 20, f"{val}", ha="center", va="bottom")
    plt.tight_layout()
    plt.savefig(out_path, dpi=160)
    plt.close()


def main():
    base_dir = Path(__file__).resolve().parent
    csv_path = base_dir / "results_axpy_compare.csv"
    out_dir = base_dir / "pic"
    out_dir.mkdir(parents=True, exist_ok=True)

    rows = load_rows(csv_path)
    plot_time(rows, out_dir / "axpy_compare_time.png")
    plot_cycles(rows, out_dir / "axpy_compare_cycles.png")
    print(f"Generated plots in: {out_dir}")


if __name__ == "__main__":
    main()
