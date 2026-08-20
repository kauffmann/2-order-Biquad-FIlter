#!/usr/bin/env python3

import csv
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: plot_response.py <csv-path>", file=sys.stderr)
        return 2

    responses = defaultdict(lambda: ([], []))
    with open(sys.argv[1], newline="", encoding="utf-8") as csv_file:
        for row in csv.DictReader(csv_file):
            frequencies, magnitudes = responses[row["filter"]]
            frequencies.append(float(row["frequency_hz"]))
            magnitudes.append(float(row["magnitude_db"]))

    labels = {
        "0": "Low-pass",
        "1": "High-pass",
        "2": "Band-pass",
        "3": "Notch",
        "4": "High-shelf",
        "5": "Low-shelf",
    }

    for filter_id, (frequencies, magnitudes) in responses.items():
        plt.semilogx(frequencies, magnitudes, label=labels.get(filter_id, filter_id))

    plt.title("MultiFilter measured frequency response")
    plt.xlabel("Frequency (Hz)")
    plt.ylabel("Magnitude (dB)")
    plt.grid(True, which="both", alpha=0.3)
    plt.legend()
    plt.tight_layout()
    output_path = Path(sys.argv[1]).with_name("filter_response.png")
    plt.savefig(output_path, dpi=150)
    print(f"Saved plot to {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
