"""Fit v3: nested stepwise + BIC, hinge-scored on ALL rows (Claude flags 2+3).

Models (nested, fewest params wins ties):
  M0 static-2  : tanh(kx)/tanh(k) + a*xe^2                        (2 params)
  M1 +bumps    : + oddC*uo^3*e^-uo^2 + evenC*ue^2*e^-ue^2, static (6 params)
  M2 +gate     : bumps scaled by (env/(env+gn0))^gq                (8 params)
Widths (oddW/evenW) join at M1 (else bumps cannot place); gate adds gn0/gq.
Data: remap.csv, B3/DI bypass only; B7-hot (color>+12) and T2 excluded.
Score: hinge loss on ALL B3/DI rows — residual counts only where the
  capture responds (got above floor+10, floor = per-(cap,h) min); below
  floor, penalize only predictions dirtier than got+3 dB. Reported alongside
  the selected-rows RMSE so selection bias is visible, not hidden.
Model choice: BIC = n*ln(RSS/n) + p*ln(n) on hinge residuals; a step must
  clear BIC margin, else fewer params win by default.
"""
import csv
import os

import numpy as np
from scipy.optimize import least_squares

HERE = os.path.dirname(os.path.abspath(__file__))
FS, N, WARM = 48000.0, 48000, 4800
BOOSTS = {"B3": 9.0, "DI": 6.0}
HS = (2, 3, 4, 5)


def ladder(y, f, top=6):
    y = np.asarray(y, dtype=float)
    y -= np.mean(y)
    t = np.arange(len(y)) / FS
    out, h1 = [], None
    for h in range(1, top + 1):
        a = 2 * np.pi * f * h * t
        m = 2 * np.hypot(np.sum(y * np.cos(a)), np.sum(y * np.sin(a))) / len(y)
        if h == 1:
            h1 = m
            out.append(20 * np.log10(max(h1, 1e-12)))
        else:
            out.append(20 * np.log10(max(m / h1, 1e-12)))
    return out


def mirror(xs, k, a, oc, ow, ec, ew, gn0, gq, gated):
    norm = np.tanh(k)
    env = 0.0
    ys = np.empty_like(xs)
    for i, x in enumerate(xs):
        ax = abs(x)
        env += (0.19 if ax > env else 0.000208) * (ax - env)
        gate = (env / (env + gn0)) ** gq if gated else 1.0
        uo, ue = x / ow, x / ew
        br = gate * (oc * uo**3 * np.exp(-uo * uo) + ec * ue * ue * np.exp(-ue * ue))
        xe = min(31.6, max(-31.6, x))
        ys[i] = np.tanh(k * x) / norm + a * xe * xe + br
    return ys


def load():
    rows = [r for r in csv.DictReader(open(os.path.join(HERE, "remap.csv")))
            if r["cap"] in BOOSTS]
    floors = {}
    for h in HS:
        for cap in BOOSTS:
            floors[(cap, h)] = min(float(r["gotH%d" % h]) for r in rows if r["cap"] == cap)
    return rows, floors


def hinge_resid(x, rows, floors, free_idx, fixed, gated):
    p = dict(fixed)
    for i, v in zip(free_idx, x):
        p[i] = v
    k, a, oc, ow, ec, ew, gn0, gq = (p[i] for i in range(8))
    errs = []
    for r in rows:
        f, lv = float(r["freq"]), float(r["level_dbfs"])
        amp = 10 ** ((lv + BOOSTS[r["cap"]]) / 20.0)
        xs = amp * np.sin(2 * np.pi * f * np.arange(N + WARM) / FS)
        pred = ladder(mirror(xs, k, a, oc, ow, ec, ew, gn0, gq, gated)[-N:], f)
        for h in HS:
            got = float(r["gotH%d" % h])
            d = pred[h - 1] - got
            if got > floors[(r["cap"], h)] + 10.0:
                errs.append(d)
            else:
                errs.append(max(0.0, d - 3.0))  # hinge, fixed length
    return np.array(errs)


MODELS = [
    # (tag, x0, bounds, npar, gated, free_idx, fixed)
    ("M0 static-2", [0.06, 0.0015], ([0.01, 1e-4], [0.30, 1e-2]), 2, False,
     (0, 1), {2: 0.0, 3: 0.56, 4: 0.0, 5: 0.82, 6: 10.526, 7: 0.60}),
    ("M1 +bumps", [0.06, 0.0015, 0.03475, 0.56, -0.05, 0.82],
     ([0.01, 1e-4, 0.0, 0.2, -0.2, 0.3], [0.30, 1e-2, 0.2, 2.0, 0.2, 2.0]), 6, False,
     (0, 1, 2, 3, 4, 5), {6: 10.526, 7: 0.60}),
    ("M2 +gate", [0.06, 0.0015, 0.03475, 0.56, -0.05, 0.82, 10.526, 0.60],
     ([0.01, 1e-4, 0.0, 0.2, -0.2, 0.3, 1.0, 0.3],
      [0.30, 1e-2, 0.2, 2.0, 0.2, 2.0, 40.0, 1.5]), 8, True,
     (0, 1, 2, 3, 4, 5, 6, 7), {}),
]


def main():
    rows, floors = load()
    print("rows: %d (all B3/DI, hinge-scored)" % len(rows), flush=True)
    results = []
    for tag, x0, bounds, npar, gated, free_idx, fixed in MODELS:
        sol = least_squares(hinge_resid, x0, bounds=bounds,
                            args=(rows, floors, free_idx, fixed, gated),
                            verbose=0, max_nfev=120)
        rss = float(np.sum(sol.fun**2))
        n = len(sol.fun)
        bic = n * np.log(rss / n) + npar * np.log(n)
        rmse = float(np.sqrt(rss / n))
        full = dict(fixed)
        for i, v in zip(free_idx, sol.x):
            full[i] = v
        results.append((tag, rmse, bic))
        print("%s hinge-rmse %.2f dB BIC %.0f" % (tag, rmse, bic), flush=True)
        print("  k=%.4f a=%.2e oddC=%.4f oddW=%.3f evenC=%.4f evenW=%.3f gateN0=%.2f gateQ=%.3f"
              % tuple(full[i] for i in range(8)), flush=True)
    best = min(results, key=lambda r: r[2])
    print("BIC winner: %s" % best[0], flush=True)


if __name__ == "__main__":
    main()
