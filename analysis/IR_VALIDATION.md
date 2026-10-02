# IR validation (Task 8) — method, status, and pending-file instructions

## Status: harness GREEN, both measurement legs pending local files

`analysis/ir_check.py` (stdlib + numpy only, no scipy) is validated on
synthetic data and exits 0 in absent-files mode. No real Tone3000 IR or Zoom
capture has been measured yet — the per-tone deltas and the conflict-point
adjudication below are the exact procedures to run once the files exist.
No WAVs are committed (gitignored local dirs).

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
   The 10–20 kHz region (manual axis-caveat zone) is **reported separately,
   never gated**.

### Python-port verification (no build coupling)

The RBJ / HighCut math in `ir_check.py` is transcribed verbatim from
`src/dsp/ToneBank.h` / `src/dsp/HighCut.h`. Proof of equivalence: the port
reproduces all six published C++ either-oracle worst deltas (Task 4 report,
48 kHz) exactly — T1 0.47 / T2 0.75 / T3 0.27 / T4 0.48 / T5 0.11 /
T6 0.30 dB (all ≤ 0.01 dB) — far inside the required 0.05 dB.

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

## 3. IR leg — BLOCKED-ON-FILES (user steps)

1. Download the Avalon U5 set (14 files, TONE0–6 × HIGHCUT on/off) from
   https://www.tone3000.com/tones/avalon_u5-36172 with your Tone3000
   account (T3K license — NEVER commit the WAVs).
2. Drop the WAVs in `analysis/ir_local/` (gitignored). Accepted names
   (case-insensitive), e.g. `tone3_highcut_off_48k.wav`,
   `TONE2_HIGHCUT_ON_44k1.wav`, `tone0.wav` (must contain tone 0–6 and,
   except bypass-only files, `on`/`off` for highcut; unparseable names are
   listed as SKIP, never guessed).
3. Run `python analysis/ir_check.py`. Per-tone max/mean deltas vs ours and
   vs the chart print to console with PASS/FAIL vs the ±1 dB gate.
4. Record the numbers in §5 below. Do NOT tune DSP in this step — this task
   measures; re-tune is a follow-up decision for the user.

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

## 5. Results table (fill in when files exist)

| Tone | Highcut | vs-ours max/mean | vs-chart max/mean | Verdict |
|------|---------|------------------|-------------------|---------|
| — | — | pending | pending | BLOCKED-ON-FILES |

## 6. Highcut on/off check (same run, no extra work)

Each tone's on/off IR pair isolates the highcut stage: the script compares
each file against chain-with/without-highcut respectively, so a highcut
modeling error shows as the on-leg failing while the off-leg passes.
Additionally, (on_IR − off_IR) per tone should equal the 8 kHz one-pole
curve (−3.0 dB @ 8 kHz, ≈ −1.0 dB @ 4 kHz, ≈ −5.8 dB @ 15 kHz at 48 kHz);
eyeball this
difference when reviewing output.

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
