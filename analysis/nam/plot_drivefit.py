"""Throwaway: fitted (k=0.06,a=1.5e-3) vs current vs captures, 440 Hz ladders."""
import sys

sys.path.insert(0, r"C:\Users\<you>\AppData\Local\Temp\opencode")
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from nam_numpy import NAME

FS, N, F0 = 48000.0, 48000, 440.0
K_FIT, A_FIT = 0.06, 1.5e-3
K_CUR, A_CUR = 0.03, 6e-4


def series_at(color_db, k, a):
    amp = 10 ** (color_db / 20.0)
    xs = amp * np.sin(2 * np.pi * F0 * np.arange(N) / FS)
    ys = np.tanh(k * xs) / np.tanh(k) + a * xs * xs
    return ys - ys.mean()


def spectrum(y):
    y = np.asarray(y, dtype=float)
    y -= np.mean(y)
    w = np.hanning(len(y))
    spec = np.abs(np.fft.rfft(y * w)) / (len(y) / 4)
    f = np.fft.rfftfreq(len(y), 1.0 / FS)
    db = 20 * np.log10(np.maximum(spec, 1e-12))
    return f, db - db[440]


caps = {
    "B3 (-10 in, color -1)": (r"C:\Users\<you>\Downloads\Avalon U5 Boost3 MicOut No EQ.nam", -1.0),
    "B7 (-10 in, color +11)": (r"C:\Users\<you>\Downloads\Avalon U5 Boost7(Unity) MicOut No EQ.nam", 11.0),
    "DI (-10 in, color -4)": (r"C:\Users\<you>\Downloads\Avalon U5 DI Preamplifier.nam", -4.0),
}
x = 10 ** (-10.0 / 20.0) * np.sin(2 * np.pi * F0 * np.arange(N) / FS)

fig, axes = plt.subplots(1, 3, figsize=(16, 5), sharey=True)
for ax, (tag, (path, cdb)) in zip(axes, caps.items()):
    f, db = spectrum(NAME(path).forward(x))
    ax.semilogx(f[1:], db[1:], color="#ff7b00", lw=1.4, label="NAM")
    f, db = spectrum(series_at(cdb, K_FIT, A_FIT))
    ax.semilogx(f[1:], db[1:], color="#39d353", lw=1.4, label="fitted k/a")
    f, db = spectrum(series_at(cdb, K_CUR, A_CUR))
    ax.semilogx(f[1:], db[1:], color="#8b949e", lw=1.2, ls="--", label="current")
    ax.set_xlim(100, 20000)
    ax.set_ylim(-120, 5)
    ax.set_xlabel("Hz")
    ax.set_title(tag)
    ax.grid(True, which="both", alpha=0.35)
    ax.legend(fontsize=8)
    for h in (1, 2, 3, 4):
        ax.axvline(F0 * h, color="#555555", lw=0.8, ls=":", alpha=0.6)
        ax.annotate("H%d" % h, (F0 * h, 2), fontsize=7, ha="center", color="#555555")
axes[0].set_ylabel("dB rel H1")
fig.suptitle("Fitted drive vs captures vs current (440 Hz, H1-normalized)")
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\drivefit.png", dpi=110)
print("wrote drivefit.png")
