"""Throwaway spike probe 2: fit a gentle LS+HS tilt to the mean NAM-ours
FR delta (B3/B7 gain-matched). Reports corners/gains, residual, and the
flat-bypass gate cost."""
import sys

sys.path.insert(0, r"C:\Users\<you>\Code\Abalone-W5\analysis")
import numpy as np
from ir_check import rbj_cook

d = np.loadtxt(r"C:\Users\<you>\AppData\Local\Temp\opencode\namcmp.csv",
               delimiter=",", skiprows=1)
fr, nb3, nb7, ob3, ob7 = d.T
i1k = int(np.argmin(np.abs(fr - 1000.0)))
delta = ((nb3 - nb3[i1k]) - (ob3 - ob3[i1k]) + (nb7 - nb7[i1k]) - (ob7 - ob7[i1k])) / 2
print("target delta: LF<200 mean %+.2f  HF>8k mean %+.2f  rms %.2f" % (
    float(np.mean(delta[fr < 200])), float(np.mean(delta[fr > 8000])),
    float(np.sqrt(np.mean(delta ** 2)))))

FS = 48000.0


def shelf_resp(f, typ, f0, g):
    w = 2 * np.pi * np.asarray(f) / FS
    z = np.exp(1j * w)
    b0, b1, b2, a1, a2 = rbj_cook(typ, f0, 0.71, g, FS)
    h = (b0 + b1 / z + b2 / z / z) / (1.0 + a1 / z + a2 / z / z)
    return 20 * np.log10(np.abs(h))


best = None
for ls_fc in (80.0, 120.0, 160.0, 220.0):
    for ls_g in (0.3, 0.5, 0.7):
        for hs_fc in (8000.0, 10000.0, 12000.0):
            for hs_g in (-0.5, -0.3, -0.2):
                fit = shelf_resp(fr, "ls", ls_fc, ls_g) + shelf_resp(fr, "hs", hs_fc, hs_g)
                r = float(np.sqrt(np.mean((delta - fit) ** 2)))
                if best is None or r < best[0]:
                    best = (r, ls_fc, ls_g, hs_fc, hs_g)
print("best tilt: LS %gHz %+.1fdB + HS %gHz %+.1fdB, residual rms %.2f dB" % (
    best[1], best[2], best[3], best[4], best[0]))
print("flat-bypass gate cost: bypass would carry %+.2f LF / %+.2f HF (gate +-0.5)" % (
    best[2], best[4]))
