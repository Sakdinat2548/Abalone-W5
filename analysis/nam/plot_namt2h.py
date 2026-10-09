"""Throwaway: NAM TONE2/Boost4 harmonic spectrum, 440 Hz -10 dBFS."""
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

y = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\namt2h.txt")
y -= np.mean(y)
w = np.hanning(len(y))
spec = np.abs(np.fft.rfft(y * w)) / (len(y) / 4)
f = np.fft.rfftfreq(len(y), 1.0 / 48000.0)
db = 20 * np.log10(np.maximum(spec, 1e-12))
db -= db[440]

fig, ax = plt.subplots(figsize=(12, 6))
ax.semilogx(f[1:], db[1:], color="#ff7b00", lw=1.2)
ax.fill_between(f[1:], db[1:], -120, color="#ff7b00", alpha=0.18)
ax.set_xlim(100, 20000)
ax.set_ylim(-100, 10)
ax.set_xlabel("Hz")
ax.set_ylabel("dB rel H1")
ax.set_title("Avalon U5 capture: TONE 2 @ Boost4 — 440 Hz -10 dBFS harmonics")
ax.grid(True, which="both", alpha=0.35)
for h in (1, 2, 3, 4, 5, 6):
    ax.axvline(440.0 * h, color="#555555", lw=0.8, ls=":", alpha=0.6)
    ax.annotate("H%d" % h, (440.0 * h, -4), fontsize=8, ha="center", color="#555555")
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\namt2h.png", dpi=110)
print("wrote namt2h.png")
