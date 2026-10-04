"""Hardware-constrained tone refinement (research, R&D only — not gated).

Fits the SHIPPED RBJ section types/counts (numbers-only, ToneBank.h) toward
traced-hardware truth (nodal_tone.py netlists) ONLY inside IR-confirmed win
zones (Welch cross-spectral kIrSpots where nodal beats shipped DSP >0.5dB),
preserving the manual-chart CSV everywhere else:

  T1: 5-12 kHz, T2: 2-16 kHz, T3: 80-200 Hz + 12-16 kHz,
  T4: 2-5 kHz + 12-16 kHz, T5: 35-300 Hz, T6: 35-300 Hz + 3-16 kHz.

Bounds (refinement, not rebuild): f0 x[0.7,1.4], gain +/-4 dB, Q [0.15,8],
HP f0 [3,40]. Penalties: pole triangle, 44.1/48 kHz agreement <=0.12 dB.

RESULT (2026-10-04): no joint position exists — every tone either keeps
CSV damage >0.9 dB or leaves win-zone error unmoved (T2 optimizer diverged:
7.2 dB). Chart and hardware differ 1-4 dB in these zones; no same-budget
RBJ position satisfies both. DSP stays as shipped; promoting hardware
truth is a re-spec decision (new gates + possible +sections), not a fit
correction. See TRACE_NOTES.md (nodal adjudication + v2 oracle sections).

numpy only. Usage: python3 analysis/circuit_trace/fit_hardware.py
"""
import csv
import math
import os
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))  # analysis/ for ir_check + nodal
sys.path.insert(0, HERE)
from ir_check import rbj_cook, log_interp  # noqa: E402
from nodal_tone import solve as nodal_solve, freqs as FULLF  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))
FS48, FS44 = 48000.0, 44100.0
GATE = (FULLF >= 40) & (FULLF <= 15000)

# (type, f0, q, gain) shipped numbers (ToneBank.h params table)
CUR = {
    1: [("hp", 5.0, 0.40395, 0.0), ("pk", 49.0866, 0.35951, 1.8346),
        ("pk", 1048.8736, 0.16364, -13.7137), ("pk", 1734.5392, 0.21392, 7.1515),
        ("hs", 11981.7927, 1.06858, 1.311), ("pk", 284.8489, 0.89252, 1.335)],
    2: [("pk", 644.5752, 0.31635, -9.5942), ("pk", 715.0513, 1.6408, -12.8167),
        ("ls", 12.4907, 0.91201, -1.5117), ("pk", 34.9406, 0.50216, 1.5057),
        ("pk", 3159.9499, 0.61821, -0.7183), ("hs", 610.9243, 1.6477, 1.3745)],
    3: [("pk", 1139.8615, 0.15462, -3.7423), ("pk", 18852.0009, 0.17095, 0.4067),
        ("ls", 12.9996, 0.78994, -3.6748), ("pk", 40.4319, 0.64122, 0.8181),
        ("pk", 677.6882, 1.72978, -0.2364)],
    4: [("ls", 23.3315, 0.66438, -2.6291), ("pk", 8815.7501, 1.19986, -2.0138),
        ("hs", 12.6487, 1.02216, 1.5772), ("pk", 5139.997, 0.66627, -4.9216),
        ("hs", 14083.1316, 0.57743, 1.3207), ("pk", 9.9251, 3.37706, -0.4468)],
    5: [("ls", 16.5931, 0.66482, -4.0), ("ls", 61.4232, 0.42485, -19.2452),
        ("hs", 166.6066, 0.51491, 2.5516), ("pk", 732.2752, 1.09303, 0.3299),
        ("pk", 11.0426, 3.60242, -0.6)],
    6: [("ls", 17.7493, 0.65975, -3.972), ("ls", 63.4268, 0.43413, -18.9831),
        ("hs", 198.2506, 0.53229, 2.7376), ("hs", 14729.0504, 0.51821, -5.6445),
        ("pk", 20141.4081, 1.42257, -0.5909), ("pk", 11.1211, 2.63754, -0.7999)],
}

WIN = {1: [(5000, 12000)], 2: [(2000, 16000)], 3: [(80, 200), (12000, 16000)],
       4: [(2000, 5000), (12000, 16000)], 5: [(35, 300)],
       6: [(35, 300), (3000, 16000)]}


def cascade_db(sections, f, fs):
    w = 2 * np.pi * np.asarray(f) / fs
    z = np.exp(1j * w)
    h = np.ones_like(f, dtype=complex)
    for typ, f0, q, g in sections:
        b0, b1, b2, a1, a2 = rbj_cook(typ, f0, q, g, fs)
        h *= (b0 + b1 / z + b2 / z / z) / (1.0 + a1 / z + a2 / z / z)
    return 20 * np.log10(np.maximum(np.abs(h), 1e-12))


def worst_pole(sections, fs):
    r = 0.0
    for typ, f0, q, g in sections:
        _, _, _, a1, a2 = rbj_cook(typ, f0, q, g, fs)
        if abs(a2) >= 1.0 or abs(a1) >= 1.0 + a2:
            return 2.0
        r = max(r, math.sqrt(abs(a2)))
    return r


def zone_mask(tone):
    m = np.zeros_like(FULLF, bool)
    for lo, hi in WIN[tone]:
        m |= (FULLF >= lo) & (FULLF <= hi)
    return m & GATE


def objective(sections, tgt_nodal, tgt_csv, zm):
    m = cascade_db(sections, FULLF, FS48)
    dn = np.abs((m - tgt_nodal)[zm])
    dc = np.abs((m - tgt_csv)[~zm & GATE])
    against = float(max(dn.max(), dc.max()) + 0.05 * (dn.mean() + dc.mean()))
    rate = cascade_db(sections, FULLF, FS44) - cascade_db(sections, FULLF, FS48)
    rate_pen = max(0.0, float(np.abs(rate[GATE]).max()) - 0.12) * 5.0
    pole_pen = max(0.0, worst_pole(sections, FS48) - 0.9999) * 100.0
    pole_pen += max(0.0, worst_pole(sections, FS44) - 0.9999) * 100.0
    return against + rate_pen + pole_pen


def clamp(sec, ref):
    typ, f0, q, g = sec
    _, rf0, _, rg = ref
    if typ == "hp":
        f0 = min(max(f0, 3.0), 40.0)
    else:
        f0 = min(max(f0, rf0 * 0.7), rf0 * 1.4)
        g = min(max(g, rg - 4.0), rg + 4.0)
    q = min(max(q, 0.15), 8.0)
    return (typ, f0, q, g)


def fit_tone(sections, tgt_nodal, tgt_csv, zm):
    refs = [(t, f, q, g) for t, f, q, g in sections]
    p = [clamp(x, r) for x, r in zip(sections, refs)]
    best = objective(p, tgt_nodal, tgt_csv, zm)
    mult = [1.12, 1.05, 1.02, 1.008]
    adds = [1.5, 0.7, 0.3, 0.12]
    for _ in range(50):
        improved = False
        for i, (typ, f0, q, g) in enumerate(p):
            for kind, step in (("f", mult[0]), ("q", mult[0]), ("g", adds[0])):
                if typ == "hp" and kind == "g":
                    continue
                for s in (+1, -1):
                    q2 = [list(x) for x in p]
                    if kind == "f":
                        q2[i][1] *= step ** s
                    elif kind == "q":
                        q2[i][2] *= step ** s
                    else:
                        q2[i][3] += s * step
                    q2[i] = list(clamp(tuple(q2[i]), refs[i]))
                    cand = [(t, f, qq, gg) for t, f, qq, gg in q2]
                    e = objective(cand, tgt_nodal, tgt_csv, zm)
                    if e < best - 1e-9:
                        best, p, improved = e, cand, True
        if not improved:
            if adds[0] < 0.03:
                break
            adds = [a / 2.5 for a in adds]
            mult = [1 + (m_ - 1) / 2.5 for m_ in mult]
    return p, best


def main():
    cf, cc = [], {}
    with open(os.path.join(REPO, "analysis", "u5_tone_curves_digitized.csv"),
              newline="") as fh:
        import csv as _csv
        for row in _csv.DictReader(fh):
            cf.append(float(row["freq_hz"]))
            for t in range(1, 7):
                cc.setdefault(t, []).append(float(row["curve%d_db" % t]))
    cf = np.array(cf)
    Vn = nodal_solve(FULLF)
    Hnod = 20 * np.log10(np.maximum(Vn, 1e-30))
    print("tone | win-nodal(max/RMS) | csv-keep(gate max/RMS) | pole48 rate")
    for t in range(1, 7):
        nn = Hnod[t - 1] - float(log_interp(1000.0, FULLF, Hnod[t - 1]))
        cc1k = np.array(cc[t]) - float(np.interp(1000.0, cf, np.array(cc[t])))
        csvf = np.interp(FULLF, cf, cc1k)
        zm = zone_mask(t)
        best_p, best_e = None, 1e9
        for jit in (1.0, 0.9, 1.1):
            q0 = [(typ, f0 * jit, q, g) for typ, f0, q, g in CUR[t]]
            p, e = fit_tone(q0, nn, csvf, zm)
            if e < best_e:
                best_p, best_e = p, e
        m = cascade_db(best_p, FULLF, FS48)
        wn = np.abs((m - nn)[zm])
        wc = np.abs((m - csvf)[GATE])
        print("T%d nodal-win max %.3f rms %.4f || csv gate max %.3f rms %.4f "
              "pole48 %.5f rate %.3f" % (
                  t, float(wn.max()), float(np.sqrt((wn ** 2).mean())),
                  float(wc.max()), float(np.sqrt((wc ** 2).mean())),
                  worst_pole(best_p, FS48),
                  float(np.abs(cascade_db(best_p, FULLF, FS44)
                               - cascade_db(best_p, FULLF, FS48))[GATE].max())))
        for typ, f0, q, g in best_p:
            print("    %s %.4f %.5f %+.4f" % (typ, f0, q, g))


if __name__ == "__main__":
    main()
