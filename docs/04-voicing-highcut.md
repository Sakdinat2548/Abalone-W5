# 04 — Voicing + HighCut: fingerprint and filter

## Voicing: the sound of switched-on

Three fixed biquads, on whenever ACTIVE is engaged, gone (bit-flat) on
bypass:

| Section | Detail |
|---|---|
| Low shelf 75 Hz | +0.91 dB, Q 0.54 |
| High shelf 5441 Hz | −0.34 dB, Q 0.62 |
| Peak 180 Hz | +0.11 dB, Q 1.97 |

No real box is a perfect wire — buffers and gain stages leave a
fingerprint even at neutral settings: bass lifted a touch, top eased
off, a whisper of low-mid body. That's this trio, all under ±1 dB.
Seasoning, not EQ.

It was measured, not tuned: the trio fits the engaged-vs-bypassed gap
across captures that disagree with each other, landing within
hundredths of a dB. Deliberately not exact — exact to *what*, when the
witnesses differ? (The dip at 10 Hz in plots is our own DC-blockers, see
below — not the voicing.)

## HighCut: the noise filter

One-pole lowpass at 4 kHz, fitted to a sweep at 0.13 dB — it shipped
8 kHz through v1.2 until measurement plus an ear-check overturned it.
(The 2002 manual says 1.8 kHz. Same neighborhood; measurement spread.)
One line of code: `state += a·(x − state)`, coefficient matched exactly.
A single pole is 6 dB/oct only far above the corner; here it's gentler
(−3.7 dB 4k→8k, ~−4.0 to 16k), flattening toward Nyquist. In practice:
same sound, less noise. Hiss, string squeak, and saturation hash live up
there; bass tone doesn't. Off unless you want it.

It sits after Color for the oldest reason in guitar gear: distortion
sprays harmonics upward, the filter shaves them back. Dimed settings
snarl instead of fizzing because of this ordering.

## The two DC-blockers (not voicing, don't mix them up)

- **Input, 5 Hz:** kills incoming DC, rumble, and plosive thumps *before*
  Boost ever sees them, so junk can't modulate the drive. Costs ~1 dB at
  10 Hz, on purpose.
- **Post-color, 2 Hz:** mops up what the x² term manufactures (7.5 mV at
  +10 dB, ~190 mV dimed at +24 dB). No pops, no stolen headroom.

## Where it's pinned

Voicing oracle + engaged-flat gates (`ChainTest`); HighCut corner,
stopband, and passband (`HighCutTest`); blocker corners owned by design.
