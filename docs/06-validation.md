# 06 — Validation: what "green" proves

Seven suites, all of them NDEBUG-independent (Release builds delete bare
`assert()`, so every check here survives that):

| Suite | Pins |
|---|---|
| Bypass | bit-transparent; flat ±0.5 dB across the band (measured to 0.95×Nyquist — nothing honest measures past it) |
| Tones | shapes ±1 dB in ruled zones (40 Hz–15 kHz); pink anchors; rate agreement recorded, not idealized (T1/T3 0.11, T6 0.15, T4 0.3) |
| Boost | +3 dB/step, never clips |
| HighCut | −3 dB at 4 kHz, stopband, passband |
| Color | ladder H2–H8 (tolerances follow capture spread) + H9–H12 below −60; THD ceilings + level map; CCIF IMD; hot/DC/gain bounds; alias (−80 dB at 1 kHz, −60 at 5/7/11 kHz) |
| Chain | end-to-end sanity, exact latency, mono rule, peak tracker |
| IO | sync/recall/trim, bypass behavior |

Run `ctest --test-dir build --output-on-failure` and count to seven.
Format gate (`clang-format --dry-run --Werror`) runs on every commit.

## What green does NOT prove

- **Truth against hardware.** Gates pin our design to our measurements
  within stated tolerances. The captures disagree with each other (B3 vs
  DI differ 4–8 dB up top — single boxes vary, that's life).
- **The quiet end.** Our DFT floor and the captures' model floor meet
  somewhere between −10 and −20 dBFS in. Down there it's floor against
  floor, which proves nothing about tone.
- **Single measurements.** The 4 kHz corner stands on one IR file with
  two corroborations and no replication: call it a 3–5 kHz zone. Exactly
  4.0 is precision theater.
- **Real music.** Every fit is steady sine waves. IMD and transient
  probes backstop them (CCIF is gated, the rest report) — program
  material remains ear-tested, by you.

## The outside world (CI + release, every run)

pluginval at strictness 10 on VST3 across all three OSes, plus
`auval -strict` for the AU on macOS. Apple signing doesn't exist here by
decision (ad-hoc instead); auval passing is what says Logic will load
it. Downloaded zips may trip Gatekeeper once — standard right-click-open.

## The machinery

`analysis/ir_check.py` compares sweeps against the chain against the
chart, checks the Python port against the C++ (≤0.05 dB), and self-tests.
`analysis/nam/` holds the fits and the probes — the C++ ones drive the
real headers, not copies. `analysis/IR_VALIDATION.md` is the v1.1-era
writeup: history now, with current notes in LEVELS.md and the nam README.
