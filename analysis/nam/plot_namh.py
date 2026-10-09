"""Throwaway: NAM vs ours harmonic spectra, 440 Hz -10 dBFS, H1-normalized."""
import sys

sys.path.insert(0, r"C:\Users\<you>\AppData\Local\Temp\opencode")
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from nam_numpy import NAME

FS = 48000.0
N = 48000
x = 10 ** (-10.0 / 20.0) * np.sin(2 * np.pi * 440.0 * np.arange(N) / FS)

blocks = {}
label = None
with open(r"C:\Users\<you>\AppData\Local\Temp\opencode\namh.txt") as f:
    for line in f:
        line = line.strip()
        if line.startswith("#"):
            label = line[2:]
            blocks[label] = []
        elif line:
            blocks[label].append(float(line))

series = {
    "NAM B3": NAME(r"C:\Users\<you>\Downloads\Avalon U5 Boost3 MicOut No EQ.nam").forward(x),
    "NAM B7": NAME(r"C:\Users\<you>\Downloads\Avalon U5 Boost7(Unity) MicOut No EQ.nam").forward(x),
    "OURS B3": np.asarray(blocks["ourB3"]),
    "OURS B7": np.asarray(blocks["ourB7"]),
}


def spectrum(y):
    y = np.asarray(y, dtype=float)
    y -= np.mean(y)
    w = np.hanning(len(y))
    spec = np.abs(np.fft.rfft(y * w)) / (len(y) / 4)
    f = np.fft.rfftfreq(len(y), 1.0 / FS)
    db = 20 * np.log10(np.maximum(spec, 1e-12))
    return f, db - db[440]


fig, (ax3, ax7) = plt.subplots(1, 2, figsize=(14, 5), sharey=True)
for ax, keys, title in [(ax3, ("NAM B3", "OURS B3"), "Boost 3 pair"),
                        (ax7, ("NAM B7", "OURS B7"), "Boost 7 pair")]:
    for k, c, ls in [(keys[0], "#ff7b00", "-"), (keys[1], "#00bfff", "--")]:
        f, db = spectrum(series[k])
        ax.semilogx(f[1:], db[1:], color=c, lw=1.2, ls=ls, label=k)
    ax.set_xlim(100, 20000)
    ax.set_ylim(-120, 5)
    ax.set_xlabel("Hz")
    ax.set_title(title)
    ax.grid(True, which="both", alpha=0.35)
    ax.legend(fontsize=8)
    for h in (1, 2, 3, 4):
        ax.axvline(440.0 * h, color="#555555", lw=0.8, ls=":", alpha=0.6)
        ax.annotate("H%d" % h, (440.0 * h, 2), fontsize=7, ha="center", color="#555555")
ax3.set_ylabel("dB rel H1")
fig.suptitle("440 Hz -10 dBFS harmonics: U5 captures vs Abalone (H1-normalized)")
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\namh.png", dpi=110)
print("wrote namh.png")
