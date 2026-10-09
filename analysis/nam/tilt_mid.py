"""Throwaway spike: one extra low-mid filter on the 90Hz-1kHz deviation.
Base locked: LS75/+0.91/Q0.54 + HS5441/-0.34/Q0.62. Extra: free peak.
"""
import sys

sys.path.insert(0, r"C:\Users\<you>\Code\Abalone-W5\analysis")
import numpy as np
from scipy.optimize import minimize
from ir_check import rbj_cook

FS = 48000.0
d = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt_hs.txt", skiprows=1)
GRID, target = d[:, 0], d[:, 1]
BASE = [("ls", 75.0, 0.54, 0.91), ("hs", 5441.0, 0.62, -0.34)]


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
    fit = resp(BASE + [("pk", p[0], p[2], p[1])])
    r = fit - target
    r1k = np.interp(np.log10(1000.0), np.log10(GRID), fit)
    l2 = np.diff(fit, 2)
    return float(np.mean(huber(r)) + 50.0 * r1k * r1k + 0.02 * np.mean(l2 * l2))


dd = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\namcmp.csv", delimiter=",", skiprows=1)
fr, nb3, nb7, ob3, ob7 = dd.T
ddd = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\namdi_fr.csv", delimiter=",", skiprows=1)
frd, namd, ourd = ddd.T


def delta(fm, a, b):
    ia = int(np.argmin(abs(fm - 1000.0)))
    return np.interp(np.log10(GRID), np.log10(fm), (a - a[ia]) - (b - b[ia]))


t3 = delta(fr, nb3, ob3)
t7 = delta(fr, nb7, ob7)
td = delta(frd, namd, ourd)

# show base residual in 90-1000 to justify the extra filter
base = resp(BASE)
band = (GRID >= 90) & (GRID <= 1000)
print("base residual 90-1k: rms %.3f max %.3f" % (
    float(np.sqrt(np.mean((base - target)[band] ** 2))),
    float(np.max(np.abs((base - target)[band])))))

r = minimize(loss, x0=np.array([300.0, 0.15, 1.0]),
             bounds=[(60, 1500), (-0.8, 0.8), (0.3, 3.0)],
             method="Nelder-Mead", options={"maxiter": 300})
fit = resp(BASE + [("pk", r.x[0], r.x[2], r.x[1])])
print("extra: PK %.0fHz %+.2fdB Q%.2f" % tuple(r.x))
print("1kHz: %+.4f" % float(np.interp(np.log10(1000.0), np.log10(GRID), fit)))
for name, t in (("B3", t3), ("B7", t7), ("DI", td), ("median", target)):
    rr = fit - t
    print("  %-6s rmse %.3f max %.3f @ %.0f" % (
        name, float(np.sqrt(np.mean(rr ** 2))), float(np.max(abs(rr))),
        GRID[int(np.argmax(abs(rr)))]))
ok = True
for typ, f0, q, g in BASE + [("pk", r.x[0], r.x[2], r.x[1])]:
    b0, b1, b2, a1, a2 = rbj_cook(typ, f0, q, g, FS)
    pr = float(np.max(abs(np.roots([1.0, a1, a2]))))
    ok = ok and pr < 1.0
print("stable:", ok)
np.savetxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\tilt_mid.txt",
           np.column_stack([GRID, target, base, fit]),
           header="freq,target,base,plusmid", comments="")
print("wrote tilt_mid.txt")
