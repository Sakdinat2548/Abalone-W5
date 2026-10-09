"""Throwaway: NAM TONE2-Boost4 vs our Tone2/Boost4 — FR + harmonic spectra."""
import subprocess
import sys

sys.path.insert(0, r"C:\Users\<you>\AppData\Local\Temp\opencode")
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from nam_numpy import NAME

freqs = np.logspace(np.log10(20.0), np.log10(20000.0), 25)
exe = r"C:\Users\<you>\AppData\Local\Temp\opencode\namt2_probe.exe"
freq_arg = "\n".join("%.1f" % f for f in freqs) + "\nRAW\n"
p = subprocess.run([exe], input=freq_arg, capture_output=True, text=True,
                   cwd=r"C:\Users\<you>\Code\Abalone-W5")
fr_lines, raw, mode = [], [], "fr"
for line in p.stdout.split("\n"):
    line = line.strip()
    if line == "# ourT2B4":
        mode = "raw"
        continue
    if not line:
        continue
    (fr_lines if mode == "fr" else raw).append(line)
our_fr = np.array([[float(a), float(b)] for a, b in (l.split() for l in fr_lines)])
our_raw = np.array([float(v) for v in raw])

m = NAME(r"C:\Users\<you>\Downloads\Avalon U5 TONE 2 Boost4.nam")
nam_fr = []
for f in freqs:
    n = 48000
    x = 10 ** (-20.0 / 20.0) * np.sin(2 * np.pi * f * np.arange(n) / 48000.0)
    y = m.forward(x)
    t = slice(n // 2, n)
    nam_fr.append(10 * np.log10(float(np.mean(y[t] ** 2) / np.mean(x[t] ** 2))))
nam_fr = np.array(nam_fr)

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


np.savetxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\namt2_fr.csv",
           np.column_stack([freqs, nam_fr, our_fr[:, 1]]),
           delimiter=",", header="freq,NAM,OURS", comments="")

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
i1k = int(np.argmin(np.abs(freqs - 1000.0)))
ax1.semilogx(freqs, nam_fr, color="#ff7b00", lw=1.5, label="NAM T2/B4")
ax1.semilogx(our_fr[:, 0], our_fr[:, 1], color="#00bfff", lw=1.2, ls="--", label="Ours T2/B4")
ax1.set_xlim(20, 20000)
ax1.set_xlabel("Hz")
ax1.set_ylabel("gain dB (absolute)")
ax1.set_title("Tone 2 @ Boost 4 — capture vs Abalone FR")
ax1.grid(True, which="both", alpha=0.35)
ax1.legend(fontsize=8)

for y, c, ls, lab in [(nam_raw, "#ff7b00", "-", "NAM T2/B4"), (our_raw, "#00bfff", "--", "Ours T2/B4")]:
    f, db = spectrum(y)
    ax2.semilogx(f[1:], db[1:], color=c, lw=1.2, ls=ls, label=lab)
ax2.set_xlim(100, 20000)
ax2.set_ylim(-120, 5)
ax2.set_xlabel("Hz")
ax2.set_ylabel("dB rel H1")
ax2.set_title("440 Hz -10 dBFS harmonics, H1-normalized")
ax2.grid(True, which="both", alpha=0.35)
ax2.legend(fontsize=8)
for h in (1, 2, 3, 4):
    ax2.axvline(440.0 * h, color="#555555", lw=0.8, ls=":", alpha=0.6)
    ax2.annotate("H%d" % h, (440.0 * h, 2), fontsize=7, ha="center", color="#555555")
fig.suptitle("Tone 2 retune vs hardware capture (real DSP both sides)")
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\namt2.png", dpi=110)

d = (nam_fr - nam_fr[i1k]) - (our_fr[:, 1] - our_fr[i1k, 1] if our_fr.ndim else 0)
d = (nam_fr - nam_fr[i1k]) - (our_fr[:, 1] - our_fr[i1k, 0] * 0 - our_fr[i1k, 1])
print("gain-matched shape: max %.3f @ %.0f Hz, rms %.3f" % (
    np.max(np.abs(d)), freqs[np.argmax(np.abs(d))], float(np.sqrt(np.mean(d ** 2)))))
print("wrote namt2.png")
