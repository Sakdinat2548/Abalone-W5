# 01 — Signal chain

Eight blocks, fixed order, no routing to get lost in:

```
DC-block -> Boost -> Tone -> Color -> DC-block -> HighCut -> Trim -> Voicing + LED
```

(`docs/signal-flow.png`. Flip bypass or ACTIVE off and the whole thing
becomes a straight wire — bit-transparent, provably so.)

## DC-block + Boost (src/dsp/GainStage.h)

One stage does two jobs, in this order: a 5 Hz highpass (`hp = x − lp`)
throws away incoming DC, rumble, and plosive thumps first, then ten
switched gain steps (+3 dB each, +3…+30 dB) multiply clean float. No
saturation anywhere in here, no clipping, ever:

```
y = hp(x) * 10^(3N/20),  N = 1..10
```

Think of Boost as choosing how hard the Color stage gets punched — it
doesn't distort, it *aims*. Knob twists ramp over ~80 ms so nothing
clicks.

## Tone (src/dsp/ToneBank.h)

Six shapes plus a bypass that's literally nothing (0 sections, identity).
Each shape is up to 6 biquads — cookbook RBJ, Transposed Direct Form II,
recomputed for whatever sample rate shows up. How the curves were earned
is chapter 02's whole story.

Switching shapes crossfades 10 ms. Every shape carries a fixed staging
gain so hopping between them doesn't jump loudness on normal sources.

## Color (src/dsp/ColorStage.h)

The only stage allowed to misbehave:

```
y = tanh(kx)/tanh(k) + a·xe² + gate(env)·(odd bump + even bump)
k = 0.06, a = 1.5e-3, |xe| ≤ 31.6 (+ fitted bump constants, see chapter 03)
```

Tanh flattens peaks (that's the compression — no threshold knob, the
curve *is* the knee). The x² term exists for one reason: tanh is an odd
function and can only make odd harmonics, so H2 needs its own term.
The gated bumps paint H3–H6 where the captures say they live. Bypassable
for tests. Runs at 1x unless you ask for 2x/4x (latency reported
exactly: 0/40/60 samples).

## HighCut (src/dsp/HighCut.h)

A one-pole lowpass at 4 kHz — fitted, not spec'd (used to ship 8 kHz
until a sweep proved otherwise). One line of code:
`state += a·(x − state)`. It takes hiss, squeak, and saturation hash off
the top and leaves tone alone. Off unless you want it.

Sits after Color deliberately: saturation sprays harmonics upward, this
shaves them back down. Dimed settings snarl instead of fizzing because
of this ordering.

## Trim + LED

Trim is a plain knob, −32…+6 dB, default −3 — your manual leveler, and
deliberately the *only* auto-gain-comp in the building (there is none).
The SIGNAL LED taps before trim (after HighCut, before voicing) at
−2 dBFS: it watches your drive staging, not your trim choices.

## Voicing (src/dsp/ProcessorChain.h)

Three fixed biquads — LS 75 Hz +0.91, HS 5441 Hz −0.34, peak 180 Hz
+0.11 — on whenever ACTIVE is engaged. This is the sound of the box
merely being switched on: buffers and gain stages are never a perfect
wire, and this trio is their fingerprint. All under ±1 dB. Seasoning,
not EQ; chapter 04 has the measurement story.

## What holds it together

- The DSP core is plain standard C++ (`<cmath>`/`<array>`/`<atomic>`,
  zero JUCE) — the tests drive it with no host in sight.
- Only Color ever runs hot; everything linear stays at 1x. No wasted CPU.
- Latency is exact once settled (0/40/60); factor switches crossfade
  5 ms, then exact again.
- Every stage answers to its own CTest gate (chapter 06).
