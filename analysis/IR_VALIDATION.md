# IR validation (Tasks 8 + 12) — method, status, and measured results

## Status: Task 12 re-fit COMPLETE — all 6 tones track the measured shapes

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
the recorded C++ reference within 0.05 dB. Actual output:

```
[PASS] tone 1 port either-oracle worst=1.6032 dB (C++ ref 1.62, diff 0.0168, tol 0.05)
[PASS] tone 2 port either-oracle worst=1.9851 dB (C++ ref 1.98, diff 0.0051, tol 0.05)
[PASS] tone 3 port either-oracle worst=0.4537 dB (C++ ref 0.45, diff 0.0037, tol 0.05)
[PASS] tone 4 port either-oracle worst=3.9347 dB (C++ ref 3.94, diff 0.0053, tol 0.05)
[PASS] tone 5 port either-oracle worst=2.9102 dB (C++ ref 2.91, diff 0.0002, tol 0.05)
[PASS] tone 6 port either-oracle worst=7.0526 dB (C++ ref 7.05, diff 0.0026, tol 0.05)
port verification: GREEN
```

(worst diff 0.0168 dB — inside the required 0.05 dB. The references are
large because the Task 12 fits track the measured IR shapes, which
supersede both oracles where they conflict; the check verifies Python-port
fidelity to the C++ header, not fit quality.)

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

## 7. Zoom leg — PENDING (user-side, ear A/B cannot be automated)

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
