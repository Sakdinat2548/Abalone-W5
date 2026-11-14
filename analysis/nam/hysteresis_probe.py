"""Hysteresis probe: same instantaneous level, rising vs falling history.

440 Hz carrier with symmetric triangle amplitude envelope (1 s up, 1 s down,
peak -6 dBFS) through DI capture (Gateway Slim:1, neutral params). Coherent
H1-H3 in matched-amplitude windows on each slope. Identical harmonics both
ways -> no memory -> static suffices. Split -> real dynamics to model.
"""
import os
import subprocess

import numpy as np
from scipy.io import wavfile

HOME = os.path.expanduser("~")  # machine-local tools/presets; no literal usernames
GW_PRESETS = os.path.join(HOME, "Documents", "VST3 Presets",
                          "Atkinson Advanced Modeling, LLC", "Gateway")
PLUG = os.path.join(HOME, "Code", "Plugalyzer", "build", "Plugalyzer_artefacts", "Release", "Plugalyzer.exe")
GW = r"C:\Program Files\Common Files\VST3\Gateway.vst3"
PRESET = os.path.join(GW_PRESETS, "U5-B2-DI.vstpreset")
TMP = os.path.join(HOME, "AppData", "Local", "Temp", "opencode")
FS, F0 = 48000, 440.0
PARAMS = ["NoiseGateActive:off", "ToneStack:off", "IRToggle:off",
          "OutputMode:Raw", "CalibrateInput:off", "Slim:1.00"]


def coh(y, f, h, n0, n1):
    t = np.arange(n0, n1) / FS
    a = 2 * np.pi * f * h * t
    seg = y[n0:n1]
    return 2 * np.hypot(np.sum(seg * np.cos(a)), np.sum(seg * np.sin(a))) / len(seg)


def main():
    n = 2 * FS
    tri = np.concatenate([np.linspace(0, 1, FS, endpoint=False),
                          np.linspace(1, 0, FS, endpoint=False)])
    x = (0.5 * tri * np.sin(2 * np.pi * F0 * np.arange(n) / FS)).astype(np.float32)
    inp = os.path.join(TMP, "hy_in.wav")
    wavfile.write(inp, FS, x)
    outp = os.path.join(TMP, "hy_out.wav")
    cmd = [PLUG, "process", "--plugin", GW, "--input", inp, "--preset", PRESET,
           "--output", outp, "--overwrite", "--blockSize", "512"]
    for p in PARAMS:
        cmd += ["--param", p]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("RENDER FAIL", r.stderr[-500:])
        return
    _, y = wavfile.read(outp)
    y = np.asarray(y[:, 0] if y.ndim == 2 else y, dtype=float)
    print("level  H2rise H2fall dH2 | H3rise H3fall dH3  (dB rel H1, d=fall-rise)")
    for frac in (0.25, 0.50, 0.75):
        # rising: first half, sample index where tri==frac; falling: mirror
        i_up = int(frac * FS)
        i_dn = int(2 * FS - frac * FS)
        W = 4800
        r1 = [coh(y, F0, h, i_up - W // 2, i_up + W // 2) for h in (1, 2, 3)]
        r2 = [coh(y, F0, h, i_dn - W // 2, i_dn + W // 2) for h in (1, 2, 3)]
        print("%4.0f%% %+.1f %+.1f %+.1f | %+.1f %+.1f %+.1f" % (
            frac * 50,
            20 * np.log10(r1[1] / r1[0]), 20 * np.log10(r2[1] / r2[0]),
            20 * np.log10(r2[1] / r2[0]) - 20 * np.log10(r1[1] / r1[0]),
            20 * np.log10(r1[2] / r1[0]), 20 * np.log10(r2[2] / r2[0]),
            20 * np.log10(r2[2] / r2[0]) - 20 * np.log10(r1[2] / r1[0])), flush=True)


if __name__ == "__main__":
    main()
