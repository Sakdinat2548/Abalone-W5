"""Throwaway plot: alias census (5 kHz @ B10) + IMD (440+660 @ B10)."""
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

FS = 48000.0
blocks = {}
label = None
with open(r"C:\Users\<you>\AppData\Local\Temp\opencode\blind.txt") as f:
    for line in f:
        line = line.strip()
        if line.startswith("#"):
            if line == "# dc":
                print(line)
                label = "dc"
                continue
            label = line[2:]
            blocks[label] = []
        elif line:
            if label == "dc":
                print(line)
            else:
                blocks[label].append(float(line))


def spectrum(key):
    y = np.asarray(blocks[key])
    y -= np.mean(y)
    w = np.hanning(len(y))
    spec = np.abs(np.fft.rfft(y * w)) / (len(y) / 4)
    return np.fft.rfftfreq(len(y), 1.0 / FS), 20 * np.log10(np.maximum(spec, 1e-12))


fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5), sharey=True)

f, db = spectrum("alias5k")
ax1.semilogx(f[1:], db[1:], color="#ff7b00", lw=1.2)
ax1.set_xlim(1000, 24000)
ax1.set_ylim(-120, 30)
ax1.set_xlabel("Hz")
ax1.set_title("5 kHz @ Boost 10, -10 dB RMS — alias census")
ax1.grid(True, which="both", alpha=0.35)
for x, tag in [(5000, "H1"), (10000, "H2"), (15000, "H3"), (20000, "H4"),
               (8000, "alias\nH8"), (13000, "alias\nH7"), (18000, "alias\nH6"), (23000, "alias\nH5")]:
    ax1.annotate(tag, (x, 22), fontsize=7, ha="center", color="#333333")
    ax1.axvline(x, color="#555555", lw=0.8, ls=":", alpha=0.6)

f, db = spectrum("imd")
ax2.semilogx(f[1:], db[1:], color="#00bfff", lw=1.2)
ax2.set_xlim(100, 4000)
ax2.set_ylim(-120, 30)
ax2.set_xlabel("Hz")
ax2.set_title("440+660 Hz @ Boost 10 — IMD")
ax2.grid(True, which="both", alpha=0.35)
for x, tag in [(220, "diff\n2f1-f2"), (440, "f1"), (660, "f2"), (880, "2f1/2f2-f1"),
               (1100, "f1+f2"), (1320, "3f1"), (1540, "2f1+f2"), (1760, "4f1/f1+2f2")]:
    ax2.annotate(tag, (x, 22), fontsize=7, ha="center", color="#333333")
    ax2.axvline(x, color="#555555", lw=0.8, ls=":", alpha=0.6)

fig.suptitle("Blind spots closed: aliasing + IMD (real color stage)")
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\blind.png", dpi=110)
print("wrote blind.png")
