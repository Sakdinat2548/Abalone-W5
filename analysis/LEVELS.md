# Levels calibration + THD-vs-boost map (Task 29)

## CANDIDATE 2026-10-09: hotter color drive (k=0.06, a=1.5e-3) — NOT ADOPTED

Fit to four NAM captures' harmonic ladders (spike probe, `tone/v1.2` research).
Old map below is the shipped (k=0.03, a=6e-4) behavior and stays authoritative
until/unless the constants change. New candidate map, RMS-specified inputs
(sine peak sits 3.01 dB above RMS; stage physics identical, grid relabeled):

| Boost | Input | Stage sees | Stage drive (peak) | THD @1 kHz |
|-------|-------|-----------|---------------------|------------|
| 1 (+3 dB) | −20 dBFS RMS | +7 dBu RMS | −13.99 dB | 0.0150% |
| 1 (+3 dB) | −10 dBFS RMS | +17 dBu RMS | −3.99 dB | 0.0488% |
| 1 (+3 dB) | 0 dBFS RMS | +27 dBu RMS | +6.01 dB | 0.1918% |
| 5 (+15 dB) | −20 dBFS RMS | +19 dBu RMS | −1.99 dB | 0.0626% |
| 5 (+15 dB) | −10 dBFS RMS | +29 dBu RMS | +8.01 dB | 0.2674% |
| 5 (+15 dB) | 0 dBFS RMS | +39 dBu RMS | +18.01 dB | 1.9029% |
| 10 (+30 dB) | −20 dBFS RMS | +34 dBu RMS | +13.01 dB | 0.6810% |
| 10 (+30 dB) | −10 dBFS RMS | +44 dBu RMS | +23.01 dB | 5.2508% |
| 10 (+30 dB) | 0 dBFS RMS | +54 dBu RMS | +33.01 dB | 24.1691% |

Reads: nominal playing (−10 RMS, boost 5) 0.27%; the old operating point
(+34 dBu) reads 0.68% in RMS terms. The +44/+54 dBu RMS corners leave
"subtle" territory entirely (5.3%/24% — real grind/saturation, matching what
the captures show dimed). Adopting this supersedes the 0.1%-order THD story
below (manual figure) in favor of capture matching; H2-dominance flips to H3
above ~+5 dB stage drive (2.5× H2-led at 0 dB).

## Calibration (fixed rule)

**0 dBFS = +24 dBu** (hardware maximum input — the one anchor the manual states).

Consequences:

- +4 dBu nominal = −20 dBFS.
- EBU −18 dBFS ≈ +6 dBu here.
- Boost step N adds 3N dB on top of whatever enters (+3 dB at step 1 … +30 dB at step 10).
- Stage input level: `dBu = input_dBFS + 24 + 3N`; `thdAt` levelDb = `input_dBFS + 3N`.

## Method

1 kHz sine, tone bypassed for isolation, measured with the existing `ColorStage::thdAt`
machinery (coherent 1 s correlation @48 kHz, H2–H8 vs H1) — reused, not reinvented.
Stage-only numbers: `thdAt` drives `processSample` directly, so the chain's post-color
2 Hz DC-blocker (Task 28) strips the `a*x^2` DC term in real use but does not change
these harmonic THD figures.

## Measured 3×3 map

| Boost | Input | Stage sees | Stage drive (`thdAt` level) | THD @1 kHz |
|-------|-------|-----------|------------------------------|------------|
| 1 (+3 dB) | −20 dBFS | +7 dBu | −17 dB | 0.0042% |
| 1 (+3 dB) | −10 dBFS | +17 dBu | −7 dB | 0.0135% |
| 1 (+3 dB) | 0 dBFS | +27 dBu | +3 dB | 0.0449% |
| 5 (+15 dB) | −20 dBFS | +19 dBu | −5 dB | 0.0170% |
| 5 (+15 dB) | −10 dBFS | +29 dBu | +5 dB | 0.0584% |
| 5 (+15 dB) | 0 dBFS | +39 dBu | +15 dB | 0.2904% |
| 10 (+30 dB) | −20 dBFS | +34 dBu | +10 dB | 0.1210% |
| 10 (+30 dB) | −10 dBFS | +44 dBu | +20 dB | 0.7951% |
| 10 (+30 dB) | 0 dBFS | +54 dBu | +30 dB | 6.2594% |

Sanity: THD rises monotonically with level in every row and every column; no NaN;
only the +54 dBu corner (30 dB past hardware maximum — insane drive, not a sane
operating point) exceeds 5%, which is expected hard saturation, not a defect.

## Plain-English reads

- At +3 dB boost with a −20 dBFS bass, the stage sees +7 dBu and contributes ≈0.004% THD (inaudible).
- At +15 dB boost with a −10 dBFS bass, the stage sees +29 dBu and contributes ≈0.06% THD (subtle).
- At +30 dB boost with a −20 dBFS bass, the stage sees +34 dBu and contributes ≈0.12% THD (chosen operating point — see honesty note below).
- At +30 dB boost with a −10 dBFS bass, the stage sees +44 dBu and contributes ≈0.8% THD (audible grind).
- At +30 dB boost with a 0 dBFS input, the stage sees +54 dBu and contributes ≈6.3% THD (documented, not assert-pinned — 30 dB past hardware max, not a sane level).

## Pinned calibration points (asserts in `tests/ColorStageTest.cpp`)

- Quiet: boost 1 × −20 dBFS → `thdAt(1000, −17)` ≈ 0.0042%, pinned to [0.002%, 0.007%].
- Nominal: boost 5 × −10 dBFS → `thdAt(1000, +5)` ≈ 0.058%, pinned to [0.040%, 0.080%].
- Hot: boost 10 × −20 dBFS → `thdAt(1000, +10)` ≈ 0.121%, pinned to [0.090%, 0.160%].

## Honesty note: relationship to the manual's distortion spec

The Avalon literature states ~0.1% THD at "+10 dB" (an older manual revision
says 0.05%) — with no stated level reference. Two readings are defensible
and neither is confirmable without hardware:
(a) +10 dBu at the stage → our model reads ≈0.006% there, i.e. ~17×
cleaner than the spec figure; (b) the +34 dBu point above → ≈0.12%,
near the spec figure by construction, not by calibration.
We ship (b) as a CHOSEN OPERATING POINT, not a spec match: raising drive
17× to hit reading (a) would make every sane playing level more colored
than the unit players describe as "clean with weight" — tuning real DSP to
an ambiguous number with a factor-of-17 error bar. The map, pins, and math
above are unaffected by this naming; only the claim is corrected.

## Web-research cross-check (2026-10-05, no model change)

- 0 dBFS = +24 dBu VERIFIED (official max input, 12.28 Vrms); +4 dBu → −20
  dBFS is the self-consistent SMPTE alignment (−18 would imply +22 max,
  contradicting the spec). Calibration stands as written.
- Boost 3N dB/step CONFIRMED (manual + current spec + 10-detent/9-resistor
  hardware). The manual's "+2…+32 dB" needs 11 detents — stale rev, ignore.
- THD spec drifted between revs: 0.05% (old manual) vs 0.1% (current site),
  both at "+10 dB" with no level/load/frequency stated. Our 0.12% point
  sits inside either reading; no evidence forces a move.
- HEADROOM NOTE: max Class-A output is +30 dBu = +6 dBFS — 6 dB above
  digital clip. Hot boost + hot input (e.g. the +44/+54 dBu map corners)
  can exceed 0 dBFS inside the chain before Trim; that is modeled
  headroom behavior, not a defect. Downstream hosts/clippers own it.

## Open measurement items (need hardware — not inferable)

- Mic-vs-line output offset (pad value unpublished).
- Boost position-1 absolute gain (+3 vs the stale manual's +2).
- Headphone tap point (pre/post tone unspecified).
