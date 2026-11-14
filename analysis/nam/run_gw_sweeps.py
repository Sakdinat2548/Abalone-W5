"""Headless NAM sweeps via Plugalyzer + Gateway (Slim:1.00 = full submodel).

Grid: freqs 55/110/220/440/1k/2k/5k x in-peak -25..-4 dBFS (3 dB steps) x
captures {T2 (falsifier: bass-vs-mid knee break), DI (control: expect flat)}.
Per combo: synth sine WAV -> Gateway render (neutral params) -> coherent
H1-H8 ladder -> CSV. Compares against numpy remap rows where overlapping.
"""
import csv
import os
import subprocess
import sys

import numpy as np
from scipy.io import wavfile

HOME = os.path.expanduser("~")  # machine-local tools/presets; no literal usernames
GW_PRESETS = os.path.join(HOME, "Documents", "VST3 Presets",
                          "Atkinson Advanced Modeling, LLC", "Gateway")
PLUG = os.path.join(HOME, "Code", "Plugalyzer", "build", "Plugalyzer_artefacts", "Release", "Plugalyzer.exe")
GW = r"C:\Program Files\Common Files\VST3\Gateway.vst3"
PRESETS = {
    "T2": os.path.join(GW_PRESETS, "U5-T2-B4.vstpreset"),
    "DI": os.path.join(GW_PRESETS, "U5-B2-DI.vstpreset"),
    "B3": os.path.join(GW_PRESETS, "U5-B3.vstpreset"),
    "B7": os.path.join(GW_PRESETS, "U5-B7.vstpreset"),
}
TMP = os.path.join(HOME, "AppData", "Local", "Temp", "opencode")
FS = 48000
FREQS = [55.0, 110.0, 220.0, 440.0, 1000.0, 2000.0, 5000.0]
LEVELS = [-25.0, -22.0, -19.0, -16.0, -13.0, -10.0, -7.0, -4.0]
PARAMS = ["NoiseGateActive:off", "ToneStack:off", "IRToggle:off",
          "OutputMode:Raw", "CalibrateInput:off", "Slim:1.00"]


def ladder(y, f, top=8):
    y = np.asarray(y, dtype=float)
    y -= np.mean(y)
    t = np.arange(len(y)) / FS
    out, h1 = [], None
    for h in range(1, top + 1):
        a = 2 * np.pi * f * h * t
        m = 2 * np.hypot(np.sum(y * np.cos(a)), np.sum(y * np.sin(a))) / len(y)
        if h == 1:
            h1 = m
            out.append(20 * np.log10(max(h1, 1e-12)))
        else:
            out.append(20 * np.log10(max(m / h1, 1e-12)))
    return out


def main():
    out_csv = os.path.join(TMP, "gw_sweeps_all.csv")
    with open(out_csv, "w", newline="") as fh:
        wr = csv.writer(fh)
        wr.writerow(["cap", "freq", "level_dbfs"] + ["H%d" % h for h in range(1, 9)])
        for cap, preset in PRESETS.items():
            for f in FREQS:
                for lv in LEVELS:
                    amp = 10 ** (lv / 20.0)
                    x = (amp * np.sin(2 * np.pi * f * np.arange(2 * FS) / FS)).astype(np.float32)
                    inp = os.path.join(TMP, "sw_in.wav")
                    wavfile.write(inp, FS, x)
                    outp = os.path.join(TMP, "sw_out.wav")
                    cmd = [PLUG, "process", "--plugin", GW, "--input", inp,
                           "--preset", preset, "--output", outp, "--overwrite",
                           "--blockSize", "512"]
                    for p in PARAMS:
                        cmd += ["--param", p]
                    r = subprocess.run(cmd, capture_output=True, text=True)
                    if r.returncode != 0 or not os.path.exists(outp):
                        print("RENDER FAIL", cap, f, lv, r.stderr[-300:], flush=True)
                        continue
                    _, y = wavfile.read(outp)
                    y = np.asarray(y[:, 0] if y.ndim == 2 else y, dtype=float)
                    # skip first second (model settled + latency), ladder second
                    lad = ladder(y[FS:2 * FS], f)
                    wr.writerow([cap, "%.0f" % f, "%.0f" % lv] +
                                ["%.2f" % v for v in lad])
                    fh.flush()
                    print("%s f=%4.0f in=%+.0f H2 %+.1f H3 %+.1f" % (
                        cap, f, lv, lad[1], lad[2]), flush=True)
    print("wrote", out_csv)


if __name__ == "__main__":
    main()
