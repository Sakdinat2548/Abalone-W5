"""Throwaway: NAM-vs-ours deltas (3 captures) vs fitted tilt."""
import subprocess
import sys

sys.path.insert(0, r"C:\Users\<you>\AppData\Local\Temp\opencode")
sys.path.insert(0, r"C:\Users\<you>\Code\Abalone-W5\analysis")
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from nam_numpy import NAME
from ir_check import rbj_cook

FS = 48000.0
d = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\namcmp.csv",
               delimiter=",", skiprows=1)
fr, nb3, nb7, ob3, ob7 = d.T

freqs = np.logspace(np.log10(20.0), np.log10(20000.0), 25)
p = subprocess.run(
    [r"C:\Users\<you>\AppData\Local\Temp\opencode\namcmp_probe.exe", "2"],
    input="\n".join("%.1f" % f for f in freqs) + "\n",
    capture_output=True, text=True, cwd=r"C:\Users\<you>\Code\Abalone-W5")
our2 = np.array([float(l.split()[1]) for l in p.stdout.strip().split("\n")])
m = NAME(r"C:\Users\<you>\Downloads\Avalon U5 DI Preamplifier.nam")
nam = []
for f in freqs:
    n = 48000
    x = 10 ** (-20.0 / 20.0) * np.sin(2 * np.pi * f * np.arange(n) / 48000.0)
    y = m.forward(x)
    t = slice(n // 2, n)
    nam.append(10 * np.log10(float(np.mean(y[t] ** 2) / np.mean(x[t] ** 2))))
nam = np.array(nam)
np.savetxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\namdi_fr.csv",
           np.column_stack([freqs, nam, our2]), delimiter=",",
           header="freq,NAM,OURS", comments="")


def shelf(f, typ, f0, g):
    w = 2 * np.pi * np.asarray(f) / FS
    z = np.exp(1j * w)
    b0, b1, b2, a1, a2 = rbj_cook(typ, f0, 0.71, g, FS)
    h = (b0 + b1 / z + b2 / z / z) / (1.0 + a1 / z + a2 / z / z)
    return 20 * np.log10(np.abs(h))


GRID = np.logspace(np.log10(10), np.log10(20000), 400)
tilt = shelf(GRID, "ls", 120.0, 0.7) + shelf(GRID, "hs", 8000.0, -0.5)

fig, ax = plt.subplots(figsize=(12, 6))
i1k = lambda a: a[np.argmin(np.abs(freqs - 1000.0))] if a.ndim else None
pairs = [((nb3, ob3), "#ff7b00", "B3 N-O"), ((nb7, ob7), "#a371f7", "B7 N-O"),
         ((nam, our2), "#00bfff", "DI N-O")]
for (a, b), c, lab in pairs:
    ia, ib = int(np.argmin(np.abs(freqs - 1000.0))), int(np.argmin(np.abs(freqs - 1000.0)))
    dd = (a - a[ia]) - (b - b[ib])
    ax.semilogx(freqs, dd, color=c, lw=1.3, label="%s (rms %.2f)" % (lab, float(np.sqrt(np.mean(dd ** 2)))))
ax.semilogx(GRID, tilt, color="#39d353", lw=1.8, label="fitted tilt")
ax.axhline(0, color="#555555", lw=0.8)
ax.set_xlim(20, 20000)
ax.set_xlabel("Hz")
ax.set_ylabel("dB (gain-matched @1 kHz)")
ax.set_title("Capture-vs-chain deltas against the fitted tilt")
ax.grid(True, which="both", alpha=0.35)
ax.legend(fontsize=8)
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt_vs_nam.png", dpi=110)
print("wrote tilt_vs_nam.png")
