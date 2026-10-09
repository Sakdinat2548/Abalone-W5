"""Throwaway plot: 5 kHz @ B10 alias census, OS 1x vs 2x vs 4x (real chain)."""
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

FS = 48000.0
blocks = {}
label = None
with open(r"C:\Users\<you>\AppData\Local\Temp\opencode\os.txt") as f:
    for line in f:
        line = line.strip()
        if line.startswith("#"):
            label = line[2:]
            blocks[label] = []
        elif line:
            blocks[label].append(float(line))

fig, (ax1, ax2, ax4) = plt.subplots(1, 3, figsize=(16, 5), sharey=True)
for label, color, ax, title in [("os1x", "#ff7b00", ax1, "1x — aliases exposed"),
                                ("os2x", "#39d353", ax2, "2x — aliases pushed down"),
                                ("os4x", "#00bfff", ax4, "4x — aliases gone")]:
    y = np.asarray(blocks[label])
    y -= np.mean(y)
    w = np.hanning(len(y))
    spec = np.abs(np.fft.rfft(y * w)) / (len(y) / 4)
    freqs = np.fft.rfftfreq(len(y), 1.0 / FS)
    db = 20 * np.log10(np.maximum(spec, 1e-12))
    ax.semilogx(freqs[1:], db[1:], color=color, lw=1.2)
    ax.set_xlim(20, 20000)
    ax.set_ylim(-120, 10)
    ax.set_xlabel("Hz")
    ax.set_title(title)
    ax.grid(True, which="both", alpha=0.35)
    for x, tag in [(5000, "H1"), (10000, "H2"), (15000, "H3"), (20000, "H4")]:
        ax.annotate(tag, (x, 4), fontsize=7, ha="center", color="#8b949e")
        ax.axvline(x, color="#8b949e", lw=0.8, alpha=0.6)
    for x, tag, ty in [(8000, "H8", 8.0), (13000, "H7", -4.0), (18000, "H6", 8.0)]:
        ax.annotate(tag, (x, ty), fontsize=7, ha="center", color="#ff0000")
        ax.axvline(x, color="#ff0000", lw=0.8, ls=":", alpha=0.7)
ax1.set_ylabel("dBFS")
fig.suptitle("5 kHz @ Boost 10, -10 dB RMS — oversampling vs aliases (real chain)")
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\os.png", dpi=110)
print("wrote os.png")
