"""Regenerate docs/tone-curves.png + docs/tone-curves-12panel.png from the
LIVE ToneBank numbers (fit8 params + pink staging) (manual frame per AGENTS.md curves rule: y
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
# (type, f0, q, gain) LIVE fit8 numbers (ToneBank.h params table)
PARAMS = {
    1: [("hp", 5.827048, 0.61509, 0.0),
        ("pk", 17.809303, 0.510313, 2.386098),
        ("pk", 576.308198, 0.238087, -8.7206),
        ("pk", 316.362377, 0.156875, 8.244914),
        ("hs", 4282.69202, 0.472141, 7.93173),
        ("pk", 35.227056, 0.495389, 2.057284)],
    2: [("pk", 729.990772, 0.399998, -7.0),
        ("pk", 678.226926, 1.3, -11.5),
        ("ls", 5.0, 1.2, 9.375711),
        ("pk", 54.514871, 0.280132, 5.812911),
        ("pk", 1813.7874, 0.503095, -2.166755),
        ("hs", 651.611076, 0.490301, 8.389202)],
    3: [("pk", 849.884661, 0.155052, -10.732894),
        ("pk", 14243.1507, 0.15, -2.115138),
        ("ls", 10.056113, 0.88107, -7.999959),
        ("pk", 13.767599, 0.664759, -4.67115),
        ("pk", 979.030698, 0.193848, 5.332838)],
    4: [("ls", 28.464636, 0.627733, -3.408831),
        ("pk", 16931.4281, 0.23498, -0.9418),
        ("hs", 16.822681, 1.719369, 0.739329),
        ("pk", 7342.94425, 0.602455, -4.263846),
        ("hs", 20928.6602, 0.42978, 1.3207),
        ("pk", 10.81717, 4.534375, -1.624585)],
    5: [("ls", 10.7876, 0.53104, -5.696),
        ("ls", 52.824, 0.42485, -21.2452),
        ("hs", 217.3673, 0.91213, 0.2316),
        ("pk", 388.14, 1.30292, -0.2101),
        ("pk", 9.3407, 1.41276, -1.7296)],
    6: [("ls", 14.857262, 0.58017, -5.663124),
        ("ls", 55.137982, 0.426738, -21.94624),
        ("hs", 138.753866, 0.477581, -1.631875),
        ("hs", 10751.5081, 0.477924, -8.041181),
        ("pk", 21998.938, 0.537264, -0.739306),
        ("pk", 10.033716, 2.32244, -0.90871)],
}
GAIN = {1: -3.9308, 2: -4.0374, 3: 4.0325, 4: 0.4536, 5: 1.8810, 6: 4.5376}
GRID = np.logspace(np.log10(10), np.log10(20000), 600)


def cascade(tone, f, highcut=False):
    w = 2 * np.pi * np.asarray(f) / FS
    z = np.exp(1j * w)
    h = np.ones_like(f, dtype=complex)
    for typ, f0, q, g in PARAMS[tone]:
        b0, b1, b2, a1, a2 = rbj_cook(typ, f0, q, g, FS)
        h *= (b0 + b1 / z + b2 / z / z) / (1.0 + a1 / z + a2 / z / z)
    if highcut:  # 1-pole -3dB @ 4kHz, exact HighCut.h coefficient
        w = 2 * np.pi * 4000.0 / FS
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
