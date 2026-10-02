# IR validation (Task 8) — method, status, and measured results

## Status: measured on 14 real Tone3000 captures (2026-10-02, completed set)

`analysis/ir_check.py` harness: port verification GREEN, self-test GREEN.
Filename fix (`parse_ir_name` accepts `AVALON_TONE{n}[_HIGHCUT]`) lets all
14 local files parse and compare (0 SKIP). The set is now complete —
`AVALON_TONE2_HIGHCUT.wav` was added after the initial 13-file run.

Key file finding: the 13 WAVs are ~190 s 48 kHz 24-bit-stereo Tone3000
training/reamp captures (same stimulus reamped per setting), NOT impulse
responses. The harness's direct FFT leg therefore measures mostly the
stimulus spectrum (all 13 FAIL at 27–44 dB max — recorded below for honesty,
but non-adjudicating). The valid measurement is the Welch cross-spectral
relative shape of each file vs `AVALON_TONE0.wav` (14 × ~11 s Hann segments,
10–170 s): magnitude coherence rounds to **1.000 (min ≥ 0.999)** on every
file over 40 Hz–15 kHz, so the relative curves genuinely isolate device
shape. All verdicts below come from that leg. No DSP was tuned from these
numbers — re-tune is a follow-up decision for the user. No WAVs committed
(gitignored local dir).

## 1. Method

For each IR WAV (expected: TONE0–6 × HIGHCUT on/off = 14 files):

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
[PASS] tone 1 port either-oracle worst=0.4742 dB (C++ ref 0.47, diff 0.0042, tol 0.05)
[PASS] tone 2 port either-oracle worst=0.7506 dB (C++ ref 0.75, diff 0.0006, tol 0.05)
[PASS] tone 3 port either-oracle worst=0.2706 dB (C++ ref 0.27, diff 0.0006, tol 0.05)
[PASS] tone 4 port either-oracle worst=0.4835 dB (C++ ref 0.48, diff 0.0035, tol 0.05)
[PASS] tone 5 port either-oracle worst=0.1112 dB (C++ ref 0.11, diff 0.0012, tol 0.05)
[PASS] tone 6 port either-oracle worst=0.3005 dB (C++ ref 0.30, diff 0.0005, tol 0.05)
port verification: GREEN
```

(worst diff 0.0042 dB — far inside the required 0.05 dB).

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
[PASS] tone 3, highcut off              max=0.0009 dB mean=0.0006 dB (tol 0.05)
[PASS] tone 3, highcut on               max=0.0010 dB mean=0.0005 dB (tol 0.05)
[PASS] tone 2 notch, highcut off        max=0.0144 dB mean=0.0096 dB (tol 0.05)
[PASS] gain-invariance (x3.7)           max=6.22e-15 dB
self-test: GREEN
```

The IR leg itself was also proven end-to-end: 6 synthetic IRs
(tones 0/2/3 × highcut on/off) in a temp dir were all recovered at
vs-ours max ≤ 0.01 dB. Two real harness bugs were found and fixed by this
test: (1) float32-vs-int32 WAV misdetection for peaks > 1.0 (fixed with a
fmt-tag-sniffing manual RIFF parser); (2) filename regex `\b` rejecting
`off_48k` (fixed).

## 3. IR leg — MEASURED (files present since 2026-10-02)

Files: `analysis/ir_local/AVALON_TONE{0..6}[_HIGHCUT].wav` (14 files,
complete set). Accepted names
(case-insensitive): old `tone3_highcut_off_48k.wav` /
`TONE2_HIGHCUT_ON_44k1.wav` / `tone0.wav` forms AND the real
`AVALON_TONE{n}` (highcut off) / `AVALON_TONE{n}_HIGHCUT` (highcut on)
forms. Unparseable names and unreadable files are listed as SKIP, never
guessed — and if files are present but zero are actually compared, the
script exits 2 with a loud warning instead of looking green.

`python analysis/ir_check.py` on the 14 files: 14 parsed, 14 compared,
14 FAIL vs the ±1 dB gate (direct-FFT leg, exit 2 — stimulus-dominated,
see §5 table (a)). The adjudicating measurement is the cross-spectral
relative-shape leg (§5 table (b)): per-file transfer function vs the
TONE0-off capture, Welch-averaged (14 segments), coherence ≥ 0.999
everywhere in-band. Do NOT tune DSP in this step — this task measures;
re-tune is a follow-up decision for the user.

## 4. Conflict-point adjudication — PENDING real IRs (decision procedure)

Task 4 found the eye-read header (`analysis/tone_targets.h`) and the machine
CSV disagreeing by >1 dB. Recomputation notes: 13 points exceed 1 dB in
absolute space (Task 4 claimed 14; the 14th is interpolation-dependent —
T3@400 sits at 0.95–0.97 dB). But IRs are gain-normalized, so only
**shape-space** (1 kHz-normalized) conflicts are adjudicable by them: 11
points. Values below are dB in shape space (header minus its 1 kHz value;
CSV/ours normalized at 1 kHz):

| # | Point | Header | CSV | Ours | \|H−C\| | Notes |
|---|-------|--------|-----|------|---------|-------|
| 1 | T2@40 | +15.00 | +13.89 | +14.29 | 1.11 | appears only after normalization (header 1 kHz ref itself differs) |
| 2 | T2@80 | +14.30 | +12.13 | +13.61 | 2.17 | ours between, nearer header |
| 3 | T2@150 | +12.00 | +9.07 | +10.08 | 2.93 | ours between |
| 4 | T2@400 | +5.50 | +0.83 | +1.82 | 4.67 | largest; ours near CSV |
| 5 | T2@700 | −4.00 | −8.10 | −6.76 | 4.10 | notch tip zone; ours between |
| 6 | T2@2000 | +8.50 | +7.46 | +8.63 | 1.04 | borderline; ours near header |
| 7 | T4@4000 | −2.70 | −4.09 | −3.22 | 1.39 | dip skirt; ours between |
| 8 | T4@10000 | −1.70 | −3.20 | −2.25 | 1.50 | ours between |
| 9 | T5@40 | −14.00 | −15.07 | −15.07 | 1.07 | HP foot; ours = CSV |
| 10 | T5@400 | −2.50 | −1.13 | −1.05 | 1.37 | ours = CSV |
| 11 | T6@150 | −7.00 | −5.58 | −5.50 | 1.42 | ours = CSV |

**Decision rule per point** (applied to the real IR value at that freq):
IR within 1 dB of one side only → that side wins; within 1 dB of both →
no practical conflict; within 1 dB of neither → both lose, flag for re-tune.
Absolute-only conflicts (T1@80/150, T2@1000, T5@150/700 in raw space) are
level disagreements the normalized IRs *cannot* adjudicate — they need
absolute-level captures or the ear A/B leg.

**Measured verdicts** (IR = cross-spectral relative shape, highcut-off
files, 1 kHz-normalized; |dH|/|dC| = distance to header/CSV in dB):

| Point | IR | Header | CSV | Ours | \|dH\| | \|dC\| | Verdict |
|-------|------|--------|-----|------|---------|---------|---------|
| T2@40 | +12.75 | +15.00 | +13.89 | +14.29 | 2.25 | 1.14 | NEITHER — both lose |
| T2@80 | +12.36 | +14.30 | +12.13 | +13.61 | 1.94 | 0.23 | CSV wins (ours 1.25 off IR) |
| T2@150 | +10.19 | +12.00 | +9.07 | +10.08 | 1.81 | 1.12 | NEITHER — both lose (ours 0.11 off IR) |
| T2@400 | +1.14 | +5.50 | +0.83 | +1.82 | 4.36 | 0.31 | CSV wins |
| T2@700 | −6.49 | −4.00 | −8.10 | −6.76 | 2.49 | 1.61 | NEITHER — both lose (notch-tip zone: narrow notch, positional sensitivity; ours 0.27 off IR) |
| T2@2000 | +8.43 | +8.50 | +7.46 | +8.63 | 0.07 | 0.97 | BOTH — no practical conflict |
| T4@4000 | −2.56 | −2.70 | −4.09 | −3.22 | 0.14 | 1.53 | HEADER wins |
| T4@10000 | −3.92 | −1.70 | −3.20 | −2.25 | 2.22 | 0.72 | CSV wins |
| T5@40 | −13.36 | −14.00 | −15.07 | −15.07 | 0.64 | 1.72 | HEADER wins (ours = CSV, 1.71 off IR) |
| T5@400 | −0.71 | −2.50 | −1.13 | −1.05 | 1.79 | 0.43 | CSV wins |
| T6@150 | −3.17 | −7.00 | −5.58 | −5.50 | 3.83 | 2.41 | NEITHER — both lose (ours = CSV side) |

Tally: CSV 4 (T2@80, T2@400, T4@10000, T5@400), header 2 (T4@4000, T5@40),
both 1 (T2@2000), neither 4 (T2@40, T2@150, T2@700, T6@150). Ours sits
within 1.71 dB of the IR at 10 of 11 points (worst: T6@150 at 2.33 dB);
the four NEITHER points are genuine re-tune candidates for a follow-up —
no tuning done here.

## 5. Results tables (measured 2026-10-02, 13 files, 0 SKIP)

(a) Harness direct-FFT leg (for the record — stimulus-dominated, NOT
adjudicating; exit 2). Every file carries the same ~190 s stimulus, so the
raw FFT measures the stimulus, not the device:

| File | vs-ours max/mean | vs-chart max/mean | Verdict |
|------|------------------|-------------------|---------|
| AVALON_TONE0.wav | 30.19 / 14.14 | 30.19 / 14.14 | FAIL (stimulus) |
| AVALON_TONE0_HIGHCUT.wav | 30.48 / 15.12 | 30.48 / 15.12 | FAIL (stimulus) |
| AVALON_TONE1.wav | 30.02 / 14.00 | 30.05 / 13.99 | FAIL (stimulus) |
| AVALON_TONE1_HIGHCUT.wav | 27.40 / 14.78 | 27.59 / 14.76 | FAIL (stimulus) |
| AVALON_TONE2.wav | 29.32 / 13.70 | 30.73 / 14.06 | FAIL (stimulus) |
| AVALON_TONE2_HIGHCUT.wav | 31.74 / 15.75 | 30.65 / 15.80 | FAIL (stimulus) |
| AVALON_TONE3.wav | 29.16 / 13.94 | 29.51 / 14.00 | FAIL (stimulus) |
| AVALON_TONE3_HIGHCUT.wav | 27.68 / 14.55 | 28.07 / 14.59 | FAIL (stimulus) |
| AVALON_TONE4.wav | 28.79 / 13.95 | 29.59 / 14.05 | FAIL (stimulus) |
| AVALON_TONE4_HIGHCUT.wav | 29.30 / 14.14 | 30.11 / 14.26 | FAIL (stimulus) |
| AVALON_TONE5.wav | 31.96 / 14.70 | 32.09 / 14.71 | FAIL (stimulus) |
| AVALON_TONE5_HIGHCUT.wav | 43.49 / 17.97 | 43.63 / 17.98 | FAIL (stimulus) |
| AVALON_TONE6.wav | 33.53 / 15.52 | 33.49 / 15.55 | FAIL (stimulus) |
| AVALON_TONE6_HIGHCUT.wav | 43.90 / 17.88 | 43.87 / 17.90 | FAIL (stimulus) |

(b) Cross-spectral relative-shape leg (adjudicating; Welch cross-spectrum
vs TONE0-off, 1 kHz-normalized, gate 40 Hz–15 kHz; coherence min ≥ 0.999,
mean 1.0000 on all 14 files):

| Tone | Highcut | vs-ours max/mean | vs-chart max/mean | Verdict vs ±1 dB |
|------|---------|------------------|-------------------|------------------|
| 0 | off | 0.00 / 0.00 (ref) | 0.00 / 0.00 (ref) | REF |
| 0 | on | 5.98 / 1.20 | 5.98 / 1.20 | FAIL — highcut steeper than ours (see §6) |
| 1 | off | 1.69 / 0.43 | 1.82 / 0.56 | FAIL (worst 1.69) |
| 1 | on | 13.75 / 4.05 | 13.75 / 4.10 | FAIL — highcut (see §6) |
| 2 | off | 1.55 / 0.52 | 1.96 / 1.04 | FAIL (worst 1.55) |
| 2 | on | 21.52 / 10.62 | 20.26 / 9.75 | FAIL — highcut (see §6, worst of all on-files: notch + steep skirt stack) |
| 3 | off | 1.28 / 0.71 | 1.17 / 0.50 | FAIL (worst 1.28) |
| 3 | on | 9.68 / 2.89 | 9.80 / 2.67 | FAIL — highcut (see §6) |
| 4 | off | 2.98 / 0.75 | 2.28 / 0.56 | FAIL (worst 2.98) |
| 4 | on | 6.74 / 0.93 | 6.19 / 0.76 | FAIL — highcut (see §6) |
| 5 | off | 2.04 / 0.60 | 2.07 / 0.66 | FAIL (worst 2.04) |
| 5 | on | 15.28 / 4.07 | 15.28 / 4.07 | FAIL — highcut (see §6) |
| 6 | off | 3.41 / 1.48 | 3.34 / 1.47 | FAIL (worst 3.41) |
| 6 | on | 15.26 / 3.99 | 15.53 / 3.98 | FAIL — highcut (see §6) |

## 6. Highcut on/off check (MEASURED — finding, no tuning)

The T0 pair (`AVALON_TONE0_HIGHCUT.wav` vs `AVALON_TONE0.wav`) isolates the
highcut stage with stimulus exactly cancelled (20–50 s FFT cross-correlation
0.997; all six pairs share the identical stimulus). Measured on−off curve
(median-normalized 100 Hz–1 kHz): −3.06 dB @ 4 kHz, −7.09 dB @ 8 kHz,
−11.78 dB @ 15 kHz — far steeper than our 1-pole −3 dB @ 8 kHz
(−1.02 / −3.00 / −5.75 dB). A 1-pole lowpass fit over 40 Hz–15 kHz lands at
**fc ≈ 3.65 kHz** (mean residual 0.04 dB, max 0.77 dB; spot check vs fit:
+0.33/+0.32 @ 100 Hz, −3.06/−3.10 @ 4 kHz, −7.09/−7.06 @ 8 kHz,
−11.78/−10.93 @ 15 kHz — the 15 kHz endpoint is 0.85 dB off the 1-pole fit,
so order/skirt needs a follow-up look). All seven on-files consequently read
FAIL vs ours (max 5.98–21.52 dB, table (b)); per-tone on−off curves vary
(T4 closest: −0.59 @ 4 kHz / −2.68 @ 8 kHz vs our −1.02/−3.00) because each
pair's tone stack shapes the stimulus energy distribution before the
highcut — the T0 pair is the clean read. Recorded only; highcut re-tune is
a follow-up decision for the user, NOT done here.

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
