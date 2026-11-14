"""Fit on SETTLED Gateway data (gw_sweeps_all.csv) — supersedes remap.csv fits.

Reference set: B3 + DI bypass rows; B7 rows with color<=+12 only (hotter is
capture-unverified); T2 excluded from harmonic fit (memorized floors).
Nested M0/M1/M2 + BIC, hinge-scored, + M1+off variant (per-capture drive
offsets dB3/dDI: NAM input calibration is genuinely unknown, 2 params).
"""
import csv
import os

import numpy as np
from scipy.optimize import least_squares

import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fit_v3 import mirror, ladder

HOME = os.path.expanduser("~")  # machine-local tools/presets; no literal usernames
TMP = os.path.join(HOME, "AppData", "Local", "Temp", "opencode")
FS, N, WARM = 48000.0, 9600, 4800
BOOSTS = {"B3": 9.0, "B7": 21.0, "DI": 6.0}
HS = (2, 3, 4, 5)


def load():
    rows = []
    for r in csv.DictReader(open(os.path.join(TMP, "gw_sweeps_all.csv"))):
        if r["cap"] not in BOOSTS or r["cap"] == "T2":
            continue
        if r["cap"] == "B7" and float(r["level_dbfs"]) + 21.0 > 12.0:
            continue  # unverified hot zone
        rows.append(r)
    floors = {}
    for h in HS:
        for cap in BOOSTS:
            if cap == "T2":
                continue
            vals = [float(r["H%d" % h]) for r in rows if r["cap"] == cap]
            floors[(cap, h)] = min(vals) if vals else -120.0
    return rows, floors


def fast_mirror(xs, k, a, oc, ow, ec, ew, gn0, gq, gated, amp):
    """Vectorized mirror: envelope settled at sine peak (attack 0.1 ms settles
    in ~480 samples; release ripple ~2% moves the gate ~1% — ladder impact
    negligible, verified by self_check below)."""
    norm = np.tanh(k)
    gate = (amp / (amp + gn0)) ** gq if gated else 1.0
    uo, ue = xs / ow, xs / ew
    br = gate * (oc * uo**3 * np.exp(-uo * uo) + ec * ue * ue * np.exp(-ue * ue))
    xe = np.clip(xs, -31.6, 31.6)
    return np.tanh(k * xs) / norm + a * xe * xe + br


def self_check():
    from fit_v3 import mirror as exact
    worst = 0.0
    for f, lv in ((440.0, -10.0), (55.0, -5.0), (1000.0, 0.0)):
        amp = 10 ** ((lv + 9.0) / 20.0)
        xs = amp * np.sin(2 * np.pi * f * np.arange(N + WARM) / FS)
        a = ladder(exact(xs, 0.06, 0.0015, 0.03475, 0.56, -0.05, 0.82, 10.526, 0.60, True)[-N:], f)
        b = ladder(fast_mirror(xs, 0.06, 0.0015, 0.03475, 0.56, -0.05, 0.82, 10.526, 0.60, True, amp)[-N:], f)
        worst = max(worst, max(abs(x - y) for x, y in zip(a, b)))
    print("self-check max ladder diff: %.3f dB" % worst, flush=True)
    assert worst < 0.5, worst


def hinge(x, rows, floors, free_idx, fixed, gated, offs):
    p = dict(fixed)
    for i, v in zip(free_idx, x):
        p[i] = v
    k, a, oc, ow, ec, ew, gn0, gq = (p[i] for i in range(8))
    errs = []
    for r in rows:
        f, lv = float(r["freq"]), float(r["level_dbfs"])
        amp = 10 ** ((lv + BOOSTS[r["cap"]] + offs.get(r["cap"], 0.0)) / 20.0)
        xs = amp * np.sin(2 * np.pi * f * np.arange(N + WARM) / FS)
        pred = ladder(fast_mirror(xs, k, a, oc, ow, ec, ew, gn0, gq, gated, amp)[-N:], f)
        for h in HS:
            got = float(r["H%d" % h])
            d = pred[h - 1] - got
            if got > floors[(r["cap"], h)] + 10.0:
                errs.append(d)
            else:
                errs.append(max(0.0, d - 3.0))
    return np.array(errs)


# (tag, x0, bounds, npar, gated, free_idx, fixed, offs)
M = lambda tag, x0, bo, npar, gated, free_idx, fixed, offs=None: (
    tag, x0, bo, npar, gated, free_idx, fixed, offs or {})

MODELS = [
    M("M0 static-2", [0.06, 0.0015], ([0.01, 1e-4], [0.30, 1e-2]), 2, False,
      (0, 1), {2: 0.0, 3: 0.56, 4: 0.0, 5: 0.82, 6: 10.526, 7: 0.60}),
    M("M1 +bumps", [0.06, 0.0015, 0.03475, 0.56, -0.05, 0.82],
      ([0.01, 1e-4, 0.0, 0.2, -0.2, 0.3], [0.30, 1e-2, 0.2, 2.0, 0.2, 2.0]), 6, False,
      (0, 1, 2, 3, 4, 5), {6: 10.526, 7: 0.60}),
    M("M1+off", [0.06, 0.0015, 0.03475, 0.56, -0.05, 0.82, 0.0, 0.0],
      ([0.01, 1e-4, 0.0, 0.2, -0.2, 0.3, -6.0, -6.0],
       [0.30, 1e-2, 0.2, 2.0, 0.2, 2.0, 6.0, 6.0]), 8, False,
      (0, 1, 2, 3, 4, 5, 8, 9),
      {6: 10.526, 7: 0.60, 8: 0.0, 9: 0.0}),
    M("M2 +gate", [0.06, 0.0015, 0.03475, 0.56, -0.05, 0.82, 10.526, 0.60],
      ([0.01, 1e-4, 0.0, 0.2, -0.2, 0.3, 1.0, 0.3],
       [0.30, 1e-2, 0.2, 2.0, 0.2, 2.0, 40.0, 1.5]), 8, True,
      (0, 1, 2, 3, 4, 5, 6, 7), {}),
]


def main():
    self_check()
    rows, floors = load()
    print("rows: %d (settled, B3/DI + B7mid)" % len(rows), flush=True)
    results = []
    for tag, x0, bounds, npar, gated, free_idx, fixed, offs in MODELS:
        if tag == "M1+off":
            def fun(x, rows, floors):
                d = dict(zip((8, 9), x[-2:]))
                offmap = {"B3": d[8], "DI": d[9]}
                full = dict(fixed)
                for i, v in zip(free_idx, x):
                    if i < 8:
                        full[i] = v
                k, a, oc, ow, ec, ew, gn0, gq = (full[i] for i in range(8))
                errs = []
                for r in rows:
                    f, lv = float(r["freq"]), float(r["level_dbfs"])
                    amp = 10 ** ((lv + BOOSTS[r["cap"]] + offmap.get(r["cap"], 0.0)) / 20.0)
                    xs = amp * np.sin(2 * np.pi * f * np.arange(N + WARM) / FS)
                    pred = ladder(fast_mirror(xs, k, a, oc, ow, ec, ew, gn0, gq, False, amp)[-N:], f)
                    for h in HS:
                        got = float(r["H%d" % h])
                        dd = pred[h - 1] - got
                        errs.append(dd if got > floors[(r["cap"], h)] + 10.0
                                    else max(0.0, dd - 3.0))
                return np.array(errs)
        sol = least_squares(fun if tag == "M1+off" else hinge, x0, bounds=bounds,
                            args=((rows, floors) if tag == "M1+off" else
                                  (rows, floors, free_idx, fixed, gated, offs)),
                            verbose=0, max_nfev=80)
        rss = float(np.sum(sol.fun**2))
        n = len(sol.fun)
        bic = n * np.log(rss / n) + npar * np.log(n)
        results.append((tag, float(np.sqrt(rss / n)), bic))
        line = "%s hinge-rmse %.2f dB BIC %.0f x=%s" % (
            tag, results[-1][1], bic, " ".join("%.4g" % v for v in sol.x))
        print(line, flush=True)
        with open(os.path.join(TMP, "fit_settled_results.txt"), "a") as fh:
            fh.write(line + "\n")
    with open(os.path.join(TMP, "fit_settled_results.txt"), "a") as fh:
        fh.write("BIC winner: %s\n" % min(results, key=lambda r: r[2])[0])
    print("BIC winner: %s" % min(results, key=lambda r: r[2])[0], flush=True)


if __name__ == "__main__":
    main()
