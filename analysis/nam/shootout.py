"""Waveform shootout: OUR plugin vs NAM capture, identical input.

Renders sine probes through Gateway/DI-preset (Slim:1) and Abalone-W5.vst3
(Boost 2, Tone Bypass, HighCut Off, Output 0, OS 1x) at two drive levels,
time-aligns via fundamental phase (integer samples), normalizes fundamentals, plots overlaid
zoom + residual + spectra. PNG to Temp for preview.
"""
import os
import subprocess

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from scipy.io import wavfile

HOME = os.path.expanduser("~")  # machine-local tools/presets; no literal usernames
GW_PRESETS = os.path.join(HOME, "Documents", "VST3 Presets",
                          "Atkinson Advanced Modeling, LLC", "Gateway")
PLUG = os.path.join(HOME, "Code", "Plugalyzer", "build", "Plugalyzer_artefacts", "Release", "Plugalyzer.exe")
GW = r"C:\Program Files\Common Files\VST3\Gateway.vst3"
OURS = r"C:\Program Files\Common Files\VST3\Abalone-W5.vst3"
DI_PRESET = os.path.join(GW_PRESETS, "U5-B2-DI.vstpreset")
TMP = os.path.join(HOME, "AppData", "Local", "Temp", "opencode")
FS = 48000
GW_PARAMS = ["NoiseGateActive:off", "ToneStack:off", "IRToggle:off",
             "OutputMode:Raw", "CalibrateInput:off", "Slim:1.00"]
OUR_PARAMS = ["Boost:2", "Tone:Bypass", "High Cut:Off", "Output:0.0",
              "OS Factor:1x", "Active:On", "Bypass:Off"]


def render(plugin, inp, outp, preset=None, params=()):
    cmd = [PLUG, "process", "--plugin", plugin, "--input", inp,
           "--output", outp, "--overwrite", "--blockSize", "512"]
    if preset:
        cmd += ["--preset", preset]
    for p in params:
        cmd += ["--param", p]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr[-300:]
    _, y = wavfile.read(outp)
    return np.asarray(y[:, 0] if y.ndim == 2 else y, dtype=float)


def fund_phase(y, f, n0, n1):
    t = np.arange(n0, n1) / FS
    seg = y[n0:n1]
    a = 2 * np.pi * f * t
    return np.arctan2(np.sum(seg * np.sin(a)), np.sum(seg * np.cos(a)))


def align(a, b, freq, sr=FS, maxlag=8000):
    # Shift b onto a by the FUNDAMENTAL phase difference (harmonics must not
    # vote). Nearest integer sample: <=0.5 sample error ~= -35 dB at 440 Hz,
    # negligible next to real harmonic content. Sign: delaying b by s samples
    # retards its phase by 2*pi*f*s/fs, so s = +dphi/(2*pi*f)*fs.
    n = min(len(a), len(b)) - maxlag
    n0 = maxlag  # settled region for phase read
    dphi = fund_phase(b, freq, n0, n0 + n) - fund_phase(a, freq, n0, n0 + n)
    dphi = (dphi + np.pi) % (2 * np.pi) - np.pi
    N = int(round(dphi / (2 * np.pi * freq) * sr))
    N = max(-maxlag, min(maxlag, N))
    if N >= 0:
        bs, aa = b[N:N + n], a[:n]
    else:
        bs, aa = b[:n], a[-N:-N + n]
    return aa, bs, float(N)


def one_case(freq, level_db, tag):
    amp = 10 ** (level_db / 20.0)
    x = (amp * np.sin(2 * np.pi * freq * np.arange(2 * FS) / FS)).astype(np.float32)
    inp = os.path.join(TMP, "sh_in.wav")
    wavfile.write(inp, FS, x)
    g = render(GW, inp, os.path.join(TMP, "sh_gw.wav"), DI_PRESET, GW_PARAMS)
    o = render(OURS, inp, os.path.join(TMP, "sh_our.wav"), None, OUR_PARAMS)
    g, o, lag = align(g, o, freq)
    print("freq %g level %+.0f alignment lag %+.0f samples" % (freq, level_db, lag), flush=True)
    seg = slice(FS, FS + int(FS * 20 / freq))  # ~20 cycles
    gs, os_ = g[seg], o[seg]
    # normalize fundamentals
    t = np.arange(seg.stop - seg.start) / FS
    fg = 2 * np.hypot(np.sum(gs * np.cos(2 * np.pi * freq * t)),
                      np.sum(gs * np.sin(2 * np.pi * freq * t))) / len(gs)
    fo = 2 * np.hypot(np.sum(os_ * np.cos(2 * np.pi * freq * t)),
                      np.sum(os_ * np.sin(2 * np.pi * freq * t))) / len(os_)
    gs, os_ = gs / fg, os_ / fo
    resid = gs - os_
    rms_db = 20 * np.log10(np.sqrt(np.mean(resid**2)) + 1e-12)
    return t, gs, os_, resid, rms_db, fg, fo


def main():
    cases = [(440.0, -10.0), (55.0, -10.0), (8000.0, -10.0)]
    fig, ax = plt.subplots(len(cases), 2, figsize=(13, 4 * len(cases)))
    for i, (freq, lv) in enumerate(cases):
        t, gs, os_, resid, rms_db, fg, fo = one_case(freq, lv, i)
        n = int(len(t) * 5 / 20)
        ax[i][0].plot(t[:n] * 1000, gs[:n], label="NAM DI", lw=1)
        ax[i][0].plot(t[:n] * 1000, os_[:n], label="Ours (norm.)", lw=1, alpha=0.8)
        ax[i][0].set_title("%gHz in %+.0fdB  resid %.1fdB  gain %.2f/%.2f" % (
            freq, lv, rms_db, fg, fo))
        ax[i][0].set_xlabel("ms")
        ax[i][0].legend(fontsize=8)
        ax[i][1].plot(t[:n] * 1000, (gs - os_)[:n], lw=1)
        ax[i][1].set_title("difference (NAM - ours)")
        ax[i][1].set_xlabel("ms")
    fig.tight_layout()
    out = os.path.join(TMP, "shootout.png")
    fig.savefig(out, dpi=100)
    print("wrote", out)


if __name__ == "__main__":
    main()
