"""Save ruled blue target curves (exactly as previewed) + fit shipped RBJ
sections to them. Research only. Usage:
  python3 analysis/circuit_trace/fit_blue.py --save   # write targets CSV
  python3 analysis/circuit_trace/fit_blue.py --fit    # optimize (slow)
Target recipes: T1=nodal; T2=avg(IR,nodal); T3=avg(IR,nodal,chart);
T4=chart<1k + avg(IR,nodal)>=1k; T5/T6=nodal. 1 kHz-normalized.
"""
import csv
import math
import os
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))  # analysis/
sys.path.insert(0, HERE)
from ir_check import rbj_cook, log_interp  # noqa: E402
import derive  # noqa: E402
from nodal_tone import solve as nodal_solve, freqs as FULLF  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))
FS48, FS44 = 48000.0, 44100.0
BLUE_CSV = os.path.join(REPO, "analysis", "u5_tone_targets_blue.csv")

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


def n1k(g, v):
    return np.asarray(v) - float(np.interp(1000.0, g, v))


def build_targets():
    ff, ch = derive.load_csv()
    vn = nodal_solve(FULLF)
    hn = 20 * np.log10(np.maximum(vn, 1e-30))
    chart = {n: n1k(FULLF, np.interp(FULLF, ff, ch[n])) for n in range(1, 7)}
    nodal = {n: n1k(FULLF, hn[n - 1]) for n in range(1, 7)}
    irr = {n: derive.ir_shape(n, FULLF) for n in range(1, 7)}
    tgt = {1: nodal[1], 2: (irr[2] + nodal[2]) / 2,
           3: (irr[3] + nodal[3] + chart[3]) / 3,
           4: np.where(FULLF < 1000, chart[4], (irr[4] + nodal[4]) / 2),
           5: nodal[5], 6: nodal[6]}
    return tgt


def cmd_save():
    tgt = build_targets()
    with open(BLUE_CSV, "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["freq_hz"] + ["curve%d_db" % k for k in range(1, 7)])
        for i, f in enumerate(FULLF):
            w.writerow([round(float(f), 2)]
                       + [round(float(tgt[t][i]), 4) for t in range(1, 7)])
    print("wrote", BLUE_CSV)


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


def objective(sections, target):
    m = cascade_db(sections, FULLF, FS48) - target
    mg = m[(FULLF >= 40) & (FULLF <= 15000)]
    against = float(np.abs(mg).max() + 0.1 * np.abs(mg).mean())
    rate = cascade_db(sections, FULLF, FS44) - cascade_db(sections, FULLF, FS48)
    rate_pen = max(0.0, float(np.abs(rate).max()) - 0.15) * 10.0
    pole_pen = max(0.0, worst_pole(sections, FS48) - 0.9995) * 200.0
    pole_pen += max(0.0, worst_pole(sections, FS44) - 0.9995) * 200.0
    return against + rate_pen + pole_pen


def clamp(sec, ref):
    typ, f0, q, g = sec
    _, rf0, _, rg = ref
    if typ == "hp":
        f0 = min(max(f0, 3.0), 60.0)
    else:
        f0 = min(max(f0, rf0 * 0.4), rf0 * 2.5)
        g = min(max(g, rg - 8.0), rg + 8.0)
    q = min(max(q, 0.12), 10.0)
    return (typ, f0, q, g)


def fit_tone(sections, target):
    refs = [(t, f, q, g) for t, f, q, g in sections]
    p = [clamp(x, r) for x, r in zip(sections, refs)]
    best = objective(p, target)
    mult = [1.2, 1.08, 1.03, 1.01]
    adds = [2.0, 1.0, 0.4, 0.15]
    for _ in range(70):
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
                    e = objective(cand, target)
                    if e < best - 1e-9:
                        best, p, improved = e, cand, True
        if not improved:
            if adds[0] < 0.04:
                break
            adds = [a / 2.5 for a in adds]
            mult = [1 + (m_ - 1) / 2.5 for m_ in mult]
    return p, best


def cmd_fit():
    tgt = build_targets()
    print("tone | blue-fit(max/RMS 40-15k) | pole48 rate")
    for t in range(1, 7):
        best_p, best_e = None, 1e9
        for jf, jg in ((1.0, 0.0), (0.8, 0.0), (1.25, 0.0), (1.0, 2.0),
                       (1.0, -2.0)):
            q0 = [(typ, f0 * jf, q, g + jg) for typ, f0, q, g in CUR[t]]
            p, e = fit_tone(q0, tgt[t])
            if e < best_e:
                best_p, best_e = p, e
        m = cascade_db(best_p, FULLF, FS48) - tgt[t]
        mg = m[(FULLF >= 40) & (FULLF <= 15000)]
        print("T%d max %.3f rms %.4f pole48 %.5f rate %.3f" % (
            t, float(np.abs(mg).max()), float(np.sqrt((mg ** 2).mean())),
            worst_pole(best_p, FS48),
            float(np.abs(cascade_db(best_p, FULLF, FS44)
                         - cascade_db(best_p, FULLF, FS48)).max())))
        for typ, f0, q, g in best_p:
            print("    %s %.4f %.5f %+.4f" % (typ, f0, q, g))


if __name__ == "__main__":
    if "--save" in sys.argv:
        cmd_save()
    elif "--fit" in sys.argv:
        cmd_fit()
    else:
        print("usage: fit_blue.py --save | --fit")
