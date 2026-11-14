"""FR calibration per capture (B3/B7/DI) at low drive via Gateway renders.

If input-path FR tilts drive per frequency, harmonic residuals inherit the
pattern. Measure out/in at -30 dBFS per freq (linear regime), rel 440 Hz.
"""
import csv
import os
import subprocess

import numpy as np
from scipy.io import wavfile

HOME = os.path.expanduser("~")  # machine-local tools/presets; no literal usernames
GW_PRESETS = os.path.join(HOME, "Documents", "VST3 Presets",
                          "Atkinson Advanced Modeling, LLC", "Gateway")
PLUG = os.path.join(HOME, "Code", "Plugalyzer", "build", "Plugalyzer_artefacts", "Release", "Plugalyzer.exe")
GW = r"C:\Program Files\Common Files\VST3\Gateway.vst3"
PRESETS = {
    "B3": os.path.join(GW_PRESETS, "U5-B3.vstpreset"),
    "B7": os.path.join(GW_PRESETS, "U5-B7.vstpreset"),
    "DI": os.path.join(GW_PRESETS, "U5-B2-DI.vstpreset"),
}
TMP = os.path.join(HOME, "AppData", "Local", "Temp", "opencode")
FS = 48000
FREQS = [55.0, 110.0, 220.0, 440.0, 1000.0, 2000.0, 5000.0]
PARAMS = ["NoiseGateActive:off", "ToneStack:off", "IRToggle:off",
          "OutputMode:Raw", "CalibrateInput:off", "Slim:1.00"]


def main():
    for cap, preset in PRESETS.items():
        fr = {}
        for f in FREQS:
            x = (0.03162 * np.sin(2 * np.pi * f * np.arange(2 * FS) / FS)).astype(np.float32)
            inp = os.path.join(TMP, "fr_in.wav")
            wavfile.write(inp, FS, x)
            outp = os.path.join(TMP, "fr_out.wav")
            cmd = [PLUG, "process", "--plugin", GW, "--input", inp,
                   "--preset", preset, "--output", outp, "--overwrite",
                   "--blockSize", "512"]
            for p in PARAMS:
                cmd += ["--param", p]
            r = subprocess.run(cmd, capture_output=True, text=True)
            _, y = wavfile.read(outp)
            y = np.asarray(y[:, 0] if y.ndim == 2 else y, dtype=float)[FS:2 * FS]
            fr[f] = float(np.sqrt(np.mean(y**2)) / np.sqrt(np.mean(x[FS:2 * FS]**2)))
        ref = fr[440.0]
        print(cap + " FR rel 440Hz: " + " ".join(
            "%gHz %+.1fdB" % (f, 20 * np.log10(fr[f] / ref)) for f in FREQS), flush=True)


if __name__ == "__main__":
    main()
