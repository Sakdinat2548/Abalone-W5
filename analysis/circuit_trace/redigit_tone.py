"""Re-digitize the U5 manual tone pictures with a CORRECTED log x-axis.

Provenance (read before trusting): analysis/u5_tone_curves_digitized.csv
(v1) is Claude's eyeball digitization of docs/refs/Tone+images.png, and it
bakes in a known axis distortion — the printed 20 kHz tick sits ~2x wider
than true log scale, and v1 follows the drawn ticks (see Downloads/
claudeOutputforU5/claudeoutput.txt: "I followed the tick marks as drawn",
"the bend near 10 kHz in curves 1 and 4 may come from the graph's axis",
T2 tip hand-corrected, 10 Hz edge least reliable).

This script replicates Claude's trace extraction exactly (threshold <90,
5x5 opening, column means; v1 replica matches the repo CSV to 0.007 dB
except the hand-fixed T2 tip) and adds the v2 x-map: true-log positions
from the 10/100/1k/10k ticks, 20 kHz placed one true decade past 10 kHz,
ignoring the drawn 20k tick. Writes u5_tone_curves_digitized_v2.csv
(research oracle — does NOT replace the v1 binding file or any gate).

numpy + PIL + scipy only.
"""
import csv
import os
import numpy as np
from PIL import Image
from scipy import ndimage as ndi

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(REPO, "docs", "refs", "Tone+images.png")
from scipy import ndimage as ndi

A = np.array(Image.open(SRC).convert("L")).astype(int)
PANELS = [(29, 687, 55, 472), (783, 1440, 55, 472), (1545, 2202, 55, 472),
          (29, 687, 769, 1186), (783, 1440, 769, 1186), (1545, 2202, 769, 1186)]
XT = [(141, 278, 415, 553, 635), (894, 1031, 1169, 1306, 1388),
      (1656, 1794, 1931, 2068, 2150)]
HY = [(99, 404), (814, 1118)]
TICKS = np.array([10, 100, 1e3, 1e4, 2e4])


def xmap(x, t, fix):
    t = np.array(t, float)
    lf = np.log10(TICKS)
    if fix:  # true log: 20k one decade past 10k regardless of drawn tick
        s = (lf[3] - lf[2]) / (t[3] - t[2])
        base = t[2]
        return 10 ** (lf[2] + (x - base) * s)
    if x < t[0]:
        s = (lf[1] - lf[0]) / (t[1] - t[0])
        return 10 ** (lf[0] + (x - t[0]) * s)
    if x > t[-1]:
        s = (lf[-1] - lf[-2]) / (t[-1] - t[-2])
        return 10 ** (lf[-1] + (x - t[-1]) * s)
    return 10 ** np.interp(x, t, lf)


def ydb(y, top, bot):
    return 6 + (y - top) * (-24 - 6) / (bot - top)


def extract(fix):
    curves = {}
    for k, (x0, x1, y0, y1) in enumerate(PANELS):
        t = XT[k % 3]
        top, bot = HY[k // 3]
        sub = ndi.binary_opening(A[y0:y1, x0:x1] < 90,
                                   structure=np.ones((5, 5)))
        xs, fs, ds = [], [], []
        for x in range(t[0] - 20 - x0, x1 - x0):
            col = np.where(sub[:, x])[0]
            col = col[(col + y0 > top - 25) & (col + y0 < bot + 25)]
            if len(col) == 0:
                continue
            xs.append(x + x0)
            fs.append(xmap(x + x0, t, fix))
            ds.append(ydb(col.mean() + y0, top, bot))
        curves[k + 1] = (np.array(fs), np.array(ds))
    return curves


repo_f, repo = [], {}
with open(os.path.join(REPO, "analysis", "u5_tone_curves_digitized.csv"),
          newline="") as fh:
    for row in csv.DictReader(fh):
        repo_f.append(float(row["freq_hz"]))
        for t in range(1, 7):
            repo.setdefault(t, []).append(float(row["curve%d_db" % t]))
repo_f = np.array(repo_f)

for fix, name in ((False, "v1-replica"), (True, "v2-fixed")):
    cur = extract(fix)
    print("== %s ==" % name)
    for t in range(1, 7):
        f, d = cur[t]
        interp = np.interp(np.log10(repo_f), np.log10(f), d,
                           left=np.nan, right=np.nan)
        m = ~np.isnan(interp)
        dd = np.abs(interp[m] - np.array(repo[t])[m])
        print(" T%d pts=%d max=%.3f mean=%.3f" % (t, m.sum(), dd.max(), dd.mean()))

# Write the v2 research oracle on the 121-pt log grid (NaN where the fixed
# axis maps outside extracted coverage — none expected, asserted below).
grid = np.logspace(np.log10(10), np.log10(20000), 121)
v2 = extract(True)
cols = []
for t in range(1, 7):
    f, d = v2[t]
    assert f.min() <= 10.0 and f.max() >= 20000.0, (t, f.min(), f.max())
    cols.append(np.interp(np.log10(grid), np.log10(f), d))
with open(os.path.join(REPO, "analysis", "u5_tone_curves_digitized_v2.csv"),
          "w", newline="") as fh:
    w = csv.writer(fh)
    w.writerow(["freq_hz"] + ["curve%d_db" % k for k in range(1, 7)])
    for i, g in enumerate(grid):
        w.writerow([round(float(g), 2)] + [round(float(c[i]), 2) for c in cols])
print("wrote u5_tone_curves_digitized_v2.csv")
