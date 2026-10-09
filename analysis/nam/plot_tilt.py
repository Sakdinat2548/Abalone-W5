"""Throwaway: fitted hardware tilt curve + ChainTest measured points."""
import sys

sys.path.insert(0, r"C:\Users\<you>\Code\Abalone-W5\analysis")
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from ir_check import rbj_cook

FS = 48000.0
GRID = np.logspace(np.log10(10), np.log10(20000), 400)


def shelf(f, typ, f0, g):
    w = 2 * np.pi * np.asarray(f) / FS
    z = np.exp(1j * w)
    b0, b1, b2, a1, a2 = rbj_cook(typ, f0, 0.71, g, FS)
    h = (b0 + b1 / z + b2 / z / z) / (1.0 + a1 / z + a2 / z / z)
    return 20 * np.log10(np.abs(h))


tilt = shelf(GRID, "ls", 120.0, 0.7) + shelf(GRID, "hs", 8000.0, -0.5)


def hp(f, fc):
    a = 1.0 - np.exp(-2 * np.pi * fc / FS)
    w = 2 * np.pi * np.asarray(f) / FS
    z = np.exp(-1j * w)
    h = 1.0 - a / (1.0 - (1.0 - a) * z)
    return 20 * np.log10(np.abs(h))


# measured engaged-flat minus 3.0 dB boost (48 kHz, from ChainTest log)
pts = [(20, 0.398), (100, 0.463), (1000, 0.002), (5000, -0.050),
       (10000, -0.377), (15000, -0.487)]
full = tilt + hp(GRID, 5.0) + hp(GRID, 2.0)

fig, ax = plt.subplots(figsize=(11, 5))
ax.semilogx(GRID, tilt, color="#39d353", lw=1.6, label="fitted tilt (LS120 +0.7 / HS8k -0.5)")
ax.semilogx(GRID, full, color="#00bfff", lw=1.2, ls="--", label="+ both DC-blockers (full tail)")
ax.plot([p[0] for p in pts], [p[1] for p in pts], "o", color="#ff7b00", ms=5,
        label="measured chain - 3 dB boost")
ax.set_xlim(10, 20000)
ax.set_xlabel("Hz")
ax.set_ylabel("dB")
ax.set_title("Hardware tilt: fit vs measured engaged chain")
ax.grid(True, which="both", alpha=0.4)
ax.legend(fontsize=9)
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt.png", dpi=110)
print("wrote tilt.png")
