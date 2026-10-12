"""Regenerate docs/dynamics.png from LIVE chain transfer (real DSP).

Runs the DynDump docs tool (cmake --build build --target DynDump first),
which sweeps steady 1 kHz sine peaks -60..0 dBFS per boost step through the
real ProcessorChain (tone bypass, trim -3N unity staging; see
analysis/dyn_dump.cpp for the fixed stimulus). Same commit as any DSP
change that moves transfer shapes. numpy + matplotlib only.
"""
import os
import subprocess
import sys

import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CANDIDATES = [
    os.path.join(REPO, "build", "DynDump.exe"),
    os.path.join(REPO, "build", "DynDump"),
]


def load_rows():
    for exe in CANDIDATES:
        if os.path.isfile(exe):
            break
    else:
        sys.exit("DynDump not found: run 'cmake --build build --target DynDump' first")
    out = subprocess.run([exe], capture_output=True, text=True, check=True).stdout
    rows = []
    for line in out.splitlines():
        line = line.strip()
        if not line or line.startswith("inDb"):
            continue
        rows.append([float(v) for v in line.split(",")])
    return np.array(rows)


def main():
    data = load_rows()
    xs = data[:, 0]
    curves = {
        "Boost 1 (+3 dB)": (data[:, 1], "#17becf"),
        "Boost 3 (+9 dB)": (data[:, 2], "#1f77b4"),
        "Boost 6 (+18 dB)": (data[:, 3], "#2ca02c"),
        "Boost 8 (+24 dB)": (data[:, 4], "#9467bd"),
        "Boost 10 (+30 dB)": (data[:, 5], "#ff7f0e"),
    }

    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(8, 7))
    ax.plot(xs, xs, color="gray", lw=1.2, ls="--", label="linear")
    box = ["0 dB in:"]
    for name, (ys, color) in curves.items():
        ax.plot(xs, ys, color=color, lw=1.4, label=name)
        short = name.split()[1]
        at0 = float(ys[-1])
        droop = xs - ys  # >= 0 where saturation bites
        knee = next((float(x) for x, d in zip(xs, droop) if d >= 1.0), None)
        line = "B%s %+.2f" % (short, at0)
        if knee is not None:
            line += ", knee ~%.0f" % knee
        box.append(line)
    ax.set_title("Abalone W5 dynamics — transfer curves", fontsize=13)
    ax.set_xlabel("input peak (dBFS)")
    ax.set_ylabel("output peak (dB, unity trim per curve)")
    ax.set_xlim(-60, 0)
    ax.set_ylim(-60, 0)
    ax.grid(True, alpha=0.4)
    ax.legend(loc="lower right", fontsize=9)
    ax.text(
        0.04,
        0.78,
        "\n".join(box),
        transform=ax.transAxes,
        fontsize=9,
        family="monospace",
        va="top",
        bbox=dict(facecolor="white", edgecolor="#cccccc", alpha=0.9),
    )
    fig.tight_layout()
    fig.savefig(os.path.join(REPO, "docs", "dynamics.png"), dpi=110)
    print("wrote docs/dynamics.png")


main()
