"""ColorStage remap probe: capture harmonic ladders vs shipped-stage mirror.

Captures (analysis/ir_local, machine-local, never in git):
  B3  = Boost3 MicOut No EQ      (hardware boost +9 dB)
  B7  = Boost7(Unity) MicOut     (hardware boost +21 dB)
  DI  = DI Boost2 No tone        (hardware boost +6 dB)
  T2  = TONE 2 Boost4            (hardware boost +12 dB; tone stack engaged
                                  but linear -> no new harmonics, noted)

For each (capture, freq, input peak dBFS): coherent H1..H8 rel H1 (dB).
Model mirror (float64 exact port of src/dsp/ColorStage.h @ dev):
  tanh(0.06x)/tanh(0.06) + 0.0015*xe^2 (|xe|<=31.6) + gate*(0.03475*uo^3*e^-uo^2
  - 0.05*ue^2*e^-ue^2), uo=x/0.56, ue=x/0.82, gate=(env/(env+10.526))^0.6,
  peak-hold env with atk 0.19 / rel 0.000208 @48k. Driven at peak amplitude
  10^((level+boost)/20) with 0.1 s warm-up + mean removal, same ladder.
Comparison is rel-H1 only, so capture output-gain staging cancels.
Writes remap.csv + prints worst residuals per harmonic.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nam_numpy import NAME

HERE = os.path.dirname(os.path.abspath(__file__))
IR = os.path.join(HERE, "..", "ir_local")
FS = 48000.0
N = 48000
WARM = 4800
FREQS = [55.0, 110.0, 220.0, 440.0, 1000.0, 2000.0, 5000.0]
LEVELS = [-30.0, -25.0, -20.0, -15.0, -10.0, -5.0, 0.0]
CAPS = {
    "B3": ("Avalon U5 Boost3 MicOut No EQ.nam", 9.0),
    "B7": ("Avalon U5 Boost7(Unity) MicOut No EQ.nam", 21.0),
    "DI": ("Avalon U5 DI Boost2 No tone.nam", 6.0),
    "T2": ("Avalon U5 TONE 2 Boost4.nam", 12.0),
}
# shipped constants (src/dsp/ColorStage.h)
K, A2 = 0.06, 0.0015
NORM = np.tanh(K)
KODDC, KODDW = 0.03475, 0.56
KEVC, KEVW = -0.05, 0.82
GN0, GQ = 10.526, 0.60
KATK, KREL = 0.19, 0.000208


def ladder(y, f, top=8):
    y = np.asarray(y, dtype=float)
    y -= np.mean(y)
    t = np.arange(len(y)) / FS
    out = []
    h1 = None
    for h in range(1, top + 1):
        a = 2 * np.pi * f * h * t
        m = 2 * np.hypot(np.sum(y * np.cos(a)), np.sum(y * np.sin(a))) / len(y)
        if h == 1:
            h1 = m
            out.append(20 * np.log10(max(h1, 1e-12)))
        else:
            out.append(20 * np.log10(max(m / h1, 1e-12)))
    return out


def stage_mirror(xs):
    env = 0.0
    ys = np.empty_like(xs)
    for i, x in enumerate(xs):
        ax = abs(x)
        env += (KATK if ax > env else KREL) * (ax - env)
        if env < 1e-15:
            env = 0.0
        gate = (env / (env + GN0)) ** GQ
        uo, ue = x / KODDW, x / KEVW
        br = gate * (KODDC * uo**3 * np.exp(-uo * uo) + KEVC * ue * ue * np.exp(-ue * ue))
        xe = min(31.6, max(-31.6, x))
        ys[i] = np.tanh(K * x) / NORM + A2 * xe * xe + br
    return ys


def main():
    rows = []
    for tag, (fn, boost) in CAPS.items():
        m = NAME(os.path.join(IR, fn))
        print("capture %s (boost %+.0f dB)" % (tag, boost), flush=True)
        for f in FREQS:
            for lv in LEVELS:
                x = 10 ** (lv / 20.0) * np.sin(2 * np.pi * f * np.arange(N) / FS)
                got = ladder(m.forward(x)[-N:], f)
                amp = 10 ** ((lv + boost) / 20.0)
                xs = amp * np.sin(2 * np.pi * f * np.arange(N + WARM) / FS)
                pred = ladder(stage_mirror(xs)[-N:], f)
                rows.append((tag, f, lv, got, pred))
                print("  f=%6.0f in=%+.0f color=%+.0f | H2 %+.1f/%+.1f H3 %+.1f/%+.1f "
                      "H4 %+.1f/%+.1f H5 %+.1f/%+.1f H6 %+.1f/%+.1f" % (
                          f, lv, lv + boost, got[1], pred[1], got[2], pred[2],
                          got[3], pred[3], got[4], pred[4], got[5], pred[5]), flush=True)
    with open(os.path.join(HERE, "remap.csv"), "w") as fh:
        fh.write("cap,freq,level_dbfs," + ",".join(
            "gotH%d,predH%d" % (h, h) for h in range(1, 9)) + "\n")
        for tag, f, lv, got, pred in rows:
            cells = []
            for g, p in zip(got, pred):
                cells += ["%.2f" % g, "%.2f" % p]
            fh.write("%s,%.0f,%.0f,%s\n" % (tag, f, lv, ",".join(cells)))
    print("== worst |got-pred| per harmonic (dB) ==")
    for h in range(2, 9):
        w = max(abs(g[h - 1] - p[h - 1]) for _, _, _, g, p in rows)
        print("H%d: %.1f dB" % (h, w))
    print("wrote remap.csv (%d rows)" % len(rows))


if __name__ == "__main__":
    main()
