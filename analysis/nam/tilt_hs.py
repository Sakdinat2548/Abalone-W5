"""Throwaway spike: retune HS (LS75 locked) on median target."""
import sys

sys.path.insert(0, r"C:\Users\<you>\Code\Abalone-W5\analysis")
import numpy as np
from scipy.optimize import minimize
from ir_check import rbj_cook

FS = 48000.0
d = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt_ls.txt", skiprows=1)
fr, target = d[:, 0], d[:, 1]
GRID = fr
QFIX = 0.71


def resp(sections):
    w = 2 * np.pi * GRID / FS
    z = np.exp(1j * w)
    h = np.ones_like(GRID, dtype=complex)
    for typ, f0, q, g in sections:
        b0, b1, b2, a1, a2 = rbj_cook(typ, f0, q, g, FS)
        h *= (b0 + b1 / z + b2 / z / z) / (1.0 + a1 / z + a2 / z / z)
    return 20 * np.log10(np.abs(h))


def huber(r, dd=0.15):
    a = np.abs(r)
    return np.where(a < dd, 0.5 * r * r, dd * (a - 0.5 * dd))


def loss(p):
    fit = resp([("ls", 75.0, 0.54, 0.91), ("hs", p[0], p[2], p[1])])
    r = fit - target
    r1k = np.interp(np.log10(1000.0), np.log10(GRID), fit)
    l2 = np.diff(fit, 2)
    return float(np.mean(huber(r)) + 50.0 * r1k * r1k + 0.02 * np.mean(l2 * l2))


# per-capture curves for reporting (recompute from sources)
dd = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\namcmp.csv", delimiter=",", skiprows=1)
fr0, nb3, nb7, ob3, ob7 = dd.T
ddd = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\namdi_fr.csv", delimiter=",", skiprows=1)
frd, namd, ourd = ddd.T


def delta(fm, a, b):
    ia = int(np.argmin(abs(fm - 1000.0)))
    return np.interp(np.log10(GRID), np.log10(fm), (a - a[ia]) - (b - b[ia]))


t3 = delta(fr0, nb3, ob3)
t7 = delta(fr0, nb7, ob7)
td = delta(frd, namd, ourd)

r = minimize(loss, x0=np.array([8000.0, -0.5, 0.71]),
             bounds=[(3000, 16000), (-1.2, -0.05), (0.3, 2.0)],
             method="Nelder-Mead", options={"maxiter": 300})
fit = resp([("ls", 75.0, 0.54, 0.91), ("hs", r.x[0], r.x[2], r.x[1])])
print("HS retune: HS %.0fHz %+.2fdB Q%.2f (LS75 locked)" % tuple(r.x))
print("1kHz: %+.4f" % float(np.interp(np.log10(1000.0), np.log10(GRID), fit)))
for name, t in (("B3", t3), ("B7", t7), ("DI", td), ("median", target)):
    rr = fit - t
    print("  %-6s rmse %.3f max %.3f @ %.0f" % (
        name, float(np.sqrt(np.mean(rr ** 2))), float(np.max(abs(rr))),
        GRID[int(np.argmax(abs(rr)))]))
ok = True
for typ, f0, q, g in (("ls", 75.0, 0.54, 0.91), ("hs", r.x[0], r.x[2], r.x[1])):
    b0, b1, b2, a1, a2 = rbj_cook(typ, f0, q, g, FS)
    pr = float(np.max(abs(np.roots([1.0, a1, a2]))))
    ok = ok and pr < 1.0
print("stable:", ok)
np.savetxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt_hs.txt",
           np.column_stack([GRID, target, fit]), header="freq,target,hsretune", comments="")
print("wrote tilt_hs.txt")
