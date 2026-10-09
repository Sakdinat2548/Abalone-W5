"""Throwaway spike probe 1: fit ColorStage (k, a) to capture harmonic ladders.

Captures: B3, B7 (MicOut No EQ), DI (Boost2) — all tone-bypass-equivalent.
For each: 440 Hz sine at peak -20/-10/0 dBFS in, coherent H2/H3/H4 rel H1.
Then grid-search (k, a) with scale fixed=1 (nominal alignment: -20 dBFS peak
sine = nominal instrument level on both sides), exact transfer simulation.
Reports best (k, a), residuals, and operating-point numbers.
"""
import sys

sys.path.insert(0, r"C:\Users\<you>\AppData\Local\Temp\opencode")
import numpy as np
from nam_numpy import NAME

FS = 48000.0
N = 48000
FREQS = [440.0]
# (file, boost dB):Named captures' hardware boost knob. Sub-floor points
# (loudest harmonic still near the capture floor) enter as UPPER BOUNDS:
# penalize only predictions dirtier than measured+3dB.
LEVELS = [-20.0, -15.0, -10.0, -5.0, 0.0]  # dBFS peak
CLEAN = {-10.0, -5.0, 0.0}
BOOSTS = {"B3": 9.0, "B7": 21.0, "DI": 6.0}


def ladder(y, f):
    y = np.asarray(y, dtype=float)
    y -= np.mean(y)
    out = []
    h1 = None
    for h in (1, 2, 3, 4):
        a = 2 * np.pi * f * h * np.arange(N) / FS
        m = 2 * np.hypot(np.sum(y * np.cos(a)), np.sum(y * np.sin(a))) / N
        if h == 1:
            h1 = m
        out.append(20 * np.log10(max(m / h1, 1e-12)) if h > 1 else 20 * np.log10(max(h1, 1e-12)))
    return out


def ladder23(y, f):
    """H2/H3 rel H1 only (H4+ is numerology at these drives)."""
    y = np.asarray(y, dtype=float)
    y -= np.mean(y)
    hs = []
    h1 = None
    for h in (1, 2, 3):
        a = 2 * np.pi * f * h * np.arange(N) / FS
        m = 2 * np.hypot(np.sum(y * np.cos(a)), np.sum(y * np.sin(a))) / N
        if h == 1:
            h1 = m
        else:
            hs.append(20 * np.log10(max(m / h1, 1e-12)))
    return hs[0], hs[1]


print("== capture ladders (H2/H3 rel H1) ==")
caps = {
    "B3": r"C:\Users\<you>\Downloads\Avalon U5 Boost3 MicOut No EQ.nam",
    "B7": r"C:\Users\<you>\Downloads\Avalon U5 Boost7(Unity) MicOut No EQ.nam",
    "DI": r"C:\Users\<you>\Downloads\Avalon U5 DI Preamplifier.nam",
}
meas = {}
for tag, path in caps.items():
    m = NAME(path)
    meas[tag] = {}
    for lv in LEVELS:
        x = 10 ** (lv / 20.0) * np.sin(2 * np.pi * 440.0 * np.arange(N) / FS)
        y = m.forward(x)
        h2, h3 = ladder23(y, 440.0)
        meas[tag][lv] = (h2, h3)
        print("%s in %+.0f (color %+.0f): H2 %+.1f H3 %+.1f" % (tag, lv, lv + BOOSTS[tag], h2, h3))

print("== grid search (k, a): H2/H3, known staging, hinge below floor ==")
best = None


def acc(se_n, pred, got, clean):
    se, n = se_n
    if clean:
        return (se + (pred - got) ** 2, n + 1)
    over = pred - (got + 3.0)  # upper bound: only dirtier-than-measured counts
    return (se + (over ** 2 if over > 0 else 0.0), n + (1 if over > 0 else 0))


for k in np.logspace(np.log10(0.03), np.log10(0.2), 11):
    for a in np.logspace(np.log10(6e-4), np.log10(1.2e-2), 11):
        se, n = 0.0, 0
        norm = np.tanh(k)
        for tag in caps:
            for lv in LEVELS:
                amp = 10 ** ((lv + BOOSTS[tag]) / 20.0)
                xs = amp * np.sin(2 * np.pi * 440.0 * np.arange(N) / FS)
                ys = np.tanh(k * xs) / norm + a * xs * xs
                ys -= ys.mean()
                h2, h3 = ladder23(ys, 440.0)
                for pred, got in ((h2, meas[tag][lv][0]), (h3, meas[tag][lv][1])):
                    se, n = acc((se, n), pred, got, lv in CLEAN)
        rmse = (se / n) ** 0.5
        if best is None or rmse < best[0]:
            best = (rmse, k, a)
print("best: rmse %.2f dB  k=%.4f  a=%.2e" % best)
se = 0.0
n = 0
for tag in caps:
    for lv in LEVELS:
        amp = 10 ** ((lv + BOOSTS[tag]) / 20.0)
        xs = amp * np.sin(2 * np.pi * 440.0 * np.arange(N) / FS)
        ys = np.tanh(0.03 * xs) / np.tanh(0.03) + 6e-4 * xs * xs
        ys -= ys.mean()
        h2, h3 = ladder23(ys, 440.0)
        for pred, got in ((h2, meas[tag][lv][0]), (h3, meas[tag][lv][1])):
            se, n = acc((se, n), pred, got, lv in CLEAN)
print("current k=0.03 a=6e-4: rmse %.2f dB" % ((se / n) ** 0.5))
# operating-point projection for the winner
_, bk, ba = best
for L in (0.0, 10.0):
    A = 10 ** (L / 20.0)
    h2 = ba * A / 2 * 100
    h3 = (bk ** 2) * (A ** 2) / 12 * 100
    import math
    print("winner @ %+.0fdB: THD ~%.3f%% (H2 %.3f H3 %.3f)" % (
        L, math.sqrt(h2 * h2 + h3 * h3), h2, h3))
