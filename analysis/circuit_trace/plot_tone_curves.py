"""Regenerate docs/tone-curves.png + docs/tone-curves-12panel.png from the
SHIPPED ToneBank numbers (manual frame per AGENTS.md curves rule: y
+6/0/-6/-12/-18/-24, x 10/100/1kHz/10k/20k, full tick labels every panel).
Same commit as any DSP shape change. numpy + matplotlib only."""
import math
import os
import sys
import numpy as np

sys.path.insert(0, r"C:\Users\kluis\Code\Abalone-W5\analysis")
from ir_check import rbj_cook

REPO = r"C:\Users\kluis\Code\Abalone-W5"
FS = 48000.0
# (type, f0, q, gain) shipped numbers (ToneBank.h params table)
PARAMS = {
    1: [("hp", 3.8136, 0.46776, 0.0), ("pk", 19.966, 0.27101, 6.0138),
        ("pk", 919.3987, 0.16448, -9.7137),
        ("pk", 1927.2658, 0.17827, 9.1515),
        ("hs", 8986.3445, 0.48574, 8.1929),
        ("pk", 135.9733, 0.26901, 5.335)],
    2: [("pk", 644.5752, 0.31635, -9.5942), ("pk", 715.0513, 1.6408, -12.8167),
        ("ls", 12.4907, 0.91201, -1.5117), ("pk", 34.9406, 0.50216, 1.5057),
        ("pk", 3159.9499, 0.61821, -0.7183),
        ("hs", 610.9243, 1.6477, 1.3745)],
    3: [("pk", 1139.8615, 0.15462, -3.7423),
        ("pk", 18852.0009, 0.17095, 0.4067),
        ("ls", 12.9996, 0.78994, -3.6748), ("pk", 40.4319, 0.64122, 0.8181),
        ("pk", 677.6882, 1.72978, -0.2364)],
    4: [("ls", 21.9804, 1.11756, -1.9891), ("pk", 16931.4281, 0.23498, -0.9418),
        ("hs", 79.0544, 0.12, -0.2948), ("pk", 8000.0, 0.69283, -4.128),
        ("hs", 20928.6602, 0.42978, 1.3207),
        ("pk", 10.4613, 4.56174, -1.5092)],
    5: [("ls", 10.7876, 0.53104, -5.696), ("ls", 52.824, 0.42485, -21.2452),
        ("hs", 217.3673, 0.91213, 0.2316), ("pk", 388.14, 1.30292, -0.7101),
        ("pk", 9.3407, 1.41276, -1.7296)],
    6: [("ls", 19.5861, 0.84932, -3.652), ("ls", 51.7034, 0.42067, -19.1111),
        ("hs", 292.1971, 0.94755, -0.1584),
        ("hs", 9158.3184, 0.54857, -6.8765),
        ("pk", 18976.0169, 0.8665, -0.9109),
        ("pk", 9.3951, 4.00708, -1.8879)],
}
GAIN = {1: -6.8239, 2: 0.0, 3: 0.0, 4: 0.7677, 5: 2.4954, 6: 2.81}
GRID = np.logspace(np.log10(10), np.log10(20000), 600)


def cascade(tone, f, highcut=False):
    w = 2 * np.pi * np.asarray(f) / FS
    z = np.exp(1j * w)
    h = np.ones_like(f, dtype=complex)
    for typ, f0, q, g in PARAMS[tone]:
        b0, b1, b2, a1, a2 = rbj_cook(typ, f0, q, g, FS)
        h *= (b0 + b1 / z + b2 / z / z) / (1.0 + a1 / z + a2 / z / z)
    if highcut:  # 1-pole -3dB @ 8kHz, exact HighCut.h coefficient
        w = 2 * np.pi * 8000.0 / FS
        c = math.cos(w)
        t = 2.0 - c
        b = t - math.sqrt(t * t - 1.0)
        h *= ((1.0 - b) / (1.0 - b / z))
    return 20 * np.log10(np.maximum(np.abs(h), 1e-30)) + GAIN[tone]


import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

NAMES = {1: "1", 2: "2", 3: "3", 4: "4", 5: "5", 6: "6"}
fig, axes = plt.subplots(2, 3, figsize=(15, 9), sharex=True, sharey=True)
for idx, t in enumerate(range(1, 7)):
    ax = axes[idx // 3, idx % 3]
    ax.semilogx(GRID, cascade(t, GRID), color="red", lw=1.4)
    ax.set_title("Tone %s" % NAMES[t], fontsize=11)
    ax.set_xlim(10, 20000)
    ax.set_ylim(-24, 6)
    ax.set_yticks([6, 0, -6, -12, -18, -24])
    ax.set_xticks([10, 100, 1000, 10000, 20000])
    ax.set_xticklabels(["10", "100", "1kHz", "10k", "20k"])
    ax.grid(True, which="both", alpha=0.4)
    ax.set_xlabel("Hz")
    ax.set_ylabel("dB")
fig.suptitle("Abalone-W5 tone curves @48kHz (highcut off)", fontsize=13)
fig.tight_layout()
fig.savefig(os.path.join(REPO, "docs", "tone-curves.png"), dpi=110)
print("wrote docs/tone-curves.png")

fig2, ax2 = plt.subplots(6, 2, figsize=(12, 16), sharex=True, sharey=True)
for t in range(1, 7):
    for j, hc in enumerate((False, True)):
        ax = ax2[t - 1, j]
        ax.semilogx(GRID, cascade(t, GRID, hc), color="red", lw=1.2)
        ax.set_title("Tone %s highcut %s" % (NAMES[t], "ON" if hc else "off"),
                     fontsize=9)
        ax.set_xlim(10, 20000)
        ax.set_ylim(-24, 6)
        ax.set_yticks([6, 0, -6, -12, -18, -24])
        ax.set_xticks([10, 100, 1000, 10000, 20000])
        ax.set_xticklabels(["10", "100", "1kHz", "10k", "20k"])
        ax.grid(True, which="both", alpha=0.4)
        ax.set_xlabel("Hz")
        ax.set_ylabel("dB")
fig2.suptitle("Abalone-W5 tone bank — all 6 tones, highcut off/on",
              fontsize=12)
fig2.tight_layout()
fig2.savefig(os.path.join(REPO, "docs", "tone-curves-12panel.png"), dpi=100)
print("wrote docs/tone-curves-12panel.png")
