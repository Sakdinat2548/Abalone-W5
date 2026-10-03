# Levels calibration + THD-vs-boost map (Task 29)

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
- At +30 dB boost with a −20 dBFS bass, the stage sees +34 dBu and contributes ≈0.12% THD (the spec anchor).
- At +30 dB boost with a −10 dBFS bass, the stage sees +44 dBu and contributes ≈0.8% THD (audible grind).
- At +30 dB boost with a 0 dBFS input, the stage sees +54 dBu and contributes ≈6.3% THD (documented, not assert-pinned — 30 dB past hardware max, not a sane level).

## Pinned calibration points (asserts in `tests/ColorStageTest.cpp`)

- Quiet: boost 1 × −20 dBFS → `thdAt(1000, −17)` ≈ 0.0042%, pinned to [0.002%, 0.007%].
- Nominal: boost 5 × −10 dBFS → `thdAt(1000, +5)` ≈ 0.058%, pinned to [0.040%, 0.080%].
- Hot: boost 10 × −20 dBFS → `thdAt(1000, +10)` ≈ 0.121%, pinned to [0.090%, 0.160%].
