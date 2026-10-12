# 02 — Tones: measured, then fitted

Six shapes plus bypass. Rule zero of this chapter: every number was
measured first and fitted second. Nothing was tuned by ear.

## What a tone actually is

A cascade of up to 6 biquads (cookbook RBJ, Transposed Direct Form II)
plus one fixed staging gain. Bypass is the identity — zero sections,
bit-exact. Coefficients recompute for whatever sample rate shows up, so
there are no rate-specific tables to drift apart.

## The pecking order

Three sources, strict priority. When they disagree, higher wins:

1. **Sweep IRs** — licensed hardware captures living gitignored in
   `analysis/ir_local/`. Take `FFT(TONEn)/FFT(TONE0)` in dB, normalize at
   1 kHz; dividing by TONE0 cancels whatever gain the capture rig added.
   This is the binding oracle since the v1.3 refit.
2. **The manual chart** (digitized to CSV) — published curves. Trusted at
   the extremes (10 Hz, 20 kHz), where sweeps get noisy; advisory
   everywhere else.
3. **NAM captures** — cross-checks only, never the deciding vote.

Fit between 30 Hz and 18 kHz. Outside that, match only what all three
agree on.

## How close (compiled-C++ probe vs sweep)

| T1 | T2 | T3 | T4 | T5 | T6 |
|---|---|---|---|---|---|
| 0.01 | 0.31 | 0.08 | 0.05 | 0.10 | 0.02 |

(The `ir_check.py` harness gates a different band, 40 Hz–15 kHz — same
conclusion, different ruler.)

T2 and T3 keep their v1 numbers by ruling. T5 was already near-perfect,
so nobody touched it. T2 is the weakest link — its notch placement is
argued by three sources (below). Rate agreement isn't perfectly flat
either: T1/T3 0.11, T6 0.15, T4 0.3 dB, all recorded as-is.

## What they sound like (dB rel 1 kHz)

- **T1, the slap smile:** +7 lows, flat mids, air climbing past +8 up top.
  Big bottom, glassy highs, stays out of the guitars' way.
- **T2, the aggressive scoop:** +13 lows, a −8 hole around 700 Hz, searing
  top. The modern-metal DI. Also our worst fit — season accordingly.
- **T3, the honest one:** +4 lows, near-flat everywhere else. The default,
  because it sounds like your bass, slightly fatter.
- **T4–T6:** warm, neutral-bright, airy. T5 ~0.1, the others inside
  0.05–0.1 with their deviations on record.

## Why each tone has a gain number

Shapes change loudness as a side effect, which makes A/B'ing them
miserable — so each carries a fixed gain picked so pink noise measures
the same in and out (±0.01, throwaway-verified, not gated). Pink is one
guess at "typical source," and it shows: mid-heavy material still swings
(T2 −4.4 vs T6 +2.6 — measured, saved in
`analysis/nam/tone_gain_spread.csv`). No fixed number matches every
source; that's what Trim is for.

## The T2 notch saga — read before re-litigating

Ours sits at 678 Hz. The sweep says ~662, a NAM probe read lower still,
the manual reads ~715. Four witnesses, four numbers, ~0.3 RMSE covering
all of them. Move the notch and you please one oracle while betraying
two — this one stays. (Absolute anchors live at pink levels, T1 −2.52
through T6 −21.15 in `ToneBankTest`; the chart rules zones, not points.)
