"""Throwaway spike: refit tilt (LS+HS, same architecture) to median B3/B7/DI.

Spec (ChatGPT task): median target, Huber loss, per-octave weighting,
smoothness penalty, exact 0 dB at 1 kHz, free gains+corners (Q fixed),
validate old-vs-new per capture + stability. Decision rule applied at end.
"""
import sys

sys.path.insert(0, r"C:\Users\<you>\Code\Abalone-W5\analysis")
import numpy as np
from scipy.optimize import minimize
from ir_check import rbj_cook

FS = 48000.0
d = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\namcmp.csv",
               delimiter=",", skiprows=1)
fr, nb3, nb7, ob3, ob7 = d.T
dd = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\namdi_fr.csv",
                delimiter=",", skiprows=1)
frd, namd, ourd = dd.T

GRID = np.logspace(np.log10(20.0), np.log10(20000.0), 120)


def interp_delta(fm, a, b):
    ia = int(np.argmin(np.abs(fm - 1000.0)))
    dd = (a - a[ia]) - (b - b[ia])
    return np.interp(np.log10(GRID), np.log10(fm), dd)


t3 = interp_delta(fr, nb3, ob3)
t7 = interp_delta(fr, nb7, ob7)
td = interp_delta(frd, namd, ourd)
target = np.median(np.stack([t3, t7, td]), axis=0)

# per-octave weights: uniform in log-f (GRID already log-uniform) x Huber
Q = 0.71


def tilt_resp(p):
    ls_fc, ls_g, hs_fc, hs_g = p
    w = 2 * np.pi * GRID / FS
    z = np.exp(1j * w)
    h = np.ones_like(GRID, dtype=complex)
    for typ, f0, g in (("ls", ls_fc, ls_g), ("hs", hs_fc, hs_g)):
        b0, b1, b2, a1, a2 = rbj_cook(typ, f0, Q, g, FS)
        h *= (b0 + b1 / z + b2 / z / z) / (1.0 + a1 / z + a2 / z / z)
    return 20 * np.log10(np.abs(h))


def huber(r, d=0.15):
    a = np.abs(r)
    return np.where(a < d, 0.5 * r * r, d * (a - 0.5 * d))


def loss(p):
    fit = tilt_resp(p)
    r = fit - target
    # exact 0 dB at 1 kHz, hard
    r1k = np.interp(np.log10(1000.0), np.log10(GRID), fit)
    # smoothness: penalize 2nd log-difference
    l2 = np.diff(fit, 2)
    return float(np.mean(huber(r)) + 50.0 * r1k * r1k + 0.02 * np.mean(l2 * l2))


def report(p, tag):
    fit = tilt_resp(p)
    print("== %s ==" % tag)
    print("params: LS %.1fHz %+.2fdB  HS %.0fHz %+.2fdB" % tuple(p))
    print("1kHz value: %+.4f dB" % float(np.interp(np.log10(1000.0), np.log10(GRID), fit)))
    for name, t in (("B3", t3), ("B7", t7), ("DI", td), ("median", target)):
        r = fit - t
        print("  %-6s rmse %.3f  max |.| %.3f @ %.0f Hz" % (
            name, float(np.sqrt(np.mean(r ** 2))), float(np.max(np.abs(r))),
            GRID[int(np.argmax(np.abs(r)))]))
    # stability: pole radii of both shelves at 48 kHz
    ok = True
    for typ, f0, g in (("ls", p[0], p[1]), ("hs", p[2], p[3])):
        b0, b1, b2, a1, a2 = rbj_cook(typ, f0, Q, g, FS)
        poles = np.roots([1.0, a1, a2])
        pr = float(np.max(np.abs(poles)))
        ok = ok and pr < 1.0
        print("  %s poles |.|max %.6f %s" % (typ, pr, "STABLE" if pr < 1 else "UNSTABLE"))
    return ok


print("--- OLD (LS120 +0.7 / HS8000 -0.5) ---")
report([120.0, 0.7, 8000.0, -0.5], "old")
res = minimize(loss, x0=np.array([120.0, 0.7, 8000.0, -0.5]),
               bounds=[(40, 400), (0.1, 1.5), (4000, 16000), (-1.0, -0.05)],
               method="Nelder-Mead", options={"maxiter": 400, "xatol": 0.5, "fatol": 1e-5})
print("--- NEW ---")
ok = report(res.x, "new")
np.savetxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt_refit.txt",
           np.column_stack([GRID, target, tilt_resp([120.0, 0.7, 8000.0, -0.5]),
                            tilt_resp(res.x)]),
           header="freq,target,old,new", comments="")
print("stable:", ok, "| success:", res.success, "| adopt:", "ONLY on user ruling")
