"""U5 tone-board trace: first-principles passive-RC theory vs three oracles.

Reads values.csv (transcription w/ confidence) + digitized CSV oracle +
DSP biquad fits + local Tone3000 IRs (read-only), fits a minimal passive
shelf/notch network per tone from first principles (numpy only), and writes
one overlay PNG per tone + verdict_table.csv.

Unknowns stay unknown: transcribed values are NOT used as network values
(none are `clear`); the fit instead reports the REQUIRED RC products
(corner taus + divider ratios) each tone's network must realize, plus a
sensitivity ranking (which param moves curves most) for evidence shopping.
"""
import csv
import os
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
CSV_ORACLE = os.path.join(REPO, "analysis", "u5_tone_curves_digitized.csv")
BIQUADS = os.path.join(REPO, "analysis", "u5_tone_biquads.json")
IR_DIR = os.path.join(REPO, "analysis", "ir_local")

# --- first-principles passive blocks (voltage dividers, s-domain) ---
# Low shelf: series R1 (bypassed by C at HF), shunt R2.
#   H(s) = G0*(1+s*R1*C)/(1+s*(R1||R2)*C); G0 = R2/(R1+R2) < 1.
#   Param: fz (zero Hz), G0db (DC gain dB); pole fp = fz/G0lin.
def lowshelf(f, fz, g0db):
    g0 = 10.0 ** (g0db / 20.0)
    s = 1j * f / fz
    return g0 * (1 + s) / (1 + s * g0)


# High shelf (treble cut above fp, flat LF): series R1, shunt R2 + series C.
#   H(s) = (1+s*R2*C)/(1+s*(R1+R2)*C); HFinf = R2/(R1+R2).
#   Param: fp (pole Hz), ginfdb (HF gain dB); zero fz = fp/ginflin.
#   Treble *boost* relative to LF is the same shape with ginfdb > 0
#   (passive realization then needs overall divider loss, absorbed in gain).
def highshelf(f, fp, ginfdb):
    ginf = 10.0 ** (ginfdb / 20.0)
    s = 1j * f / fp
    return (1 + s * ginf) / (1 + s)


# Passive notch (bridged-T family): zero pair Q k-times sharper than poles.
#   H(s) = (s^2+s*w0/(Q*k)+w0^2)/(s^2+s*w0/Q+w0^2); depth ~ 20log10(1/k).
def notch(f, f0, q, depthdb):
    k = 10.0 ** (depthdb / 20.0)  # depthdb > 0 means cut depth
    w = f / f0
    s = 1j * w
    return (1 - w * w + s / (q * k)) / (1 - w * w + s / q)


def db(x):
    return 20 * np.log10(np.maximum(np.abs(x), 1e-12))


# --- oracles ---
def load_csv():
    d = np.genfromtxt(CSV_ORACLE, delimiter=",", skip_header=1)
    return d[:, 0], {n + 1: d[:, n + 1] for n in range(6)}


def dsp_response(sections, f, fs=48000.0):
    w = 2 * np.pi * f / fs
    z = np.exp(1j * w)
    h = np.ones_like(f, dtype=complex)
    for b0, b1, b2, a0, a1, a2 in sections:
        h *= (b0 + b1 / z + b2 / z / z) / (a0 + a1 / z + a2 / z / z)
    return h


def load_dsp():
    import json
    with open(BIQUADS) as fh:
        return json.load(fh)["fits"]["48000"]


def read_wav(path):
    with wave.open(path, "rb") as w:
        n, ch, sw, fr = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
        raw = w.readframes(n)
    if sw == 3:  # 24-bit: sign-extend to int32
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        x = (b[:, 0].astype(np.int32) | (b[:, 1].astype(np.int32) << 8)
             | (b[:, 2].astype(np.int32) << 16))
        x = np.where(x >= 1 << 23, x - (1 << 24), x).reshape(-1, ch).mean(axis=1)
    else:
        dt = {1: np.int8, 2: np.int16, 4: np.int32}[sw]
        x = np.frombuffer(raw, dtype=dt).reshape(-1, ch).mean(axis=1).astype(float)
    x = np.asarray(x, dtype=float)
    x /= np.max(np.abs(x)) + 1e-12
    return x, fr


def ir_shape(n, fgrid):
    """|FFT(TONEn)|/|FFT(TONE0)| in dB, 1/3-oct median-smoothed, 0dB at 1kHz.

    Mirrors analysis/ir_check.py (whole file = IR, no window, 1kHz norm)."""
    x0, fs = read_wav(os.path.join(IR_DIR, "AVALON_TONE0.wav"))
    xn, _ = read_wav(os.path.join(IR_DIR, "AVALON_TONE%d.wav" % n))
    m = min(len(x0), len(xn))
    N = 1
    while N < m:
        N *= 2
    H0 = np.abs(np.fft.rfft(x0[:m], N))
    Hn = np.abs(np.fft.rfft(xn[:m], N))
    fr = np.fft.rfftfreq(N, 1.0 / fs)
    ratio = 20 * np.log10(Hn / np.maximum(H0, 1e-9) + 1e-12)
    out = np.zeros_like(fgrid)
    for i, fc in enumerate(fgrid):
        lo, hi = fc / 2 ** (1.0 / 6), fc * 2 ** (1.0 / 6)
        band = (fr >= lo) & (fr < hi) & (fr > 5)
        out[i] = np.median(ratio[band]) if np.any(band) else np.nan
    return out - np.interp(1000.0, fgrid, out)


# --- per-tone model structures: (blocks, initial params) ---
# params: [ls_fz, ls_g0, n_f0, n_q, n_depth, hs_fp, hs_ginf, gain]
# T5/T6 use two cascaded low shelves (two-stage RC low skirt); the second
# shelf reuses slots n_f0/n_q? No — dedicated: ls2_fz, ls2_g0 appended.
MODELS = {
    1: (["ls", "n", "hs"], [90.0, -1.0, 700.0, 0.8, 8.0, 7000.0, 1.0, 0.5]),
    2: (["ls", "n", "hs"], [120.0, -1.0, 730.0, 3.0, 22.0, 9000.0, 0.5, 0.8]),
    3: (["ls", "n", "hs"], [110.0, -1.0, 750.0, 0.7, 4.5, 8000.0, 0.5, 0.3]),
    4: (["ls", "n", "hs"], [150.0, 0.0, 6000.0, 1.2, 5.5, 12000.0, 2.0, 1.5]),
    5: (["ls", "ls2"], [150.0, -13.0, 0, 0, 0, 0, 0, 2.55, 400.0, -13.0]),
    6: (["ls", "ls2", "hs"], [150.0, -13.0, 0, 0, 0, 5000.0, -3.0, 2.55, 400.0, -13.0]),
}


def apply_model(blocks, p, f):
    h = np.ones_like(f, dtype=complex)
    if "ls" in blocks:
        h = h * lowshelf(f, p[0], p[1])
    if "ls2" in blocks:
        h = h * lowshelf(f, p[8], p[9])
    if "n" in blocks:
        h = h * notch(f, p[2], p[3], p[4])
    if "hs" in blocks:
        h = h * highshelf(f, p[5], p[6])
    return db(h) + p[7]


def free_idx(blocks):
    idx = [0, 1, 7]
    if "ls2" in blocks:
        idx += [8, 9]
    if "n" in blocks:
        idx += [2, 3, 4]
    if "hs" in blocks:
        idx += [5, 6]
    return sorted(idx)


GAINLIKE = (1, 4, 6, 7, 9)
FREQ = (0, 2, 5, 8)


def shape_only(blocks, p, f):
    """Model response without the overall-gain param (gain solved analytically)."""
    q = p.copy()
    q[7] = 0.0
    return apply_model(blocks, q, f)


def fit_tone(blocks, p0, f, target):
    p = np.array(p0, float)
    mult = [1.25, 1.1, 1.04, 1.015]
    adds = [3.0, 1.5, 0.7, 0.3]  # dB steps for gain-ish params
    p[7] = float(np.mean(target - shape_only(blocks, p, f)))
    best = np.max(np.abs(apply_model(blocks, p, f) - target))
    for _ in range(80):
        improved = False
        for i in free_idx(blocks):
            if i == 7:
                continue  # gain always solved, never stepped
            for s in (+1, -1):
                q = p.copy()
                if i in GAINLIKE:
                    q[i] += s * adds[0]
                else:
                    q[i] *= mult[0] ** s
                    if i == 3:
                        q[i] = min(max(q[i], 0.15), 12.0)
                if i in (1, 9):
                    q[i] = min(q[i], 0.0)  # passive: DC gain <= 0 dB
                if i == 4:
                    q[i] = min(max(q[i], 0.5), 30.0)
                if i in FREQ:
                    q[i] = min(max(q[i], 10.0), 20000.0)
                q[7] = float(np.mean(target - shape_only(blocks, q, f)))
                e = np.max(np.abs(apply_model(blocks, q, f) - target))
                if e < best - 1e-9:
                    best, p, improved = e, q, True
        if not improved:
            if adds[0] < 0.05:
                break
            adds = [a / 2.5 for a in adds]
            mult = [1 + (m - 1) / 2.5 for m in mult]
    # polish pass on all params jointly (fine grid)
    return p, best


def main():
    ff, curves = load_csv()
    dsp = load_dsp()
    fgrid = np.logspace(np.log10(40), np.log10(15000), 200)
    thr, dth, ith = [], [], []
    have_ir = os.path.isdir(IR_DIR) and any(
        os.path.exists(os.path.join(IR_DIR, "AVALON_TONE%d.wav" % n)) for n in range(7)
    )
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        do_plot = True
    except ImportError:
        do_plot = False
    print("tone blocks theory-vs-CSV(max/RMS) dsp-vs-CSV(max/RMS) ir-vs-CSV(max/RMS)")
    for n in range(1, 7):
        tgt = np.interp(fgrid, ff, curves[n])
        blocks, p0 = MODELS[n]
        best_p, best_e = None, 1e9
        for jitter in (1.0, 0.6, 1.6):  # multi-start vs local minima
            q0 = np.array(p0, float)
            q0[[0, 2, 5]] *= jitter
            p, e = fit_tone(blocks, q0, fgrid, tgt)
            if e < best_e:
                best_p, best_e = p, e
        p = best_p
        theo = apply_model(blocks, p, fgrid)
        e = theo - tgt
        tmax, trms = float(np.max(np.abs(e))), float(np.sqrt(np.mean(e ** 2)))
        secs = dsp[str(n)]["sos_b0_b1_b2_a0_a1_a2"]
        dd = db(dsp_response(np.array(secs), fgrid)) # cspell:disable-line
        dd += np.mean(tgt - dd)  # overall-gain align (bank has no gain stage)
        de = dd - tgt
        dmax, drms = float(np.max(np.abs(de))), float(np.sqrt(np.mean(de ** 2)))
        if have_ir:
            try:
                ir = ir_shape(n, fgrid)
                csvn = tgt - np.interp(1000.0, fgrid, tgt)
                ie = ir - csvn
                imax = float(np.nanmax(np.abs(ie)))
                irms = float(np.sqrt(np.nanmean(ie ** 2)))
                iw = "worst@%.0fHz" % fgrid[np.nanargmax(np.abs(ie))]
            except Exception as ex:
                print("tone %d IR leg failed: %s" % (n, ex))
                imax, irms, iw = float("nan"), float("nan"), ""
        else:
            imax, irms, iw = float("nan"), float("nan"), ""
        thr.append((n, "+".join(blocks), tmax, trms))
        dth.append((n, dmax, drms))
        ith.append((n, imax, irms))
        vt = "AGREE" if tmax <= 1.0 else "DIVERGE"
        desc = (["ls_fz=%.0fHz,ls_g0=%.1fdB" % (p[0], p[1])]
                + (["ls2_fz=%.0fHz,ls2_g0=%.1fdB" % (p[8], p[9])] if "ls2" in blocks else [])
                + (["n_f0=%.0fHz,Q=%.2f,depth=%.1fdB" % (p[2], p[3], p[4])] if "n" in blocks else [])
                + (["hs_fp=%.0fHz,hs_g=%.1fdB" % (p[5], p[6])] if "hs" in blocks else [])
                + ["gain=%.2fdB" % p[7]])
        print("T%d %-9s %.3f/%.3f %s | dsp %.3f/%.3f | ir %.3f/%.3f(%s) | %s %s" % (
            n, "+".join(blocks), tmax, trms, desc,
            dmax, drms, imax, irms, iw, vt,
            ("worst@%.0fHz" % fgrid[np.argmax(np.abs(e))] if vt == "DIVERGE" else "")))
        if do_plot:
            fig, ax = plt.subplots(figsize=(7, 4))
            ax.semilogx(ff, curves[n], ".", ms=2, label="CSV oracle")
            ax.semilogx(fgrid, theo, label="passive theory")
            ax.semilogx(fgrid, dd, label="DSP biquads 48k")
            if have_ir and np.isfinite(imax):
                ax.semilogx(fgrid, ir + np.interp(1000.0, fgrid, tgt), label="IR ratio re 1kHz")
            ax.set_title("Tone %d theory vs oracles (max %.2fdB)" % (n, tmax))
            ax.set_xlabel("Hz")
            ax.set_ylabel("dB")
            ax.grid(True, which="both", alpha=0.3)
            ax.legend(fontsize=8)
            fig.tight_layout()
            fig.savefig(os.path.join(HERE, "overlay_tone%d.png" % n), dpi=80)
            plt.close(fig)
    with open(os.path.join(HERE, "verdict_table.csv"), "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["tone", "blocks", "theory_max", "theory_rms",
                    "dsp_max", "dsp_rms", "ir_max", "ir_rms", "verdict"])
        for (n, b, tm, tr), (_, dm, dr), (_, im, ir_) in zip(thr, dth, ith):
            w.writerow([n, b, round(tm, 3), round(tr, 3), round(dm, 3),
                        round(dr, 3), round(im, 3), round(ir_, 3),
                        "AGREE" if tm <= 1.0 else "DIVERGE"])
    # sensitivity: +10% each fitted param -> max dB move (tone 2 = hardest)
    print("sensitivity (tone,param,+10%/+2dB->maxdB):")
    for n in (2, 5, 4):
        blocks, p0 = MODELS[n]
        tgt = np.interp(fgrid, ff, curves[n])
        p, _ = fit_tone(blocks, p0, fgrid, tgt)
        base = apply_model(blocks, p, fgrid)
        sens = []
        for i in free_idx(blocks):
            q = p.copy()
            q[i] = q[i] + 2.0 if i in GAINLIKE else q[i] * 1.1
            sens.append(("p%d" % i, float(np.max(np.abs(apply_model(blocks, q, fgrid) - base)))))
        sens.sort(key=lambda t: -t[1])
        print("T%d" % n, ["%s:%.2f" % s for s in sens])


if __name__ == "__main__":
    main()
