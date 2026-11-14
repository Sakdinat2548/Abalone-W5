"""Claude-flag-1 test: is TONE 2's linear FR level-dependent?

Measure T2 capture out/in RMS per freq at -30/-25/-20 dBFS in (all nominally
linear regime). If FR moves with level, the tone path itself is
level-dependent: the static-FR correction in t2_calcheck.py is invalid and
the T2 tilt needs a level-dependent account, not a second static block.
Also reports H2 at each point to confirm linear regime (must stay floored).
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nam_numpy import NAME

HERE = os.path.dirname(os.path.abspath(__file__))
IR = os.path.join(HERE, "..", "ir_local")
FS, N = 48000.0, 48000
FREQS = [55.0, 110.0, 220.0, 440.0, 1000.0, 2000.0, 5000.0]
LEVELS = [-30.0, -25.0, -20.0]


def main():
    m = NAME(os.path.join(IR, "Avalon U5 TONE 2 Boost4.nam"))
    fr = {}
    for lv in LEVELS:
        for f in FREQS:
            x = 10 ** (lv / 20.0) * np.sin(2 * np.pi * f * np.arange(N) / FS)
            y = m.forward(x)
            g = float(np.sqrt(np.mean(y**2)) / np.sqrt(np.mean(x**2)))
            t = np.arange(N) / FS
            a2 = 2 * np.pi * f * 2 * t
            h2 = 2 * np.hypot(np.sum(y * np.cos(a2)), np.sum(y * np.sin(a2))) / N
            a1 = 2 * np.pi * f * t
            h1 = 2 * np.hypot(np.sum(y * np.cos(a1)), np.sum(y * np.sin(a1))) / N
            fr[(lv, f)] = (g, 20 * np.log10(max(h2 / h1, 1e-12)))
            print("in %+.0f f %4.0f gain %+.2f H2rel %+.1f" % (lv, f, 20 * np.log10(g), fr[(lv, f)][1]), flush=True)
    print("== FR drift vs level (dB rel -30 row, per freq) ==")
    for f in FREQS:
        g0 = 20 * np.log10(fr[(-30.0, f)][0])
        print("f %4.0f " % f + " ".join(
            "%+.0fdB:%+.2f" % (lv, 20 * np.log10(fr[(lv, f)][0]) - g0) for lv in LEVELS))


if __name__ == "__main__":
    main()
