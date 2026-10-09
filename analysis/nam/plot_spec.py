"""Throwaway plot: analyzer-style spectrum (440 Hz, Boost 10, real ColorStage)."""
import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

FS = 48000.0
blocks = {}
label = None
with open(r"C:\Users\<you>\AppData\Local\Temp\opencode\spec.txt") as f:
    for line in f:
        line = line.strip()
        if line.startswith("#"):
            label = line[2:]
            blocks[label] = []
        elif line:
            blocks[label].append(float(line))

fig, (axH, axN) = plt.subplots(1, 2, figsize=(14, 5), sharey=True)
note_colors = {"A4": "#ff7b00", "F4": "#39d353", "B4": "#ff4da6"}
for label, note in [("F4_-18dBRMS_B1", "F4"), ("A4_-18dBRMS_B6", "A4"), ("B4_-18dBRMS_B10", "B4"),
                    ("F4_-10dBRMS_B1", "F4"), ("A4_-10dBRMS_B6", "A4"), ("B4_-10dBRMS_B10", "B4")]:
    ax = axH if "-18dB" in label else axN
    color = note_colors[note]
    y = np.asarray(blocks[label])
    y -= np.mean(y)  # drop DC (quadratic term), like an analyzer
    w = np.hanning(len(y))
    y = y * w
    spec = np.abs(np.fft.rfft(y)) / (len(y) / 4)  # Hann coherent gain 0.5 -> x2
    freqs = np.fft.rfftfreq(len(y), 1.0 / FS)
    db = 20 * np.log10(np.maximum(spec, 1e-12))
    ax.semilogx(freqs[1:], db[1:], color=color, lw=1.4, label=label.replace("_", ", "))
    if label == "B4_-10dBRMS_B10":
        ax.fill_between(freqs[1:], db[1:], -120, color=color, alpha=0.18)
for ax, title in [(axH, "-18 dB RMS in"), (axN, "-10 dB RMS in")]:
    ax.set_xlim(20, 20000)
    ax.set_ylim(-120, 10)
    ax.set_xlabel("Hz")
    ax.set_title(title)
    ax.grid(True, which="both", alpha=0.35)
    ax.legend(fontsize=8)
axH.set_ylabel("dBFS")
axN.set_ylabel("dBFS")
for f0, tag in [(440.0, "A4"), (349.23, "F4"), (493.88, "B4")]:
    for ax in (axH, axN):
        ax.axvline(f0, color="#555555", lw=0.8, ls=":", alpha=0.7)
        ax.annotate(tag, (f0, 6), fontsize=8, ha="center", color="#555555")

fig.suptitle("Real color stage spectra, unity trim (Boost+Trim cancel)")
fig.tight_layout()
fig.savefig(r"C:\Users\<you>\AppData\Local\Temp\opencode\spec.png", dpi=110)
print("wrote spec.png")
