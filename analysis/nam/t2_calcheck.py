"""T2 tone-calibrated recheck: measure TONE 2 capture's linear FR at low drive,
then compare capture vs stage-mirror H2/H3 with per-frequency drive shaping.

If the tilted residual collapses, chain order (Tone -> Color) already explains
it and no model change is indicated. Uses the capture as its own calibrator:
-30 dBFS sine per freq (linear regime, harmonics at floor) -> out/in gain,
normalized to 440 Hz. Then mid-band ladder comparison with shaped drive.
"""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nam_numpy import NAME
from color_remap import ladder, stage_mirror, FS, N, WARM

HERE = os.path.dirname(os.path.abspath(__file__))
IR = os.path.join(HERE, "..", "ir_local")
FREQS = [55.0, 110.0, 220.0, 440.0, 1000.0, 2000.0, 5000.0]
LEVELS = [-15.0, -10.0, -5.0]
BOOST = 12.0  # Boost4 = 3*4 dB


def main():
    m = NAME(os.path.join(IR, "Avalon U5 TONE 2 Boost4.nam"))
    # linear FR at -30 dBFS (harmonics at floor there)
    fr = {}
    for f in FREQS:
        x = 10 ** (-30.0 / 20.0) * np.sin(2 * np.pi * f * np.arange(N) / FS)
        y = m.forward(x)
        fr[f] = float(np.sqrt(np.mean(y**2)) / np.sqrt(np.mean(x**2)))
    ref = fr[440.0]
    print("T2 linear FR rel 440Hz: " + " ".join(
        "%gHz %+.1fdB" % (f, 20 * np.log10(fr[f] / ref)) for f in FREQS), flush=True)
    print("freq in_db gotH2 predH2flat predH2shaped | gotH3 predH3flat predH3shaped")
    for f in FREQS:
        for lv in LEVELS:
            x = 10 ** (lv / 20.0) * np.sin(2 * np.pi * f * np.arange(N) / FS)
            got = ladder(m.forward(x)[-N:], f)
            amp_flat = 10 ** ((lv + BOOST) / 20.0)
            amp_sh = amp_flat * fr[f] / ref
            ps, pf = {}, {}
            for tag, amp in (("sh", amp_sh), ("fl", amp_flat)):
                xs = amp * np.sin(2 * np.pi * f * np.arange(N + WARM) / FS)
                l = ladder(stage_mirror(xs)[-N:], f)
                (ps if tag == "sh" else pf)[f] = l
            print("%4.0f %+.0f %+.1f %+.1f/%+.1f | %+.1f %+.1f/%+.1f" % (
                f, lv, got[1], pf[f][1], ps[f][1], got[2], pf[f][2], ps[f][2]), flush=True)


if __name__ == "__main__":
    main()
