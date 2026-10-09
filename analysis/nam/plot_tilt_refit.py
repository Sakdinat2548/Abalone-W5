"""Throwaway: old-vs-new tilt overlay from tilt_refit.txt."""
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

d = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt_refit.txt", skiprows=1)
fr, target, old, new = d.T
fig, ax = plt.subplots(figsize=(12, 6))
ax.semilogx(fr, target, color="#555555", lw=1.2, ls="--", label="median B3/B7/DI target")
ax.semilogx(fr, old, color="#39d353", lw=1.6, label="old LS120+0.7/HS8k-0.5")
ax.semilogx(fr, new, color="#ff7b00", lw=1.6, label="new LS102+0.73/HS5.3k-0.33")
ax.axhline(0, color="#555555", lw=0.8)
ax.set_xlim(20, 20000)
ax.set_xlabel("Hz")
ax.set_ylabel("dB (gain-matched @1 kHz)")
ax.set_title("Tilt refit: old vs new vs median target")
ax.grid(True, which="both", alpha=0.35)
ax.legend(fontsize=9)
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt_refit.png", dpi=110)
print("wrote tilt_refit.png")
