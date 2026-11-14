"""Hysteresis controls: reversed triangle through capture + both triangles
through OUR stage mirror (exact envelope loop).

Predictions: our gate (0.1 ms atk / 100 ms rel) MUST show dH>0 on a normal
triangle (release hangs open on fall) — validates the protocol detects
hysteresis when present. Reversed triangle on capture: if dH flips with
slope order -> measurement artifact; if fall stays cleaner -> slope physics.
"""
import os
import subprocess
import sys

import numpy as np
from scipy.io import wavfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fit_v3 import mirror as exact_mirror

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
P = (0.06, 0.0015, 0.03475, 0.56, -0.05, 0.82, 10.526, 0.60)


def coh(y, f, h, n0, n1):
    t = np.arange(n0, n1) / FS
    a = 2 * np.pi * f * h * t
    seg = y[n0:n1]
    return 2 * np.hypot(np.sum(seg * np.cos(a)), np.sum(seg * np.sin(a))) / len(seg)


def ladder_pair(y, f0, lab, pairs):
    W = 4800
    print("%s level H2a H2b dH2 | H3a H3b dH3  (d=b-a)" % lab)
    for tag, i_a, i_b, frac in pairs:
        r1 = [coh(y, f0, h, i_a - W // 2, i_a + W // 2) for h in (1, 2, 3)]
        r2 = [coh(y, f0, h, i_b - W // 2, i_b + W // 2) for h in (1, 2, 3)]
        print("%s %4.0f%% %+.1f %+.1f %+.1f | %+.1f %+.1f %+.1f" % (
            tag, frac * 50, 20 * np.log10(r1[1] / r1[0]), 20 * np.log10(r2[1] / r2[0]),
            20 * np.log10(r2[1] / r2[0]) - 20 * np.log10(r1[1] / r1[0]),
            20 * np.log10(r1[2] / r1[0]), 20 * np.log10(r2[2] / r2[0]),
            20 * np.log10(r2[2] / r2[0]) - 20 * np.log10(r1[2] / r1[0])), flush=True)


def render(x, tag):
    inp = os.path.join(TMP, "hy2_in.wav")
    wavfile.write(inp, FS, x.astype(np.float32))
    outp = os.path.join(TMP, "hy2_out.wav")
    cmd = [PLUG, "process", "--plugin", GW, "--input", inp, "--preset", PRESET,
           "--output", outp, "--overwrite", "--blockSize", "512"]
    for p in PARAMS:
        cmd += ["--param", p]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr[-300:]
    _, y = wavfile.read(outp)
    return np.asarray(y[:, 0] if y.ndim == 2 else y, dtype=float)


def main():
    n = 2 * FS
    car = np.sin(2 * np.pi * F0 * np.arange(n) / FS)
    tri = np.concatenate([np.linspace(0, 1, FS, endpoint=False),
                          np.linspace(1, 0, FS, endpoint=False)])
    sym = [("rise", int(f * FS), int(2 * FS - f * FS), f) for f in (0.25, 0.50, 0.75)]
    # control 1: OUR stage on symmetric triangle (exact envelope loop),
    # normalized by the known drive envelope (kills AM-correlation artifact).
    tri_safe = np.maximum(tri, 1e-3)
    ours = exact_mirror(0.5 * tri * car, *P, True)
    ladder_pair(ours / tri_safe, F0, "OURS sym-tri", [("r/f", a, b, f) for _, a, b, f in sym])
    # control 2: ASYMMETRIC triangle through capture (fast rise 0.5 s to
    # peak, slow fall 1.5 s). Same instantaneous levels, different histories
    # AND different in-window slopes. If dH tracks slope order -> artifact;
    # if fall stays cleaner under both geometries -> slope physics.
    up = np.linspace(0, 1, FS // 2, endpoint=False)
    dn = np.linspace(1, 0, 3 * FS // 2, endpoint=False)
    tri2 = np.concatenate([up, dn])
    pairs = []
    for f in (0.25, 0.50, 0.75):
        i_a = int(f * FS // 2)
        i_b = int(FS // 2 + (1 - f) * 3 * FS // 2)
        pairs.append(("fastrise/slowfall", i_a, i_b, f))
    y = render((0.5 * tri2 * car).astype(np.float32), "asym")
    lat = len(y) - len(tri2)  # uncompensated plugin latency; trim, steady-state unaffected
    print("render latency trim: %d samples" % lat, flush=True)
    y = y[lat:lat + len(tri2)] if lat > 0 else y[:len(tri2)]
    ladder_pair(y / np.maximum(tri2, 1e-3), F0, "CAPT asym-tri", pairs)


if __name__ == "__main__":
    main()
