import math, os, sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))  # analysis/ for ir_check
src = open(os.path.join(HERE, 'nodal_tone.py')).read()
head = src.split('freqs = ')[0]
g = {}
exec(head, g)
NETS, idx, nodes, OUT, NN, R14, VS = (g['NETS'], g['idx'], g['nodes'], g['OUT'], g['NN'], g['R14'], g['VS'])
from ir_check import tone_magnitude_at, log_interp


def solve_general(nets, freqs, probe=None):
    used = {"A"} | {n for br in nets.values() for b in br for n in b[:2]
                    if n != "0"}
    li = {n: i for i, n in enumerate(sorted(used))}
    nn = len(li)
    out = np.zeros((6, len(freqs)))
    for j, f in enumerate(freqs):
        w = 2 * math.pi * f
        Y = np.zeros((nn, nn), complex)
        I = np.zeros(nn, complex)
        Y[li["A"], li["A"]] += 1 / R14
        I[li["A"]] += VS / R14
        for t, br in nets.items():
            for a, b, v, k in br:
                y = 1 / v if k == "R" else 1j * w * v
                ia, ib = li[a], li.get(b)
                Y[ia, ia] += y
                if ib is not None:
                    Y[ib, ib] += y
                    Y[ia, ib] -= y
                    Y[ib, ia] -= y
        if probe:
            for t in nets:
                o = li["O%d" % t]
                Y[o, o] += 1 / probe
        V = np.linalg.solve(Y, I)
        for t in nets:
            out[t-1, j] = abs(V[li["O%d" % t]])
    return out


freqs = np.exp(np.linspace(math.log(10), math.log(20000), 600))
Vfull = solve_general(NETS, freqs)
Vprobe = solve_general(NETS, freqs, probe=1e6)
Hfull = 20 * np.log10(Vfull)
Hprobe = 20 * np.log10(Vprobe)
nfull = np.array([(Hfull[t] - float(log_interp(1000.0, freqs, Hfull[t])))
                  for t in range(6)])
nprobe = np.array([(Hprobe[t] - float(log_interp(1000.0, freqs, Hprobe[t])))
                   for t in range(6)])
print("== sensitivity: full-network vs 1Meg-probe-loaded (norm, max|d| gate/full) ==")
for t in range(6):
    d = nfull[t] - nprobe[t]
    m = (freqs >= 40) & (freqs <= 15000)
    print(" T%d: gate %.3f full %.3f" % (t + 1, abs(d[m]).max(), abs(d).max()))
print("== sensitivity: full-network vs isolated-cell (norm, max|d| gate/full) ==")
iso = np.zeros_like(Vfull)
for t in range(1, 7):
    Vi = solve_general({t: NETS[t]}, freqs)
    iso[t-1] = Vi[t-1]
Hiso = 20 * np.log10(iso)
niso = np.array([(Hiso[t] - float(log_interp(1000.0, freqs, Hiso[t])))
                  for t in range(6)])
for t in range(6):
    d = nfull[t] - niso[t]
    m = (freqs >= 40) & (freqs <= 15000)
    print(" T%d: gate %.3f full %.3f" % (t + 1, abs(d[m]).max(), abs(d).max()))

Hbiq = np.array([[tone_magnitude_at(t, f, 48000.0) for f in freqs]
                 for t in range(1, 7)])
nbiq = np.array([(Hbiq[t] - float(log_interp(1000.0, freqs, Hbiq[t])))
                 for t in range(6)])
SPOTS = [10, 20, 40, 100, 200, 500, 1000, 2000, 5000, 8000, 10000, 15000, 20000]
print("== signed nodal-minus-biq (dB) at spot freqs, 1kHz-normalized ==")
print("tone " + "".join("%8d" % s for s in SPOTS))
for t in range(6):
    d = nfull[t] - nbiq[t]
    print("T%d  " % (t + 1) + "".join("%8.2f" % float(log_interp(s, freqs, d))
                                      for s in SPOTS))
print("== worst nodal-vs-biq in gate band: freq + signed delta ==")
m = (freqs >= 40) & (freqs <= 15000)
for t in range(6):
    d = (nfull[t] - nbiq[t])[m]
    fm = freqs[m]
    i = int(abs(d).argmax())
    print(" T%d: %7.0f Hz  %+5.2f dB" % (t + 1, fm[i], d[i]))
