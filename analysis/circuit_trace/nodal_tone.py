"""Nodal AC analysis of rsasgtr's LTSpice tone sim (read-only adjudication).
Full coupled network: all six cells hang on common rail A, driven by ideal
Vs=0.316 through R14=1k (exactly like the .asc). Open-circuit outputs."""
import csv, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))  # analysis/ for ir_check
from ir_check import tone_magnitude_at, log_interp

K, N, P, MEG = 1e3, 1e-9, 1e-12, 1e6
R14, VS = 1.0*K, 0.316

# Netlist: (nodeA, nodeB, R|xC). 'A' = common rail. '0' = GND.
NETS = {
 1: [("A","O1",180*P,"C"),("A","T1V2",100*K,"R"),("A","T1V2",100*K,"R"),
     ("T1V2","T1V4",22*N,"C"),("T1V4","O1",100*K,"R"),
     ("T1V2","T1V3",22*N,"C"),("T1V4","T1V3",1*MEG,"R"),("T1V3","0",27.32*K,"R"),
     ("O1","0",1*MEG,"R")],
 2: [("A","O2",470*P,"C"),("A","T2A",56*K,"R"),("T2A","T2B",22*N,"C"),
     ("T2B","O2",100*K,"R"),("T2A","0",22*N,"C"),("T2B","0",1*MEG,"R")],
 3: [("A","T3M",180*P,"C"),("T3M","O3",39.2*K,"R"),("A","T3V2",10*K,"R"),
     ("T3V2","T3V4",50*N,"C"),("T3V2","T3X",220*N,"C"),("O3","T3V4",39.1*K,"R"),
     ("T3V4","T3X",50*K,"R"),("T3X","0",10*K,"R")],
 4: [("A","O4",180*P,"C"),("A","T4A",26.4*K,"R"),("T4A","T4B",22*N,"C"),
     ("T4B","O4",100*K,"R"),("T4A","0",820*P,"C"),("T4B","0",1*MEG,"R")],
 5: [("A","O5",9*N,"C"),("O5","0",95*K,"R")],
 6: [("A","T6A",9.9*K,"R"),("T6A","T6B",10*N,"C"),("T6B","O6",100*K,"R"),
     ("T6B","0",95*K,"R"),("O6","0",180*P,"C")],
}
# Note T1: RN18(A-T1V2) + RN12(A-T1V2b) with T1V2b shorted to T1V2 (=2x100k in
# parallel, kept as separate branches for netlist fidelity).

nodes = sorted({n for br in NETS.values() for b in br for n in b[:2]
                        if n != "0"})
idx = {n: i for i, n in enumerate(nodes)}
OUT = {t: idx["O%d" % t] for t in range(1, 7)}
NN = len(nodes)

def solve(freqs):
    out = np.zeros((6, len(freqs)))
    for j, f in enumerate(freqs):
        w = 2*math.pi*f
        Y = np.zeros((NN, NN), complex)
        I = np.zeros(NN, complex)
        Y[idx["A"], idx["A"]] += 1/R14
        I[idx["A"]] += VS/R14
        for br in NETS.values():
            for a, b, v, k in br:
                y = 1e12 if k == "W" else (1/v if k == "R" else 1j*w*v)
                ia, ib = idx[a], idx.get(b)
                Y[ia, ia] += y
                if ib is not None:
                    Y[ib, ib] += y; Y[ia, ib] -= y; Y[ib, ia] -= y
        V = np.linalg.solve(Y, I)
        for t in range(1, 7):
            out[t-1, j] = abs(V[OUT[t]])
    return out

freqs = np.exp(np.linspace(math.log(10), math.log(20000), 600))
Vnodal = solve(freqs)
Hnod = 20*np.log10(Vnodal)
Hnod_n = np.array([(Hnod[t]-float(log_interp(1000.0, freqs, Hnod[t])))
                   for t in range(6)])

# Absolute-level sanity markers from transcription (unnormalized, @~10 Hz)
print("== absolute level check (model vs sim/measured markers) ==")
for t, m in zip(range(1,7), [-18.62,-18.58,-19.16,-18.26,-35.98,-37.36]):
    v10 = abs(solve(np.array([10.0]))[t-1,0])
    print(" T%d: marker %6.2f | 20log|Vout|=%6.2f  20log|Vout/0.316|=%6.2f"
          % (t, m, 20*math.log10(v10), 20*math.log10(v10/VS)))

# CSV oracle
cf, cc = [], {t: [] for t in range(1, 7)}
with open(os.path.join(HERE, "..", "u5_tone_curves_from_claude.csv"),
          newline="") as fh:
    for row in csv.DictReader(fh):
        cf.append(float(row["freq_hz"]))
        for t in range(1, 7):
            cc[t].append(float(row["curve%d_db" % t]))
cf = np.array(cf)
Hcsv = np.array([log_interp(freqs, cf, np.array(cc[t])) for t in range(1, 7)])
Hcsv_n = np.array([(Hcsv[t]-float(log_interp(1000.0, freqs, Hcsv[t])))
                   for t in range(6)])

# Our DSP biquads @48k
Hbiq = np.array([[tone_magnitude_at(t, f, 48000.0) for f in freqs]
                 for t in range(1, 7)])
Hbiq_n = np.array([(Hbiq[t]-float(log_interp(1000.0, freqs, Hbiq[t])))
                   for t in range(6)])

def stats(d, mask):
    a = np.abs(d[mask]); return a.max(), float(np.sqrt((a*a).mean()))

BANDS = {"40-15k": (freqs>=40)&(freqs<=15000),
         "full 10-20k": np.ones(len(freqs), bool),
         "10-100": (freqs<100), "100-1k": (freqs>=100)&(freqs<1000),
         "1k-8k": (freqs>=1000)&(freqs<8000), "8k-20k": (freqs>=8000)}
PAIRS = {"nodal-CSV": (Hnod_n, Hcsv_n), "nodal-biq": (Hnod_n, Hbiq_n),
         "biq-CSV": (Hbiq_n, Hcsv_n)}
print("\n== per-tone max/RMS (dB), 1kHz-normalized ==")
for t in range(6):
    print("--- T%d ---" % (t+1))
    for pn, (A, B) in PAIRS.items():
        d = A[t]-B[t]
        mg, rg = stats(d, BANDS["40-15k"]); mf, rf = stats(d, BANDS["full 10-20k"])
        print(" %-9s gate: max %5.2f rms %5.3f | full: max %5.2f rms %5.3f"
              % (pn, mg, rg, mf, rf))
    print(" band detail (max |d|, nodal-CSV / nodal-biq / biq-CSV):")
    for bn in ["10-100","100-1k","1k-8k","8k-20k"]:
        m = BANDS[bn]
        ds = [abs((A[t]-B[t])[m]).max() for A, B in PAIRS.values()]
        print("   %-7s %5.2f / %5.2f / %5.2f" % (bn, *ds))
