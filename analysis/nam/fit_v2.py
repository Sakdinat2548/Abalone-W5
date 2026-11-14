"""Fit v2 step 1: refit current ColorStage architecture on trustworthy rows.

Null hypothesis: tanh(kx)/tanh(k) + a*xe^2 + gate*(oddC*uo^3*e^-uo^2 +
evenC*ue^2*e^-ue^2) with fitted (k, a, oddC, oddW, evenC, evenW, gateN0,
gateQ) closes captures within noise. Compares static-only vs gated loss.
Data: analysis/nam/remap.csv (196 rows). Trustworthy gate: B3/DI bypass
captures only; row kept when gotH2 AND gotH3 both rise >=3 dB over the next
quieter level at same (cap, freq) — i.e. responding, not floored. B7-hot
(hallucination) and T2 (separate Block2 step) excluded here.
"""
import csv
import os

import numpy as np
from scipy.optimize import least_squares

HERE = os.path.dirname(os.path.abspath(__file__))
FS, N, WARM = 48000.0, 48000, 4800
BOOSTS = {"B3": 9.0, "DI": 6.0}


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


def mirror(xs, p, gated):
    k, a, oc, ow, ec, ew, gn0, gq = p
    norm = np.tanh(k)
    env = 0.0
    # attack/release shape fixed (timing untouched per dyn battery); only
    # settled level behavior is fitted here.
    ys = np.empty_like(xs)
    for i, x in enumerate(xs):
        ax = abs(x)
        env += (0.19 if ax > env else 0.000208) * (ax - env)
        gate = (env / (env + gn0)) ** gq if gated else 0.0
        uo, ue = x / ow, x / ew
        br = gate * (oc * uo**3 * np.exp(-uo * uo) + ec * ue * ue * np.exp(-ue * ue))
        xe = min(31.6, max(-31.6, x))
        ys[i] = np.tanh(k * x) / norm + a * xe * xe + br
    return ys


def load():
    rows = list(csv.DictReader(open(os.path.join(HERE, "remap.csv"))))
    bykey = {}
    for r in rows:
        if r["cap"] not in BOOSTS:
            continue
        bykey.setdefault((r["cap"], r["freq"]), []).append(r)
    keep = []
    for key, rs in bykey.items():
        rs.sort(key=lambda r: float(r["level_dbfs"]))
        for i, r in enumerate(rs):
            if i == 0:
                continue  # need quieter neighbor to prove response
            q = rs[i - 1]
            if float(r["gotH2"]) - float(q["gotH2"]) >= 3.0 and \
               float(r["gotH3"]) - float(q["gotH3"]) >= 3.0:
                keep.append(r)
    return keep


def errvec(p, rows, gated, hs=(2, 3, 4, 5)):
    errs = []
    for r in rows:
        f, lv = float(r["freq"]), float(r["level_dbfs"])
        amp = 10 ** ((lv + BOOSTS[r["cap"]]) / 20.0)
        xs = amp * np.sin(2 * np.pi * f * np.arange(N + WARM) / FS)
        pred = ladder(mirror(xs, p, gated)[-N:], f)
        for h in hs:
            errs.append(float(r["gotH%d" % h]) - pred[h - 1])
    return np.array(errs)


P0 = [0.06, 0.0015, 0.03475, 0.56, -0.05, 0.82, 10.526, 0.60]
BOUNDS = ([0.01, 1e-4, 0.0, 0.2, -0.2, 0.3, 1.0, 0.3],
          [0.30, 1e-2, 0.2, 2.0, 0.2, 2.0, 40.0, 1.5])


def main():
    rows = load()
    print("trustworthy rows: %d" % len(rows), flush=True)
    base = errvec(P0, rows, True)
    print("shipped params rmse: %.2f dB" % float(np.sqrt(np.mean(base**2))), flush=True)
    for gated, tag in ((True, "gated"), (False, "static")):
        sol = least_squares(errvec, P0, bounds=BOUNDS, args=(rows, gated),
                            verbose=0, max_nfev=200)
        rmse = float(np.sqrt(np.mean(sol.fun**2)))
        print("%s fit rmse: %.2f dB" % (tag, rmse), flush=True)
        print("  k=%.4f a=%.2e oddC=%.4f oddW=%.3f evenC=%.4f evenW=%.3f "
              "gateN0=%.2f gateQ=%.3f" % tuple(sol.x), flush=True)
        if tag == "gated":
            best = sol.x
    # per-harmonic residual of winner
    w = errvec(best, rows, True)
    per = {}
    i = 0
    for r in rows:
        for h in (2, 3, 4, 5):
            per.setdefault(h, []).append(abs(w[i]))
            i += 1
    print("winner mean|resid|: " + " ".join(
        "H%d %.1fdB" % (h, float(np.mean(per[h]))) for h in (2, 3, 4, 5)), flush=True)


if __name__ == "__main__":
    main()
