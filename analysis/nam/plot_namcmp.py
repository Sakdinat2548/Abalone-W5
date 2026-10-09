"""Throwaway plot: NAM captures vs our chain (absolute + gain-matched delta)."""
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

d = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\namcmp.csv",
               delimiter=",", skiprows=1)
fr, nb3, nb7, ob3, ob7 = d.T

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
for y, c, ls, lab in [(nb3, "#ff7b00", "-", "NAM Boost3"),
                      (nb7, "#ff7b00", "--", "NAM Boost7"),
                      (ob3, "#00bfff", "-", "Ours Boost3"),
                      (ob7, "#00bfff", "--", "Ours Boost7")]:
    ax1.semilogx(fr, y, color=c, lw=1.4, ls=ls, label=lab)
ax1.set_xlim(20, 20000)
ax1.set_xlabel("Hz")
ax1.set_ylabel("gain dB (absolute)")
ax1.set_title("U5 captures vs Abalone chain — No EQ, -20 dBFS sine")
ax1.grid(True, which="both", alpha=0.35)
ax1.legend(fontsize=8)

i1k = int(np.argmin(np.abs(fr - 1000.0)))
for a, b, c, lab in [(nb3, ob3, "#39d353", "Boost3 N-O delta"),
                     (nb7, ob7, "#a371f7", "Boost7 N-O delta")]:
    dd = (a - a[i1k]) - (b - b[i1k])
    ax2.semilogx(fr, dd, color=c, lw=1.4, label="%s (rms %.2f)" % (lab, float(np.sqrt(np.mean(dd ** 2)))))
ax2.axhline(0, color="#555555", lw=0.8)
ax2.set_xlim(20, 20000)
ax2.set_xlabel("Hz")
ax2.set_ylabel("dB (matched @1 kHz)")
ax2.set_title("Shape difference, gain-matched")
ax2.grid(True, which="both", alpha=0.35)
ax2.legend(fontsize=8)
fig.suptitle("NAM U5 MicOut captures vs Abalone W5 (real DSP both sides)")
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\namcmp.png", dpi=110)
print("wrote namcmp.png")
