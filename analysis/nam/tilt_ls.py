"""Throwaway spike: retune LS (HS frozen) + optional one extra filter.
Same target/objective discipline as tilt_refit.py. Decision-grade output only.
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

QFIX = 0.71


def shelf_resp(f, sections):
    w = 2 * np.pi * np.asarray(f) / FS
    z = np.exp(1j * w)
    h = np.ones_like(f, dtype=complex)
    for typ, f0, q, g in sections:
        b0, b1, b2, a1, a2 = rbj_cook(typ, f0, q, g, FS)
        h *= (b0 + b1 / z + b2 / z / z) / (1.0 + a1 / z + a2 / z / z)
    return 20 * np.log10(np.abs(h))


def huber(r, d=0.15):
    a = np.abs(r)
    return np.where(a < d, 0.5 * r * r, d * (a - 0.5 * d))


def mk_loss(lowtype, extra=None):
    def loss(p):
        secs = [(lowtype, p[0], p[2], p[1]), ("hs", 8000.0, QFIX, -0.5)]
        if extra:
            secs.append((extra, p[3], p[5], p[4]))
        fit = shelf_resp(GRID, secs)
        r = fit - target
        r1k = np.interp(np.log10(1000.0), np.log10(GRID), fit)
        l2 = np.diff(fit, 2)
        return float(np.mean(huber(r)) + 50.0 * r1k * r1k + 0.02 * np.mean(l2 * l2))
    return loss


def report(p, lowtype, tag):
    secs = [(lowtype, p[0], p[2], p[1]), ("hs", 8000.0, QFIX, -0.5)]
    desc = "%s %.0fHz %+.2fdB Q%.2f + HS8k -0.5" % (lowtype.upper(), p[0], p[1], p[2])
    fit = shelf_resp(GRID, secs)
    print("== %s: %s ==" % (tag, desc))
    print("1kHz: %+.4f" % float(np.interp(np.log10(1000.0), np.log10(GRID), fit)))
    for name, t in (("B3", t3), ("B7", t7), ("DI", td), ("median", target)):
        r = fit - t
        print("  %-6s rmse %.3f max %.3f @ %.0f" % (
            name, float(np.sqrt(np.mean(r ** 2))), float(np.max(np.abs(r))),
            GRID[int(np.argmax(np.abs(r)))]))
    ok = True
    for typ, f0, q, g in secs:
        b0, b1, b2, a1, a2 = rbj_cook(typ, f0, q, g, FS)
        pr = float(np.max(np.abs(np.roots([1.0, a1, a2]))))
        ok = ok and pr < 1.0
    print("  stable:", ok)
    return fit


print("--- stage 1: LS free, HS frozen ---")
r1 = minimize(mk_loss("ls"), x0=np.array([120.0, 0.7, 0.71]),
              bounds=[(40, 400), (0.1, 1.5), (0.3, 2.0)],
              method="Nelder-Mead", options={"maxiter": 300})
f1 = report(r1.x, "ls", "LS-retuned")
print("--- stage 2: low PEAK instead of shelf (returns to flat) + HS frozen ---")
r2 = minimize(mk_loss("pk"), x0=np.array([110.0, 0.7, 1.0]),
              bounds=[(40, 400), (0.1, 1.5), (0.3, 3.0)],
              method="Nelder-Mead", options={"maxiter": 300})
f2 = report(r2.x, "pk", "low-peak")
np.savetxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt_ls.txt",
           np.column_stack([GRID, target, f1, f2]),
           header="freq,target,ls_only,lowpeak", comments="")
print("wrote tilt_ls.txt")
