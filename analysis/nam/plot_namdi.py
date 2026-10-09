"""Throwaway: DI capture (Boost2, no tone) vs our Boost2/tone-bypass — FR + harmonics."""
import subprocess
import sys

sys.path.insert(0, r"C:\Users\<you>\AppData\Local\Temp\opencode")
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from nam_numpy import NAME

freqs = np.logspace(np.log10(20.0), np.log10(20000.0), 25)
exe = r"C:\Users\<you>\AppData\Local\Temp\opencode\namcmp_probe.exe"
freq_arg = "\n".join("%.1f" % f for f in freqs) + "\n"
p = subprocess.run([exe, "2"], input=freq_arg, capture_output=True, text=True,
                   cwd=r"C:\Users\<you>\Code\Abalone-W5")
our = np.array([[float(a), float(b)] for a, b in
                (l.split() for l in p.stdout.strip().split("\n"))])

m = NAME(r"C:\Users\<you>\Downloads\Avalon U5 DI Preamplifier.nam")
nam = []
for f in freqs:
    n = 48000
    x = 10 ** (-20.0 / 20.0) * np.sin(2 * np.pi * f * np.arange(n) / 48000.0)
    y = m.forward(x)
    t = slice(n // 2, n)
    nam.append(10 * np.log10(float(np.mean(y[t] ** 2) / np.mean(x[t] ** 2))))
nam = np.array(nam)

N = 48000
x = 10 ** (-10.0 / 20.0) * np.sin(2 * np.pi * 440.0 * np.arange(N) / 48000.0)
nam_raw = m.forward(x)


def spectrum(y):
    y = np.asarray(y, dtype=float)
    y -= np.mean(y)
    w = np.hanning(len(y))
    spec = np.abs(np.fft.rfft(y * w)) / (len(y) / 4)
    f = np.fft.rfftfreq(len(y), 1.0 / 48000.0)
    db = 20 * np.log10(np.maximum(spec, 1e-12))
    return f, db - db[440]


fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
i1k = int(np.argmin(np.abs(freqs - 1000.0)))
ax1.semilogx(freqs, nam, color="#ff7b00", lw=1.5, label="NAM DI/Boost2")
ax1.semilogx(our[:, 0], our[:, 1], color="#00bfff", lw=1.2, ls="--", label="Ours Boost2")
ax1.set_xlim(20, 20000)
ax1.set_xlabel("Hz")
ax1.set_ylabel("gain dB (absolute)")
ax1.set_title("DI capture vs Abalone Boost 2 — FR")
ax1.grid(True, which="both", alpha=0.35)
ax1.legend(fontsize=8)

f, db = spectrum(nam_raw)
ax2.semilogx(f[1:], db[1:], color="#ff7b00", lw=1.2, label="NAM DI/Boost2")
ax2.set_xlim(100, 20000)
ax2.set_ylim(-120, 5)
ax2.set_xlabel("Hz")
ax2.set_ylabel("dB rel H1")
ax2.set_title("440 Hz -10 dBFS harmonics, H1-normalized")
ax2.grid(True, which="both", alpha=0.35)
ax2.legend(fontsize=8)
for h in (1, 2, 3, 4, 5):
    ax2.axvline(440.0 * h, color="#555555", lw=0.8, ls=":", alpha=0.6)
    ax2.annotate("H%d" % h, (440.0 * h, 2), fontsize=7, ha="center", color="#555555")
fig.suptitle("Clean DI reference vs Abalone (real DSP both sides)")
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\namdi.png", dpi=110)

d = (nam - nam[i1k]) - (our[:, 1] - our[i1k, 1])
print("DI gain-matched shape: max %.3f @ %.0f Hz, rms %.3f" % (
    np.max(np.abs(d)), freqs[np.argmax(np.abs(d))], float(np.sqrt(np.mean(d ** 2)))))
print("NAM DI gain @1k: %+.2f | OURS B2 @1k: %+.2f" % (nam[i1k], our[i1k, 1]))

y = np.asarray(nam_raw, dtype=float)
y -= y.mean()


def harm(h):
    a = 2 * np.pi * 440.0 * h * np.arange(N) / 48000.0
    return 2 * np.hypot(np.sum(y * np.cos(a)), np.sum(y * np.sin(a))) / N


h1 = harm(1)
print("NAM DI H1 %+.1fdBFS |" % (20 * np.log10(h1)), end="")
for h in (2, 3, 4, 5):
    print(" H%d %+.1f" % (h, 20 * np.log10(harm(h) / h1)), end="")
print()
print("wrote namdi.png")
