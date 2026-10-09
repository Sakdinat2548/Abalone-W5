"""Throwaway: median + LS75/HS8k + LS75/HS5441 overlay."""
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

d = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt_hs.txt", skiprows=1)
fr, target, hsnew = d.T
d2 = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt_ls.txt", skiprows=1)
ls75 = d2[:, 2]
fig, ax = plt.subplots(figsize=(12, 6))
ax.semilogx(fr, target, color="#555555", lw=1.2, ls="--", label="median target")
ax.semilogx(fr, ls75, color="#39d353", lw=1.6, label="LS75 + HS8k (rmse 0.073)")
ax.semilogx(fr, hsnew, color="#ff7b00", lw=1.6, label="LS75 + HS5441 (rmse 0.048)")
ax.axhline(0, color="#555555", lw=0.8)
ax.set_xlim(20, 20000)
ax.set_xlabel("Hz")
ax.set_ylabel("dB (gain-matched @1 kHz)")
ax.set_title("Tilt: HS retune vs median target")
ax.grid(True, which="both", alpha=0.35)
ax.legend(fontsize=9)
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt_hsplot.png", dpi=110)
print("wrote tilt_hsplot.png")
