#!/usr/bin/env python3
"""Render a Bode plot (magnitude + phase) from mna_sim CSV output.

Usage:
    python3 plot_bode.py bode_output.csv [output.png] ["Plot Title"]
"""
import sys
import csv
import matplotlib.pyplot as plt


def load_csv(path):
    freqs, mags, phases = [], [], []
    with open(path) as f:
        for row in csv.DictReader(f):
            freqs.append(float(row["frequency_hz"]))
            mags.append(float(row["magnitude_db"]))
            phases.append(float(row["phase_deg"]))
    return freqs, mags, phases


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    csv_path = sys.argv[1]
    out_path = sys.argv[2] if len(sys.argv) > 2 else "bode_plot.png"
    title = sys.argv[3] if len(sys.argv) > 3 else "Frequency Response"

    freqs, mags, phases = load_csv(csv_path)

    fig, (ax_mag, ax_phase) = plt.subplots(2, 1, figsize=(8, 7), sharex=True)

    ax_mag.semilogx(freqs, mags, color="#2b6cb0", linewidth=2)
    ax_mag.axhline(-3, color="gray", linestyle="--", linewidth=1, label="-3 dB")
    ax_mag.set_ylabel("Magnitude (dB)")
    ax_mag.set_title(title)
    ax_mag.grid(True, which="both", linestyle=":", linewidth=0.5)
    ax_mag.legend(loc="best")

    ax_phase.semilogx(freqs, phases, color="#c0392b", linewidth=2)
    ax_phase.set_ylabel("Phase (deg)")
    ax_phase.set_xlabel("Frequency (Hz)")
    ax_phase.grid(True, which="both", linestyle=":", linewidth=0.5)

    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(f"Saved plot to {out_path}")


if __name__ == "__main__":
    main()
