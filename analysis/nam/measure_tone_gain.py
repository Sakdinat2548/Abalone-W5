"""Throwaway: broadband RMS gain/loss per tone (pink + white noise, exact FFT)."""
import sys

sys.path.insert(0, r"C:\Users\<you>\Code\Abalone-W5\analysis")
import numpy as np
from ir_check import rbj_cook, TONE_PARAMS, TONE_GAIN

FS = 48000.0
N = 1 << 20
rng = np.random.default_rng(7)
white = rng.standard_normal(N)
Xw = np.fft.rfft(white)
freqs = np.fft.rfftfreq(N, 1.0 / FS)
Xp = Xw.copy()
Xp[1:] = Xw[1:] / np.sqrt(np.maximum(freqs[1:], 1e-9) / 1000.0)

print("tone | pink dB | white dB")


def resp(tone):
    z = np.exp(1j * 2 * np.pi * freqs / FS)
    h = np.ones(N // 2 + 1, dtype=complex)
    for s in range(6):
        key = tone * 10 + s
        if key not in TONE_PARAMS:
            continue
        typ, f0, q, g = TONE_PARAMS[key]
        b0, b1, b2, a1, a2 = rbj_cook(typ, f0, q, g, FS)
        h *= (b0 + b1 / z + b2 / z / z) / (1.0 + a1 / z + a2 / z / z)
    return h * 10 ** (TONE_GAIN[tone] / 20.0)


for t in range(1, 7):
    h = resp(t)
    rms_in_p = float(np.sqrt(np.mean(np.abs(Xp) ** 2)))
    rms_ou_p = float(np.sqrt(np.mean(np.abs(Xp * h) ** 2)))
    rms_in_w = float(np.sqrt(np.mean(np.abs(Xw) ** 2)))
    rms_ou_w = float(np.sqrt(np.mean(np.abs(Xw * h) ** 2)))
    print("T%d | %+.2f | %+.2f" % (t, 20 * np.log10(rms_ou_p / rms_in_p),
                                   20 * np.log10(rms_ou_w / rms_in_w)))
