"""Throwaway: tilt candidates overlay."""
import sys

sys.path.insert(0, r"C:\Users\<you>\Code\Abalone-W5\analysis")
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from ir_check import rbj_cook

FS = 48000.0
d = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt_ls.txt", skiprows=1)
fr, target, ls75, pk = d.T
GRID = fr


def resp(sections):
    w = 2 * np.pi * GRID / FS
    z = np.exp(1j * w)
    h = np.ones_like(GRID, dtype=complex)
    for typ, f0, q, g in sections:
        b0, b1, b2, a1, a2 = rbj_cook(typ, f0, q, g, FS)
        h *= (b0 + b1 / z + b2 / z / z) / (1.0 + a1 / z + a2 / z / z)
    return 20 * np.log10(np.abs(h))


stacked = resp([("ls", 25, 0.71, 0.3), ("ls", 100, 0.71, 0.7), ("hs", 8000, 0.71, -0.5)])
current = resp([("ls", 120, 0.71, 0.7), ("hs", 8000, 0.71, -0.5)])

fig, ax = plt.subplots(figsize=(12, 6))
ax.semilogx(GRID, target, color="#555555", lw=1.2, ls="--", label="median target")
ax.semilogx(GRID, current, color="#00bfff", lw=1.2, label="current LS120 (rmse 0.096)")
ax.semilogx(GRID, ls75, color="#39d353", lw=1.6, label="LS75 retune (rmse 0.072)")
ax.semilogx(GRID, stacked, color="#ff7b00", lw=1.6, label="stacked LS25+LS100 (rmse 0.082)")
ax.axhline(0, color="#555555", lw=0.8)
ax.set_xlim(20, 20000)
ax.set_xlabel("Hz")
ax.set_ylabel("dB (gain-matched @1 kHz)")
ax.set_title("Tilt candidates vs median capture target")
ax.grid(True, which="both", alpha=0.35)
ax.legend(fontsize=9)
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt_candidates.png", dpi=110)
print("wrote tilt_candidates.png")
