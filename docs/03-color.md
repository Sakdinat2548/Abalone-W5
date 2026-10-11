# 03 — Color: saturation, harmonics, drive

The only stage allowed to misbehave. Everything you hear as "drive"
happens in these few lines of `src/dsp/ColorStage.h`:

```
y = tanh(kx)/tanh(k) + a·x² + gate(env)·(odd bump + even bump)
k = 0.06, a = 1.5e-3
odd  = c_o·(x/w_o)³·e^{-(x/w_o)²},  c_o = 3.475e-2, w_o = 0.56
even = c_e·(x/w_e)²·e^{-(x/w_e)²},  c_e = -5e-2,     w_e = 0.82
gate = (env/(env+10.526))^0.60, env = peak-hold + 0.1 ms attack + 100 ms release
```

Three terms, three jobs. Tanh flattens peaks — that flattening *is* the
compression, no threshold knob, the curve is the knee. The x² term exists
for one reason: tanh is an odd function and odd functions can only make
odd harmonics, so without it there would be no H2 at all. The gated
bumps paint H3–H6 where the captures say they live. Small signals see
basically the tanh normalization (≈1.0012); the branch is flat at rest,
so quiet stays quiet.

## Why clipping and coloring are the same thing

Take the top off a sine wave and Fourier makes you pay in harmonics —
flattening in time *is* generation in frequency. Same motion, two views.
Pushed to +30 dB in (measured once, kept as reference): crest −5.2 dB,
fundamental −4.7 dB, H3 −16 dB, H5 −29 dB. Note what *doesn't* hold:
total energy. A static curve can delete power, and that deletion is
exactly what compression is.

## The ladder (440 Hz, dB rel H1, render-validated targets)

| Stage | H2 | H3 | H4 | H5 | THD |
|---|---|---|---|---|---|
| −10 | −62 | −70 | −80 | −85 | 0.10% |
| 0 | −61 | −60 | −66 | −69 | 0.20% |
| +5 | −55 | −51 | −60 | −60 | 0.33% |
| +10 | −50 | −56 | −64 | −66 | 0.47% |

H2 leads coming up, H3 leads mid-drive, H2 retakes it hot — the back and
forth is single-ended Class-A behaving like itself. H4–H8 hang 6–12 dB
under whoever leads: definition, never harshness. The H9+ tail sits ~25
dB down. Hard ceiling: 0.5% THD/IMD.

## Why there's a gate at all

Because everything dumber failed first. Five static designs — parallel
tanh terms, bias-through, signed pairs, windowed and resonant bumps —
all died the same death: any branch strong enough to paint H4 at one
drive level pollutes H2 at another. (There's a real theorem behind it —
Abel-integral coupling of harmonics to the fundamental — but the five
corpses prove it well enough.) One saturating gate scaling every order
together breaks the coupling: shut when quiet, open mid-drive, easing
shut when hot. The gate needs one envelope state, which is precisely the
thing static curves can't have. Fit notes live in chat history and the
(abandoned-documents) plan under `.superpowers/`.

## Pick compression by boost step (docs/dynamics.png)

No threshold knob — Boost picks where you sit on the fixed curve
(measured with a 100 Hz probe, unity staging):

| Input peak | B1 | B3 | B6 | B10 |
|---|---|---|---|---|
| −12 dBFS | — | — | — | −0.5 dB shaved |
| −6 dBFS | — | — | −0.1 dB | −1.9 dB |
| 0 dBFS | — | — | −0.5 dB | −5.2 dB |

Boost 1–3 rides the diagonal (pure level); 6 peels off on peaks; 10 dives
early and limits with flavor.

## Oversampling without fear

2x/4x on this stage only, latency exact (0/40/60 samples, host-told).
Worst folded content measured −67 dB (an ultrasonic H5 fold), so 1x runs
clean at zero latency and the knob is there for purists. The old
"no-oversampling-needed" comment died the day the tail arrived — aliasing
gets measured per design now (1 kHz to −80 dB, 3/5/8 kHz to −60 dB).

## Where it's pinned down

`tests/ColorStageTest.cpp` (ladder, ceilings, level map, CCIF, hot/DC/
gain bounds, bypass, alias). `analysis/LEVELS.md` (THD-vs-boost per
version).
