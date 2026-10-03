# IR validation (Tasks 8 + 12 + 15 + 17 + 20 + Fix Rounds 1–2) — method, status, measured results

## Status history

### Task 24 (tight RBJ fit: metric fix + 0.15/0.05 gates) PARTIAL — T2/T3/T5 pass, T1/T4/T6 recorded deviations

Metric fix first (Ruling 20): max/RMS now over EVERY CSV point 10 Hz–
20 kHz inclusive (the old 40 Hz+ window under-reported T2/T5 by up to
0.6 dB — honest baseline on Task-22 numbers: T1 0.252/0.112, T2
0.671/0.200, T3 0.146/0.062, T4 0.419/0.084, T5 0.746/0.256, T6
0.427/0.119 max/RMS @48k). A spike-pin test (0.4 dB at ~12 Hz on flat
bypass) guards the coverage. Notch exception recentered on the user's
715 Hz read (CSV minimum 696.75 Hz is a digitizer artifact inside the
±3% band); T2 target cubic-spliced inside ±3% of 715 Hz.

| Tone | Max 48/44.1 (gate) | RMS 48/44.1 (gate) | Sub-bands 48 (low/mid/high) | Verdict |
|------|--------------------|--------------------|------------------------------|---------|
| 1 (6s) | 0.158/0.156 (0.17 DEVIATION) | 0.0529/0.0559 (0.06 DEVIATION) | 0.077/0.158/0.083 | DEVIATION (probes 0.102/0.11) |
| 2 (6s) | 0.136/0.133 (0.15) | 0.0486/0.0487 (0.05) | 0.072/0.179/0.098 | PASS (tip 0.18 exc) |
| 3 (5s) | 0.124/0.115 (0.15) | 0.0382/0.0397 (0.05) | 0.080/0.094/0.124 | PASS |
| 4 (6s) | 0.177/0.188 (0.19 DEVIATION) | 0.0537/0.0518 (0.06 DEVIATION) | 0.070/0.177/0.129 | DEVIATION |
| 5 (5s) | 0.080/0.080 (0.15) | 0.0227/0.0228 (0.05) | 0.031/0.080/0.002 | PASS |
| 6 (6s) | 0.216/0.165 (0.22 DEVIATION) | 0.0666/0.0665 (0.07 DEVIATION) | 0.060/0.216/0.212 | DEVIATION (probes 0.140/0.15) |

(C++ `ToneBankTest` actuals. Tips: T2 in-exception 0.179/0.180 (≤0.3).
Anchors all pass (0.18/0.61/0.18/0.24/0.09/0.17). Eye low-end green on
STRICT gates — the Task-22 T6-slope deviation is SUPERSEDED (T5-red slope
now 4.459 = gray 4.464; T6-red 4.241, delta 0.219 vs 1.0). T4 probes 0.195
stay inside the standing 0.3 gate. 96 kHz verify-only: max 0.52/0.23/0.35/
1.04/0.50/1.27 (T4/T6 top ends + T5/T3 feet render near-analog at 96k vs
warped at 48k/44.1k; inherent, reported not gated).)

Deviations (structural evidence + probe numbers in
`.superpowers/sdd/2026-10-02-abalone-u55-plan/task-24-report.md`):
- T1: 6-sec floor 0.168 (10 rounds); all five user bands fixed (worst
  +0.124); 7th-peak probe (@58.7/0.55/+1.98, NOT shipped) lands max
  0.119/0.121 rms 0.039/0.040 — but probes stay 0.103. Ruling: 7th
  section + probe relief (0.103 vs 0.1), accepted deviation, or relief.
- T4: 6-sec floor 0.189; 7th-peak probe (narrow cut @6412/2.65/-0.60,
  NOT shipped) lands max 0.146/0.155 rms 0.0427/0.0424 — one point
  (9964@44.1, +0.155) 0.005 over. Ruling: 7th section (+0.005 relief),
  accepted deviation, or relief.
- T6: 6-sec floor 0.217 (12 rounds); 7-sec probe fails too (0.214) — NOT
  a budget problem. Edge provably compatible alone (e20k = 0.0000 both
  rates) but jointly incompatible with the mid-top corner; probes 0.140.
  Ruling: edge-point relief, per-rate top tables (needs logic change),
  accepted deviation, or relief.

`analysis/ir_check.py` harness: port verification GREEN (worst diff
0.0053 dB against refs 0.16/0.12/0.07/0.10/0.05/0.21), self-test GREEN
(worst 0.0328 dB). IR-shape leg (report-only, 1 kHz-normalized spots):
worsts T1 1.73 @10k / T2 1.85 @6.3k / T3 1.09 @15k / T4 2.16 @15k /
T5 2.03 @63 / T6 3.28 @63 Hz — exceed ±1 dB exactly where chart and
hardware disagree (the fits follow the chart there by design, unchanged
ruling). Port-vs-C++ sub-15 Hz corner split ≤0.03 dB recorded (MSVC-vs-
numpy transcendental LSBs at near-DC-null transfers; C++ governs).

### Task 22 (optimizer RBJ re-fit to digitized gray) COMPLETE, 2 recorded deviations

From-scratch optimizer fit (scipy DE-global + multi-start LS + L8/minimax +
gate-slack Phase-D, joint 48+44.1 kHz, one number set for all rates) to the
digitized CSV over 40 Hz–20 kHz. Section budget T1x5/T2x6/T3x4/T4x5/T5x3/T6x4
(user table +1 for RBJ). Hierarchy UNCHANGED: CSV binding 40 Hz–20 kHz;
six 10 Hz anchors binding (all pass, every anchor within 0.73 dB);
eye-read header + IR shapes advisory; highcut-ON non-adjudicating (Ruling A);
HighCut.h untouched. 96 kHz verify-only (max 0.18/0.35/0.30/1.08/0.18/0.72 —
T4/T6 top ends render near-analog at 96k vs warped at 48k; inherent,
reported not gated).

| Tone | Max 48/44.1 (gate) | RMS 48/44.1 (gate) | e10 / e20k 48 | Anchor d (gate) |
|------|--------------------|--------------------|---------------|-----------------|
| 1 (5s) | 0.218/0.217 (0.3) | 0.101/0.102 (0.11 DEVIATION) | -0.02/-0.02 | 0.13 (1.0) PASS |
| 2 (6s) | 0.257/0.271 (0.3; tip 0.14 exc) | 0.0785/0.0786 (0.08) | -0.04/+0.16 | 0.72 (1.0) PASS |
| 3 (4s) | 0.146/0.145 (0.3) | 0.0578/0.0574 (0.08) | +0.10/-0.02 | 0.20 (1.0) PASS |
| 4 (5s) | 0.188/0.270 (0.3) | 0.0536/0.0534 (0.08) | -0.02/-0.18 | 0.15 (1.0) PASS |
| 5 (3s) | 0.126/0.126 (0.3) | 0.0733/0.0733 (0.08) | -0.11/+0.08 | 0.19 (2.0) PASS |
| 6 (4s) | 0.204/0.178 (0.3) | 0.0778/0.0739 (0.08) | +0.07/+0.04 | 0.15 (2.0) PASS |

(C++ `ToneBankTest` actuals. Fine-grid 800-pt audit gap-free: worsts within
0.02 of on-grid except T6-44.1k 0.284 vs 0.178 — still <= 0.3 everywhere.
Poles < 1 at 44.1k/48k/96k incl. float32 radii. Prior gates: bypass/T2-notch
(−20.86)/T4-dip (−4.00)/sine-agreement green; rate probes T1 0.099 / T2 0.095 /
T3 0.042 / T4 0.235 (0.3 gate, old 0.244 deviation SUPERSEDED) / T5 0.009 /
T6 0.097 (0.1); eye low-end green except T6 slope 1.270 (1.3 DEVIATION);
either-oracle advisory 0.195/0.092/0.137/0.176/0.119/0.199 — all pass.)

Recorded deviations (structural evidence + probe numbers in
`.superpowers/sdd/2026-10-02-abalone-u55-plan/task-22-report.md`):
- T1 RMS 0.101/0.11 (scoop-entry see-saw needs a 6th section; 6th-peak probe
  lands rms 0.065/max 0.144 — ruling: budget+1 vs accepted deviation).
- T6 eye slope 1.270/1.3 (LS+HS foot renders ~4.05 vs T5-red 5.284; 5th-peak
  foot-cut probe lands delta 0.931 all-green — ruling: budget+1 vs accepted
  deviation). All T6 Task-22 gates pass.
- Resolved this round: T2 10–50 curve (Fix-2 shortfall, now rendered by the
  hump peak within the 6-section budget); T4 rate warp (twin dip peaks).

`analysis/ir_check.py` harness: port verification GREEN (worst diff 0.0150 dB
against refs 0.19/0.09/0.14/0.18/0.12/0.20), self-test GREEN (worst 0.0252 dB
on the T2 notch case). IR-shape leg (report-only, 1 kHz-normalized spots):
worsts T1 1.72 @10k / T2 1.82 @10k / T3 1.09 @15k / T4 2.20 @15k /
T5 2.08 @63 / T6 3.24 @63 Hz — exceed ±1 dB exactly where chart and hardware
disagree (the blend follows the chart there by design, unchanged ruling).

### Fix Round 2 (low-end eye corrections) COMPLETE except T2-curve ruling

The user's overlay read of the manual's left third (10–200 Hz) retargets the
low end below 40 Hz, where the CSV is excluded (Ruling B) and only the six
10 Hz anchors bound. T5 is the untouched reference (numbers byte-identical —
no T2-style drift). T2 keeps HEAD numbers byte-identical too: finding 2's
curve is structurally unrenderable numbers-only (recorded shortfall below),
so there was no gate-safe move; the anchor gate (±1.0) still guards its
10 Hz point.

| Tone | Dense 40–15k (gate) | Low-end 40–200 (gate) | 10 Hz abs / delta (gate) | 20 Hz eye / slope (gate) |
|------|---------------------|----------------------|--------------------------|--------------------------|
| 1 | 0.965 @ 49 Hz (±1.0 DEVIATION) | 0.965 @ 49 Hz (±1.0 DEVIATION) | −3.81 / 0.81 (±1.0) PASS | +0.31 @ 20 Hz (±0.5) PASS; hump max +0.32 (±1.2 hump cap) |
| 2 | 0.481 @ 86 Hz (±0.5) PASS | 0.481 @ 86 Hz (±0.5) PASS | +0.70 / 0.95 (±1.0) PASS | curve SHORTFALL (see below — not gated) |
| 3 | 0.540 @ 49 Hz (±0.6 DEVIATION) | 0.540 @ 49 Hz (±0.6 DEVIATION) | −3.77 / 0.77 (±1.0) PASS | +0.27 @ 20 Hz (±0.5) PASS; monotonic 10-15-20-30 |
| 4 | 0.480 @ 5.3 kHz (±0.5) PASS | 0.282 @ 81 Hz (±0.3) PASS | −2.79 / 0.21 (±1.0) PASS | −0.01 @ 20 Hz (±0.5) PASS; monotonic 10-40 |
| 5 | 0.319 @ 477 Hz (±0.5) PASS | 0.227 @ 40 Hz (±0.3) PASS | −23.21 / 1.21 (±2.0) PASS | slope ref +5.80 dB/oct (untouched) |
| 6 | 0.460 @ 12.8 kHz (±0.5) PASS | 0.261 @ 196 Hz (±0.3) PASS | −23.06 / 1.06 (±2.0) PASS | slope +5.10, delta 0.70 vs T5 (±1.0) PASS |

(C++ `ToneBankTest` actuals at 48 kHz; rate probes: T1 0.030 / T2 0.065 /
T3 0.033 / T4 0.244 (±0.3 DEVIATION, unchanged) / T5 0.000 / T6 0.094 —
all in gate. Either-oracle advisory (±1.0): 0.82/0.44/0.33/0.47/0.23/0.28 —
all pass. Port GREEN worst diff 0.0055 dB; self-test GREEN.)

Per-finding numbers (where each came from — closed-form where visible,
bounded Nelder-Mead multi-start search only where coupled):
- Finding 1 (T1 20 Hz = 0 dB, hump gone, 10 Hz held): HP corner 14.859 →
  12.344 Hz from the anchor+knee closed form, Q 1.5 → 0.8499 from the
  hump-kill (peak/shelf frozen — the approved scoop stays bit-identical:
  bottom −7.03 dB @ 697 Hz vs CSV −7.18). Wart +3.85 → +0.32 dB max over
  10–40 Hz. Price: the 40–60 Hz foot (anchor-pinned HP ≈ 0 dB + peak skirt
  ≈ −0.5 dB vs CSV +0.5–0.6 dB) lands 0.965 @ ~49 Hz — closed-form floor,
  joint HP+peak search only moves the pain into the scoop, so gates widen
  0.6 → 1.0 (RECORDED OVERRIDE, eye wins ties).
- Finding 2 (T2 curve — NOT MET, ruling needed): numbers UNCHANGED.
  Structural evidence: below ~60 Hz only the lowshelf acts (peaks/HS ≈ 0 dB
  there) and one shelf has a single transition, but the ink needs two
  features (rise 10→27, fall 50→100) while dense pins 40 Hz at +1.04 ±0.5
  (forces gain ≈ +0.7, hence the flat floor) and the 60–150 Hz peak-skirt
  complementarity pins the corner ≈ 75 Hz. Eye-hard probe (10 Hz −0.25
  ±0.25, 27 Hz +1.3 ±0.35 as unbreakable walls) BREAKS both walls
  (+0.48/+0.66) while destroying the foot (1.72 @ 40 Hz); positive-gain
  shelves overshoot the wrong way (measured dip, no bump). Options: 5th
  section (dedicated ~27 Hz hump), peak role-change (escalate first), or
  accepted flatness.
- Finding 3 (T3/T4 convex through 0 dB @ 20 Hz): T3 LS corner 31.232 →
  14.795 Hz (knee position from the 10/20 Hz closed form) + scoop peak
  Q 0.2315 → 0.28 in-role (foot relief; Q at cap, noted); T4 LS 25.902 →
  18.189 Hz + HS 126.355 → 100.0 Hz (two-stage tilt-then-knee search: HS
  sets the +1.3–1.5 shelf over 40–200, LS cuts the knee). 20 Hz: T3 +0.27,
  T4 −0.01. T3 price: foot error 0.540 @ ~49 Hz (one knee, two places) —
  gates widen 0.5 → 0.6 / 0.3 → 0.6 (RECORDED OVERRIDE). T3/T4 separation
  above 40 Hz intact (1.0–4.7 dB apart at 100 Hz–10 kHz spots).
- Finding 4 (T6 foot in T5's slope family): LS corner 51.684 → 29.651 Hz
  (knee position from the slope closed form) + HS 240.601 → 182.857 Hz
  in-role (holds the 100–200 Hz tail the steeper knee would drop); slope
  +2.59 → +5.10 dB/oct vs T5's +5.80 (delta 0.70, gate 1.0). Anchor
  −23.06 (1.06/2.0 PASS), 40 Hz foot and 15 kHz top end still land
  (dense worst moved 40 Hz → 12.8 kHz at 0.460, in gate).
- Finding 5 (no regression): every Fix-1 gate stays green except the three
  recorded eye-price overrides (T1 dense/low 0.6 → 1.0, T3 dense 0.5 → 0.6,
  T3 low 0.3 → 0.6). T4 rate deviation unchanged (0.244/0.3). Character
  gates hold (T2 notch −20.55 dB < −12; T4 dip −4.38 dB < −2; bypass
  bit-transparent; sine-vs-magnitudeAt exact).

Thin margins (deterministic, recorded not chased): T1 dense/low 0.965/1.0 +
anchor 0.81/1.0 + 20 Hz 0.31/0.5; T2 anchor 0.95/1.0 + low 0.48/0.5
(pre-existing); T3 dense/low 0.540/0.6; T4 dense 0.480/0.5 + low 0.282/0.3
+ rate 0.244/0.3; T6 rate 0.094/0.1 (pre-existing) + slope delta 0.70/1.0;
T5 pole radius 0.99975 (< 1). Fine-grid audit (800-pt vs log-interp CSV):
gap-free on all tones (within 0.004 of on-grid worsts).

### Fix Round 1 (T2 fourth section + absolute 10 Hz anchors) COMPLETE

Authoritative user eye-reads supersede the CSV below 40 Hz: absolute 10 Hz
anchors T1 −3 / T2 −0.25 / T3 −3 / T4 −3 / T5 −22 / T6 −22 dB (gates T1–T4
±1.0, T5/T6 ±2.0 — looser by design, their HP skirts are near-vertical there).
The 10–40 Hz CSV band stays excluded (Ruling B); the anchors replace it as
ground truth. Anchors apply to the ABSOLUTE response — `magnitudeAt` includes
each tone's full cascade and ToneBank has no overall-gain stage, so the test
compares unnormalized (documented in `checkAbsoluteAnchors`). Hierarchy:
**CSV binding 40 Hz–15 kHz; six 10 Hz anchors binding; eye-read header and IR
shapes advisory**. T2 gains ONE section (twin-peak V: 610/0.43/−10.96 +
702/1.57/−9.77); all other tones numbers-only, same types. Section budget:
3×5 + 4×1.

| Tone | Dense 40–15k (gate) | Low-end 40–200 (gate) | 10 Hz abs / delta (gate) | Rate 44.1/48k (gate) |
|------|---------------------|----------------------|--------------------------|---------------------|
| 1 | 0.55 @ 55 Hz (±0.6 DEVIATION) | 0.55 @ 55 Hz (±0.6 DEVIATION) | −3.84 / 0.84 (±1.0) PASS | 0.030 (±0.1) PASS |
| 2 | 0.48 @ 86 Hz (±0.5) PASS | 0.48 @ 86 Hz (±0.5) PASS | +0.70 / 0.95 (±1.0) PASS | 0.065 (±0.1) PASS |
| 3 | 0.19 @ 173 Hz (±0.5) PASS | 0.19 @ 173 Hz (±0.3) PASS | −3.22 / 0.22 (±1.0) PASS | 0.039 (±0.1) PASS |
| 4 | 0.44 @ 10 kHz (±0.5) PASS | 0.28 @ 81 Hz (±0.3) PASS | −3.40 / 0.40 (±1.0) PASS | 0.243 (±0.3 DEVIATION) |
| 5 | 0.32 @ 477 Hz (±0.5) PASS | 0.23 @ 40 Hz (±0.3) PASS | −23.20 / 1.21 (±2.0) PASS | 0.001 (±0.1) PASS |
| 6 | 0.40 @ 12.8 kHz (±0.5) PASS | 0.28 @ 173 Hz (±0.3) PASS | −20.50 / 1.50 (±2.0) PASS | 0.094 (±0.1) PASS |

(C++ `ToneBankTest` actuals at 48 kHz; rate on the seven C++ probes.)

No-regression vs Task 20 (before → after; gates kept unless deviation noted):
dense T1 0.28 → 0.55 (DEVIATION, anchor-forced — see below), T2 0.88 →
0.48 (deviation RESOLVED by 4th section), T3 0.33 → 0.19, T4 0.34 → 0.44
(stays in gate), T5 0.18 → 0.32 (stays in gate), T6 0.13 → 0.40 (stays in
gate); low-end T1 0.19 → 0.55 (DEVIATION, anchor-forced), T2 0.50 → 0.48,
T3 0.29 → 0.19, T4 0.22 → 0.28, T5 0.08 → 0.23, T6 0.13 → 0.28 (all stay in
gate); chart either-oracle (advisory, ±1.0) 0.26/0.71/0.30/0.32/0.17/0.14 →
0.53/0.44/0.18/0.43/0.23/0.30 (all pass); port GREEN (worst diff 0.005 dB);
user-complaint directions held (T1 scoop depth now within 0.55 dB at the
anchor-constrained foot, T3 dip present, T3/T4 non-twins — verified on the
regen plot vs `u5_tone_fit_check.png`).

**T1 recorded deviation** (dense gate 0.6, low-end gate 0.6 — anchor wins per
tie-break): the −3 dB anchor pins the highpass at ~14.9 Hz while the CSV foot
sits at +0.64 dB @ 40 Hz; an anchor-pinned highpass is ~0 dB at 40 Hz, so the
closed-form floor is ~= 0.55 dB over 40–60 Hz. Four independent optimizer runs
(varied bounds/seeds/anchor pressure, widened peak-f0 and highshelf-corner
ranges) stall at dense/low 0.55–0.70 — structural, not optimizer weakness.
Wart recorded: the HP knee (Q 1.5) carries a +3.8 dB hump at ~18 Hz; it is
load-bearing for the 40 Hz foot (a hump-free knee costs dense 0.70), lives in
the excluded 10–40 Hz band, and the anchor still passes. Ruling options: 4th
section on T1, accepted deviation, or anchor relaxation.

**T4 recorded deviation** (rate gate 0.3 for T4): the anchor forces a
lowshelf-down arrangement (LS ~26 Hz, HS ~126 Hz rebuilding the mid shelf)
with a deeper peak (−6.84) for the dip; the steep recovery slope then warps
0.243 dB between 44.1k/48k at the 15 kHz probe (0.13 at 10 kHz) — a
per-section diagnostic pins the warp entirely in the peak, and rate-weighted
refits from both the Task-20 and stalled seeds floor at 0.24. Inaudible
(0.24 dB @ 15 kHz across rates); ruling options: 4th section for the top
octave, accepted deviation, or anchor relaxation.

Thin margins (deterministic float32-vs-float64 ~1e-6 class, recorded not
chased — same acceptance as Task 20's T2-low 0.496/0.5): T2 anchor 0.95/1.0
+ low 0.48/0.5 (structurally coupled through the lowshelf), T6 rate
0.094/0.1, T5 pole radius 0.99975 (< 1, float32 cook in the loop).
Fine-grid audit (800-pt vs log-interp CSV): all tones gap-free (fine worst
within 0.02 of on-grid); 15–20 kHz top cap ≤ 1.0 dB holds (T6 +0.95 worst,
report-only).

### Task 20 full-band fit to the digitized CSV COMPLETE — CSV binding ±0.5 dB

User-directed (recreate the fit-check): the tone bank now tracks
`analysis/u5_tone_curves_digitized.csv` within **±0.5 dB over 40 Hz–15 kHz
at every CSV point** on every tone except T2 (recorded deviation, below).
Hierarchy (user ruling, supersedes Tasks 12/15/17): **digitized CSV binding
full-band; eye-read header advisory; IR shapes advisory (reported, not
gated)** — the user judges the CSV + `docs/refs/u5_tone_fit_check.png`
truer than both the eye-reads and the captures. Low end keeps Task-17
gates ([40,200] Hz ±0.3 dB, T2 ±0.5); 10–40 Hz stays excluded (chart-noise
floor); highcut-ON captures stay non-adjudicating (Ruling A); HighCut.h
untouched; 3 RBJ sections per tone, types unchanged (numbers-only).

Dense-CSV worsts (C++ `ToneBankTest`, 48 kHz; T3/T5 held at Task-17 numbers):

| Tone | Before (Task-17) | After (Task-20) | Gate | Verdict |
|------|------------------|-----------------|------|---------|
| 1 | 0.57 dB @ 654 Hz | 0.28 dB @ ~1.9 kHz | ±0.5 | PASS |
| 2 | 0.97 dB @ 956 Hz | 0.88 dB @ 742 Hz | ±0.5 | DEVIATION (dense gate stays ±1.0) |
| 3 | 0.33 dB | 0.33 dB (held) | ±0.5 | PASS |
| 4 | 0.71 dB @ 742 Hz | 0.34 dB @ ~10 kHz | ±0.5 | PASS |
| 5 | 0.18 dB | 0.18 dB (held) | ±0.5 | PASS |
| 6 | 0.56 dB @ 12.0 kHz | 0.13 dB @ 40 Hz | ±0.5 | PASS |

Method: hand-rolled bounded Nelder-Mead with restarts, genuinely randomized
multi-start (24 seeds + basin-hopping polish), same optimizer discipline as
Tasks 4/12/17 (multi-start, no tip-chasing — full-grid objective, penalties
only beyond the true gates: dense, low-end, analytic poles < 1 at
44.1k + 48k, 44.1k/48k agreement ≤ 0.1 dB). Two audit catches, both fixed
before porting: (1) the stock NM stagnated without restarts (T4 sat at 0.67
until restarts unlocked 0.40); (2) T4's coarse-grid optimum hid a +0.59 dB
shelf-knee overshoot *between* CSV points at ~14.9 kHz — refit on an 800-pt
fine grid vs log-interp CSV (0.34 worst everywhere in-band; ≤ 1.0 dB above
15 kHz, report-only). All other tones verified gap-free (fine-grid worst
within 0.02 dB of on-grid worst).

**T2 recorded deviation** (best 0.88 dB, low-end 0.50 dB met): six
independent runs (minimax NM, Lp NM + basin-hopping, differential evolution
×2 incl. wide bounds with the lowshelf f0 free into the midrange, low-end
penalty removed) all stall at 0.87–0.92 dB. Structural cause: one RBJ peak
renders a rounded notch bottom; the CSV tip is a sharp V (−20.99 dB @
697 Hz, itself a line-intersection reconstruction from the retired
`fit_scripts/fit.py` (script removed in audit cuts; CSV value stands)
with fat skirts (model too deep 0.6–0.9 dB at 540–614 Hz *and* 956–1312 Hz
while the tip stays 0.7–0.9 shallow) — narrowing trades skirt for tip
forever, shelves supply only tilt. The SOS reference needed five free
sections for 0.44 dB here. Pending user ruling: topology change vs accepted
deviation. No gate loosened silently, no role changed.

Known user complaints verified on the regen `docs/tone-curves.png` against
`docs/refs/u5_tone_fit_check.png` panel by panel: T1 scoop bottom tracks the
picture's dip depth (−7.18 dB @ 697 Hz within 0.28 dB); T3 keeps its slight
low-end dip (tracked, visibly not flat); T3/T4 read as non-twins (gentle
scoop vs flat shelf + 6 kHz dip). Sub-40 Hz divergences (HP plunge on
T1/T5, shelf plateaus on T3/T4/T6 vs the falling ink) are the excluded
chart-noise band — the 5 Hz DC-blocker owns that region by design.

### Task 17 low-end refit COMPLETE — chart binding, low end to CSV

Task 17 refit T1/T2/T5 numbers-only to the digitized CSV over [40,200] Hz
(±0.3 dB, T2 ±0.5 dB per Rulings B+C — see §9); T3/T4/T6 held at Task-15
numbers. Full-band chart gate stays GREEN. §7 tables below are Task-15
history (the blend endpoint this refit preserves above crossover).

### Task 15 user blend COMPLETE — chart binding again, IR advisory

User taste ruling (Task 15): the tone bank is now a **90% Task-4 manual-chart
fit + 10% Task-12 measured-IR fit per stage-parameter blend (T6 at 95/5 —
its IR shape diverges most from the chart)**. The manual chart is the binding
oracle again (±1 dB either-oracle, Task-4 style); IR deltas are REPORTED, not
gated (the T3K captures are trusted only lightly). See §7 for the blend
rationale, derivation check, and honest deltas. Task 12's re-fit tables below
are kept as history (the IR-fit endpoint of the blend).

`analysis/ir_check.py` harness: port verification GREEN, self-test GREEN.
Task 8 measured the shapes (14 real Tone3000 captures, coherence ≥ 0.999);
Task 12 re-fitted the tone bank to the highcut-off shapes (this document).
Cross-spectral relative-shape leg (adjudicating, gate 40 Hz–15 kHz,
1 kHz-normalized), new fits vs measured:

| Tone | Before (Task 8, vs old chart fit) max/mean | After (Task 12) max/mean | Verdict vs ±1 dB |
|------|---------------------------------------------|--------------------------|------------------|
| 1 | 1.69 / 0.43 | 0.02 / 0.01 | PASS |
| 2 | 1.55 / 0.52 | 0.02 / 0.01 | PASS |
| 3 | 1.28 / 0.71 | 0.05 / 0.03 | PASS |
| 4 | 2.98 / 0.75 | 0.24 / 0.06 | PASS |
| 5 | 2.04 / 0.60 | 0.01 / 0.01 | PASS |
| 6 | 3.41 / 1.48 | 0.30 / 0.18 | PASS |

Fitted against exactly these 7 files in `analysis/ir_local/` (the user
removed the highcut-ON captures; per Ruling A below they are
non-adjudicating anyway):
`AVALON_TONE0.wav` through `AVALON_TONE6.wav` (all highcut-off, 48 kHz
24-bit stereo, ~190 s each). No WAVs committed (gitignored local dir).

## 1. Method

For each IR WAV (Task 12 set: TONE0–6 highcut-off = 7 files; the Task 8
method below was written for the 14-file set and is unchanged):

1. Load mono (manual RIFF parser: PCM 8/16/24/32-int + float32, any rate).
2. `numpy.fft.rfft` → magnitude in dB, **normalized to 0 dB at 1 kHz**
   (removes capture-gain unknowns — shape agreement only, per spec).
3. Log-interpolate onto the 121-point CSV grid; compare against
   (a) our chain (`ToneBank` tone + `HighCut` on/off per filename),
   (b) the digitized manual chart (bypass = flat 0 dB; highcut-on files get
   our highcut curve added to the chart oracle so the column stays meaningful).
4. Gate band 40 Hz–15 kHz: PASS if max |IR − ours| ≤ ±1 dB.
   The 15–20 kHz region is **reported separately, never gated**
   (printed as `15-20k (report only)`).

### Python-port verification (no build coupling, self-proving)

The RBJ / HighCut math in `ir_check.py` is transcribed verbatim from
`src/dsp/ToneBank.h` / `src/dsp/HighCut.h`. Equivalence is executed, not
asserted: `--verify-port` (which also runs automatically on every invocation
as part of the self-test section) recomputes the six per-tone either-oracle
worst deltas — min(|port−header|, |port−CSV|) over the 60 eye-read header
points parsed live from `analysis/tone_targets.h`, the same algorithm as
`tests/ToneBankTest.cpp` `checkHeaderOracle` — and requires each to match
the recorded C++ reference within 0.05 dB. Actual output (Task-20 fit):

```
[PASS] tone 1 port either-oracle worst=0.2636 dB (C++ ref 0.26, diff 0.0036, tol 0.05)
[PASS] tone 2 port either-oracle worst=0.7133 dB (C++ ref 0.71, diff 0.0033, tol 0.05)
[PASS] tone 3 port either-oracle worst=0.3032 dB (C++ ref 0.30, diff 0.0032, tol 0.05)
[PASS] tone 4 port either-oracle worst=0.3232 dB (C++ ref 0.32, diff 0.0032, tol 0.05)
[PASS] tone 5 port either-oracle worst=0.1747 dB (C++ ref 0.17, diff 0.0047, tol 0.05)
[PASS] tone 6 port either-oracle worst=0.1367 dB (C++ ref 0.14, diff 0.0033, tol 0.05)
port verification: GREEN
```

(worst diff 0.0047 dB — inside the required 0.05 dB. The references are
small because the Task-20 fit tracks the digitized chart — CSV binding,
header/IR advisory; the check verifies Python-port fidelity to the C++
header, not fit quality. Task-12-era output, kept for history: T1 1.60 /
T2 1.99 / T3 0.45 / T4 3.93 / T5 2.91 / T6 7.05 dB — large because those
fits tracked the measured IR shapes where they conflict with the chart.)

Environment: Python 3 with numpy 2.5.3, **no scipy** (FFT via `numpy.fft`;
WAV I/O via `struct` + `wave`-write only — Python 3.14's `wave` module
rejects float32 files on read, so the reader is a manual RIFF parser that
uses the fmt-chunk tag to disambiguate float32 from int32).

## 2. Synthetic self-test (GREEN, no files needed)

`python analysis/ir_check.py --self-test-only` — impulse through a float32
time-domain copy of the chain (Transposed DFII + one-pole highcut), WAV
round-trip, FFT, compare vs analytic response:

```
[PASS] flat bypass (tone 0, highcut off) max=0.0000 dB mean=0.0000 dB (tol 0.05)
[PASS] tone 3, highcut off              max=0.0038 dB mean=0.0006 dB (tol 0.05)
[PASS] tone 3, highcut on               max=0.0037 dB mean=0.0005 dB (tol 0.05)
[PASS] tone 2 notch, highcut off        max=0.0093 dB mean=0.0080 dB (tol 0.05)
[PASS] gain-invariance (x3.7)           max=6.33e-15 dB
self-test: GREEN
```

The IR leg itself was also proven end-to-end: 6 synthetic IRs
(tones 0/2/3 × highcut on/off) in a temp dir were all recovered at
vs-ours max ≤ 0.01 dB. Two real harness bugs were found and fixed by this
test: (1) float32-vs-int32 WAV misdetection for peaks > 1.0 (fixed with a
fmt-tag-sniffing manual RIFF parser); (2) filename regex `\b` rejecting
`off_48k` (fixed).

## 3. IR leg — MEASURED (7 highcut-off files present since Task 12)

Files: `analysis/ir_local/AVALON_TONE{0..6}.wav` (7 files, highcut-off
only — the highcut-ON captures were removed; per Ruling A (§6) they are
non-adjudicating). Accepted names (case-insensitive): old
`tone3_highcut_off_48k.wav` / `TONE2_HIGHCUT_ON_44k1.wav` / `tone0.wav`
forms AND the real `AVALON_TONE{n}` (highcut off) /
`AVALON_TONE{n}_HIGHCUT` (highcut on) forms. Unparseable names and
unreadable files are listed as SKIP, never guessed — and if files are
present but zero are actually compared, the script exits 2 with a loud
warning instead of looking green.

Key file finding (unchanged from Task 8): the WAVs are ~190 s 48 kHz
24-bit-stereo Tone3000 training/reamp captures (same stimulus reamped per
setting), NOT impulse responses. The harness's direct FFT leg therefore
measures mostly the stimulus spectrum (all files FAIL at ~30 dB max —
recorded below for honesty, but non-adjudicating). The valid measurement is
the Welch cross-spectral relative shape of each file vs `AVALON_TONE0.wav`
(28 × ~11 s Hann segments, 10–170 s): magnitude coherence min ≥ **0.9997**
(mean 1.0000) on every file over 40 Hz–15 kHz, so the relative curves
genuinely isolate device shape. Task 12 fitted the tone bank to these
curves (§5 table (b)); the harness direct leg below is NOT the adjudicator.

`python analysis/ir_check.py` on the 7 files: 7 parsed, 7 compared,
7 FAIL vs the ±1 dB gate (direct-FFT leg, exit 2 — stimulus-dominated,
see §5 table (a)).

## 4. Conflict-point adjudication — RESOLVED by Task 12 re-fit

Task 4 found the eye-read header (`analysis/tone_targets.h`) and the machine
CSV disagreeing by >1 dB. Recomputation notes: 13 points exceed 1 dB in
absolute space (Task 4 claimed 14; the 14th is interpolation-dependent —
T3@400 sits at 0.95–0.97 dB). But IRs are gain-normalized, so only
**shape-space** (1 kHz-normalized) conflicts are adjudicable by them: 11
points. Values below are dB in shape space (header minus its 1 kHz value;
CSV/ours normalized at 1 kHz):

| # | Point | Header | CSV | Old fit | \|H−C\| | Notes |
|---|-------|--------|-----|------|---------|-------|
| 1 | T2@40 | +15.00 | +13.89 | +14.29 | 1.11 | appears only after normalization (header 1 kHz ref itself differs) |
| 2 | T2@80 | +14.30 | +12.13 | +13.61 | 2.17 | old fit between, nearer header |
| 3 | T2@150 | +12.00 | +9.07 | +10.08 | 2.93 | old fit between |
| 4 | T2@400 | +5.50 | +0.83 | +1.82 | 4.67 | largest; old fit near CSV |
| 5 | T2@700 | −4.00 | −8.10 | −6.76 | 4.10 | notch tip zone; old fit between |
| 6 | T2@2000 | +8.50 | +7.46 | +8.63 | 1.04 | borderline; old fit near header |
| 7 | T4@4000 | −2.70 | −4.09 | −3.22 | 1.39 | dip skirt; old fit between |
| 8 | T4@10000 | −1.70 | −3.20 | −2.25 | 1.50 | old fit between |
| 9 | T5@40 | −14.00 | −15.07 | −15.07 | 1.07 | HP foot; old fit = CSV |
| 10 | T5@400 | −2.50 | −1.13 | −1.05 | 1.37 | old fit = CSV |
| 11 | T6@150 | −7.00 | −5.58 | −5.50 | 1.42 | old fit = CSV |

**Decision rule per point** (applied to the real IR value at that freq):
IR within 1 dB of one side only → that side wins; within 1 dB of both →
no practical conflict; within 1 dB of neither → both lose, flag for re-tune.
Absolute-only conflicts (T1@80/150, T2@1000, T5@150/700 in raw space) are
level disagreements the normalized IRs *cannot* adjudicate — they need
absolute-level captures or the ear A/B leg.

**Task 8 measured verdicts** (IR = cross-spectral relative shape, highcut-off
files, 1 kHz-normalized; |dH|/|dC| = distance to header/CSV in dB):

| Point | IR | Header | CSV | Old fit | \|dH\| | \|dC\| | Verdict |
|-------|------|--------|-----|------|---------|---------|---------|
| T2@40 | +12.75 | +15.00 | +13.89 | +14.29 | 2.25 | 1.14 | NEITHER — both lose |
| T2@80 | +12.36 | +14.30 | +12.13 | +13.61 | 1.94 | 0.23 | CSV wins (old fit 1.25 off IR) |
| T2@150 | +10.19 | +12.00 | +9.07 | +10.08 | 1.81 | 1.12 | NEITHER — both lose (old fit 0.11 off IR) |
| T2@400 | +1.14 | +5.50 | +0.83 | +1.82 | 4.36 | 0.31 | CSV wins |
| T2@700 | −6.49 | −4.00 | −8.10 | −6.76 | 2.49 | 1.61 | NEITHER — both lose (notch-tip zone: narrow notch, positional sensitivity; old fit 0.27 off IR) |
| T2@2000 | +8.43 | +8.50 | +7.46 | +8.63 | 0.07 | 0.97 | BOTH — no practical conflict |
| T4@4000 | −2.56 | −2.70 | −4.09 | −3.22 | 0.14 | 1.53 | HEADER wins |
| T4@10000 | −3.92 | −1.70 | −3.20 | −2.25 | 2.22 | 0.72 | CSV wins |
| T5@40 | −13.36 | −14.00 | −15.07 | −15.07 | 0.64 | 1.72 | HEADER wins (old fit = CSV, 1.71 off IR) |
| T5@400 | −0.71 | −2.50 | −1.13 | −1.05 | 1.79 | 0.43 | CSV wins |
| T6@150 | −3.17 | −7.00 | −5.58 | −5.50 | 3.83 | 2.41 | NEITHER — both lose (old fit = CSV side) |

Tally: CSV 4 (T2@80, T2@400, T4@10000, T5@400), header 2 (T4@4000, T5@40),
both 1 (T2@2000), neither 4 (T2@40, T2@150, T2@700, T6@150).

**Task 12 resolution** (new fits vs the same IR values; shapes recomputed
fresh from the 7 highcut-off WAVs — values below from the Task 12 leg,
minor grid-interp differences vs Task 8 leg up to 0.02 dB):

| Point | IR | New fit | \|d\| | Call |
|-------|------|---------|------|------|
| T2@40 | +12.75 | +12.76 | 0.01 | was NEITHER — fit takes the IR side |
| T2@80 | +12.36 | +12.36 | 0.01 | was CSV — fit confirms |
| T2@150 | +10.19 | +10.19 | 0.00 | was NEITHER — fit takes the IR side |
| T2@400 | +1.15 | +1.15 | 0.00 | was CSV — fit confirms |
| T2@700 | −6.51 | −6.52 | 0.01 | was NEITHER (notch-tip zone) — fit matches tip depth character (-6.85dB @ 658Hz vs measured -6.84dB @ 661Hz); single-bin tip not chased |
| T2@2000 | +8.43 | +8.43 | 0.00 | was BOTH — fit confirms |
| T4@4000 | −2.56 | −2.32 | 0.24 | was HEADER — fit confirms (dip skirt) |
| T4@10000 | −3.91 | −3.73 | 0.18 | was CSV — fit confirms |
| T5@40 | −13.41 | −13.42 | 0.01 | was HEADER — fit confirms |
| T5@400 | −0.70 | −0.69 | 0.01 | was CSV — fit confirms |
| T6@150 | −3.17 | −2.94 | 0.23 | was NEITHER — fit takes the IR side |

All 11 conflict points now sit within 0.24 dB of the measured shape. The
four NEITHER points are resolved by trusting the IR (coherence ≥ 0.9997):
T2@40/T2@150/T6@150 land on the IR side; T2@700 matches tip depth/position
without chasing the single-bin minimum. No gate was loosened: every tone
passes ±1 dB vs measured over the full 40 Hz–15 kHz band
(worsts 0.016/0.012/0.056/0.246/0.017/0.306 dB).

## 5. Results tables (Task 12 leg: 7 highcut-off files, 0 SKIP)

(a) Harness direct-FFT leg (for the record — stimulus-dominated, NOT
adjudicating; exit 2). Every file carries the same ~190 s stimulus, so the
raw FFT measures the stimulus, not the device:

| File | vs-ours max/mean | vs-chart max/mean | Verdict |
|------|------------------|-------------------|---------|
| AVALON_TONE0.wav | 30.19 / 14.14 | 30.19 / 14.14 | FAIL (stimulus) |
| AVALON_TONE1.wav | 30.18 / 14.14 | 30.05 / 13.99 | FAIL (stimulus) |
| AVALON_TONE2.wav | 30.19 / 14.14 | 30.73 / 14.06 | FAIL (stimulus) |
| AVALON_TONE3.wav | 30.23 / 14.14 | 29.51 / 14.00 | FAIL (stimulus) |
| AVALON_TONE4.wav | 30.27 / 14.15 | 29.59 / 14.05 | FAIL (stimulus) |
| AVALON_TONE5.wav | 30.18 / 14.14 | 32.09 / 14.71 | FAIL (stimulus) |
| AVALON_TONE6.wav | 30.19 / 14.06 | 33.49 / 15.55 | FAIL (stimulus) |

(b) Cross-spectral relative-shape leg (adjudicating; Welch cross-spectrum
vs TONE0, 28 × ~11 s Hann segments over 10–170 s, 1 kHz-normalized, gate
40 Hz–15 kHz; coherence min ≥ 0.9997, mean 1.0000 on all 7 files).
New fits (Task 12) vs old chart fits (Task 8 leg for reference):

| Tone | Old fit vs-ours max/mean | New fit vs-ours max/mean | New fit vs-chart max/mean | Verdict vs ±1 dB |
|------|--------------------------|--------------------------|---------------------------|------------------|
| 1 | 1.69 / 0.43 | 0.02 / 0.01 | 1.83 / 0.56 | PASS (was FAIL) |
| 2 | 1.55 / 0.52 | 0.02 / 0.01 | 1.97 / 1.04 | PASS (was FAIL) |
| 3 | 1.28 / 0.71 | 0.05 / 0.03 | 1.17 / 0.49 | PASS (was FAIL) |
| 4 | 2.98 / 0.75 | 0.24 / 0.06 | 2.44 / 0.57 | PASS (was FAIL) |
| 5 | 2.04 / 0.60 | 0.01 / 0.01 | 2.06 / 0.66 | PASS (was FAIL) |
| 6 | 3.41 / 1.48 | 0.30 / 0.18 | 3.37 / 1.60 | PASS (was FAIL) |

The vs-chart column now records honest divergence: where the manual chart
and the hardware disagree, the fits follow the hardware. The chart remains
the only oracle outside 40 Hz–15 kHz (not asserted — see tests).

## 6. Highcut on/off check — RULING A (user): highcut-ON captures are NON-ADJUDICATING

The user rules that our 1-pole −3 dB @ 8 kHz highcut (manual spec) stands
and nothing is fitted to highcut-on files; `src/dsp/HighCut.h` is untouched
by Task 12. Technical support, kept visible (measured Task 8 leg on the
then-complete 14-file set, median-normalized 100 Hz–1 kHz): per-pair on−off
curves vary far beyond what a fixed post-EQ filter can produce — T0:
−3.06 dB @ 4 kHz / −7.09 dB @ 8 kHz / −11.78 dB @ 15 kHz vs T4: −0.59 @
4 kHz / −2.68 @ 8 kHz vs our 1-pole −1.02/−3.00/−5.75 dB. A 1-pole lowpass
fit to the T0 on−off curve lands at fc ≈ 3.65 kHz (mean residual 0.04 dB,
max 0.77 dB), and all seven on-files read FAIL vs ours (max 5.98–21.52 dB).
Because each pair's tone stack shapes the stimulus energy distribution
before the highcut — and the reamp captures are stimulus-dominated — the
capture conditions, not just the filter, differ between files. The T0 pair
is the cleanest read of the filter, but even it (fc ≈ 3.65 kHz vs specced
8 kHz) contradicts a fixed-filter interpretation across pairs, so the
highcut-ON captures do not adjudicate. The highcut-ON WAVs have been
removed from `analysis/ir_local/`; the 7 highcut-off files above are the
complete fitting set.

## 7. Task 15 user blend — 90/10 chart/IR (T6 95/5): rationale + honest deltas

**Ruling.** The user trusts the T3K captures only lightly: the manual chart
binds again, the IR contributes a small advisory dose. Weights: T1–T5 90%
chart / 10% IR, T6 95% / 5% (its IR shape diverges most from the chart —
e.g. T6@150 Hz shape-space: header −7.00, CSV −5.58, IR −3.17 — so it keeps
the lightest IR dose). Gate hierarchy inverted vs Task 12: chart gated
(±1 dB either-oracle, Task-4 style, in `tests/ToneBankTest.cpp`), IR
reported only. HighCut.h untouched (Ruling A stands); 3 sections per tone.

**Derivation (independently re-derived, not trusted from the tree).**
Linear blend per stage-parameter in f0/Q/gainDb:
`blend = w*chart + (1-w)*IR` with w = 0.9 (0.95 on T6), using the Task-4
chart fit (`84caf2c:src/dsp/ToneBank.h`) and the Task-12 IR fit
(`HEAD:src/dsp/ToneBank.h`). Every stage kept its filter type on both sides,
so no role mapping was needed — but stage ROLES still moved (T4s0: 794 Hz
chart shelf vs 46 Hz IR shelf → blend 719.2 Hz; T6s1 kept the chart's
+2.5 dB restore at 95/5 → +2.24 dB @ 292 Hz, deliberately NOT the IR's
−2.75 dB mid cut, which would need a role change). The tree's pre-written
numbers were re-derived from scratch and matched exactly (within rounding),
so they were KEPT — verified, not replaced.

**Deviation bound (param-blend vs true dB-domain blend).** The honest blend
target is `w*chart(f) + (1-w)*IR(f)` in dB; parameter-space blending is an
approximation. Dense-grid (2000-pt, 40 Hz–15 kHz, 48 kHz) worst deviations:

| Tone | Param-blend vs dB-blend worst | Where |
|------|-------------------------------|-------|
| 1 | 0.24 dB | ~12.6 kHz |
| 2 | 0.33 dB | ~89 Hz |
| 3 | 0.03 dB | ~316 Hz |
| 4 | 0.38 dB | ~10.4 kHz |
| 5 | 0.04 dB | ~198 Hz |
| 6 | 0.12 dB | ~402 Hz |

Worst 0.38 dB on T4 — the role-moved tone, as expected; the 90% chart
weight keeps it near-chart. Poles re-checked < 1 at 44.1 kHz + 48 kHz
(max radius 0.9985); 44.1k/48k invariance ≤ 0.07 dB at all gate probes.

**Chart gate (binding, `ctest` GREEN).** Either-oracle worsts per tone
(C++ `ToneBankTest` output, matches the independent Python check):

| Tone | Either-oracle worst | Dense-CSV worst | Verdict vs ±1 dB |
|------|--------------------|-----------------|------------------|
| 1 | 0.44 dB | 0.54 dB @ ~55 Hz | PASS |
| 2 | 0.77 dB | 0.98 dB @ ~576 Hz | PASS (thinnest margin, recorded) |
| 3 | 0.31 dB | 0.33 dB @ ~4.7 kHz | PASS |
| 4 | 0.67 dB | 0.71 dB @ ~742 Hz | PASS |
| 5 | 0.32 dB | 0.34 dB @ ~143 Hz | PASS |
| 6 | 0.37 dB | 0.56 dB @ ~12.0 kHz | PASS |

Shape continuity on role-moved stages: T2 notch tip −6.88 dB @ 676 Hz
(1 kHz-normalized) vs measured −6.84 dB @ 661 Hz — depth character kept;
T4 dip −4.74 dB @ 5922 Hz normalized, formed jointly by peak + high shelf.

**IR deltas (REPORTED, not gated — the honest cost of the ruling).**
Blend vs the 14-spot measured-shape table (`kIrSpots`, 1 kHz-normalized):

| Tone | IR-shape worst (spots) | At |
|------|------------------------|-----|
| 1 | 1.50 dB | 10 kHz |
| 2 | 1.33 dB | 40 Hz |
| 3 | 1.12 dB | 150 Hz |
| 4 | 2.36 dB | 15 kHz |
| 5 | 1.80 dB | 100 Hz |
| 6 | 3.38 dB | 15 kHz |

These exceed ±1 dB exactly where the chart and the hardware disagree
(T4/T6 high end, T5 low end) — the blend follows the chart there by design.
`docs/tone-curves.png` regenerated from the final blended numbers.

## 9. Task 17 low-end refit — CSV tracking over [40,200] Hz (Rulings B+C)

**Ruling B (region).** The low-end gate covers **[40,200] Hz**, not
[10,200]. Round-1 optimizer evidence (6-seed Nelder-Mead, all stages free:
bests T1 0.88 / T2 1.16 / T3 2.66 / T4 3.82 / T5 0.98 / T6 1.68 dB —
failing by 3–13×, not edge distance) plus the analytic DC-value proof
(no-HP tones T2/T3/T4: model(DC) = shelf gain cannot satisfy both the DC
read and the 27–118 Hz rises) showed the [10,200] gate infeasible
numbers-only. Measurement reason to exclude 10–40 Hz: T1/T3/T4 CSV traces
are identical within ≤0.11 dB at every CSV point 10–26 Hz despite three
different low-end circuits (common-mode ink/frame-edge artifact, not
device response); T5/T6 foot slopes (~4.8 dB/oct) are incompatible with
any biquad HP (12 dB/oct asymptote). The 10–40 band is chart-noise floor;
the 5 Hz DC-blocker owns sub-40 behavior by design. Bass fundamentals
start at 41 Hz (low E). No contradiction persists inside [40,200] — no
escalation was needed.

**Ruling C (T2 gate).** T2 gets **±0.5 dB** over [40,200] (boundary tension
at ~196 Hz: crossover proximity + notch skirt; 0.37 achievable vs 0.3
asked — physics accepted, reality gated).

**Fit (numbers-only; no new sections, no role changes).** Nelder-Mead,
genuinely randomized multi-start (8 seeds), penalties only beyond the true
gates (dense ≤ 1.0, header either-oracle ≤ 1.0, drift-vs-Task-15 ≤ 0.3,
analytic pole check at 44.1k + 48k):

| Tone | Change (T1/T2/T5 only) | Low-band worst [40,200] | Gate | Drift vs Task-15 |
|------|------------------------|-------------------------|------|------------------|
| 1 | HP 21.7/0.966 → 36.1/1.052; peak 803.1/0.20/−6.88 → 777.1/0.173/−6.64 | 0.20 dB @ 126 Hz | ±0.3 PASS | 0.25 |
| 2 | peak 677.9/0.727/−20.97 → 681.2/0.687/−21.03; LS 93.9/1.504/+0.68 → 87.2/1.646/+0.85; HS 3218.5/0.23/+2.30 → 3129.2/0.173/+2.65 | 0.38 dB @ 71 Hz (twin peak 0.375 @ 196 Hz — same value both ends of the band) | ±0.5 PASS | 0.26 in-band |
| 3 | held | 0.28 dB @ 67 Hz | ±0.3 PASS | 0 |
| 4 | held | 0.28 dB @ 196 Hz | ±0.3 PASS | 0 |
| 5 | HP 32.8/0.305 → 33.7/0.305; LS 126.4/0.734/−3.68 → 133.2/0.738/−3.74 | 0.08 dB @ 40 Hz | ±0.3 PASS | 0.13 |
| 6 | held | 0.29 dB @ 59 Hz | ±0.3 PASS | 0 |

Rejoin is smooth: above-crossover drift ≤ 0.27 dB in-band on all tones,
and the full-band chart gate (dense ≤ 1.0 + header either-oracle ≤ 1.0)
stays GREEN — either-oracle worsts T1 0.32 / T2 0.78 / T3 0.30 / T4 0.67 /
T5 0.17 / T6 0.37 dB; dense worsts T1 0.57 / T2 0.97 / T3 0.33 / T4 0.71 /
T5 0.18 / T6 0.56 dB (T2's 0.97 improves on the Task-15 0.98).

**Honest costs.** T2's notch tip eases 0.4 dB (now −6.46 dB @ 678 Hz
1 kHz-normalized vs measured −6.84 @ 661 Hz — the high-shelf lift that
fixes the low end); T1's drift sits at 0.25 (0.05 margin, deterministic).
T2's dense 0.97 margin (0.03) is thin but strictly better than Task-15's
0.98. `docs/tone-curves.png` regenerated from the final numbers.
Gated in `tests/ToneBankTest.cpp` (`checkLowEndCsv`); port re-verified
(`--verify-port` GREEN against the new C++ refs).

## 8. Zoom leg — PENDING (user-side, ear A/B cannot be automated)

1. Record the same riff: DI plus Tones 2/3/4 through the plugin, same input
   level, matched output level, 48 kHz WAV.
2. Name them `riff_di.wav` / `riff_tone2.wav` / `riff_tone3.wav` /
   `riff_tone4.wav` in `analysis/zoom_ref/` (gitignored).
3. Level-match (RMS over the riff body, ±0.1 dB), then spectral-compare each
   tone capture vs the DI (long-time-average FFT, 1/3-octave smoothing):
   the difference curve should sit on the corresponding `chain_db` shape
   within ±1 dB, 40 Hz–15 kHz, same gate as the IR leg.
4. Final call is ears: flip between DI and each tone at matched loudness;
   T2's scoop, T3's gentle dip, and T4's presence lift should be clearly
   audible and free of harshness/clicks on tone switches.
