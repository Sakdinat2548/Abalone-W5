# U5 tone-board circuit trace — method, verdicts, unknowns

## Method

1. Inventoried sources: `docs/refs/circuits/*.jpg` (640x405 max) + SAUCE dir
   (same photos + `hr_avalonLit_U5.pdf` manual + front-panel PNG). No hi-res
   PCB shots exist anywhere in-scope.
2. Working crops under `crops/` (PIL 6x Lanczos + autocontrast + unsharp;
   each <250KB): per-resistor bands, poly-cap markings, RN1/SW1 package.
3. Transcribed `values.csv` (42 rows: 5 clear / 10 uncertain / 27 unreadable).
   Clear = refdes, package, populated/DNP, silkscreen, wire presence.
   No resistor or film-cap VALUE is clear — hues and print are ambiguous at
   640px, so none feed the derivation as fixed values (per brief discipline).
4. `derive.py` (numpy-only math; matplotlib only for PNGs): passive
   shelf/notch blocks from first principles (divider transfer functions in
   comments), coordinate-descent fit per tone over 40Hz-15kHz vs CSV, with
   overall gain solved analytically. Unknowns = the fitted params themselves
   (corner taus + divider ratios), with sensitivity ranking.

## Agreement report (theory vs oracles, 40Hz-15kHz)

| Tone | Theory-vs-CSV | DSP-vs-CSV | IR-vs-CSV | Verdict |
|------|---------------|------------|-----------|---------|
| 1 | 0.84/0.54 | 0.08/0.03 | 1.83/0.72 | AGREE (IR diverges >9kHz only) |
| 2 | 1.08/0.60 worst@1.2kHz | 0.18/0.05 | 1.90/1.15 worst@notch | DIVERGE-marginal (theory; IR notch-tip is smoothing artifact) |
| 3 | 0.39/0.25 | 0.09/0.03 | 1.16/0.61 | AGREE |
| 4 | 0.59/0.24 | 0.10/0.03 | 2.27/0.75 worst@15kHz edge | AGREE (IR edge = capture noise) |
| 5 | 0.59/0.27 | 0.11/0.04 | 2.12/1.02 worst@<80Hz | AGREE (IR LF = capture noise floor) |
| 6 | 0.84/0.41 | 0.13/0.05 | 3.39/1.95 worst@<80Hz | AGREE (same LF caveat) |

(max/RMS dB. DSP leg reproduces IR_VALIDATION.md: all ≤0.2dB max.)

Required RC products (any candidate network must realize these):
- T1/T3: wide shallow scoop (notch f0 0.8/1.1kHz, Q 0.15 bound, 8/5dB) + flat ends.
- T2: sharp notch f0 ~680Hz, Q ~0.2?? — fitted depth 20dB; the 1.08dB residual sits
  on the notch shoulders (~1.2kHz): a single bridged-T section is almost but not
  quite the real network there.
- T4: dip f0 ~6kHz Q 0.54 depth 6.5dB + HF rise to +2dB.
- T5/T6: TWO cascaded low shelves (~70Hz/-7dB + ~65-75Hz/-10dB, +2.8/+3.2dB
  make-up) REQUIRED by fit divergence — single-shelf fit diverged 1.5-6dB,
  so two cascaded shelves are needed to match the curve; actual circuit
  topology unconfirmed. T6 adds HF cut fp ~10.5kHz, -4.8dB.
- Below 40Hz (not gated): T5/T6 CSV plunges to -22dB @10Hz, deeper than two
  shelves render — measured curve falls faster sub-40Hz than the fitted
  shape; steeper-cut topology unconfirmed.

## Unknowns + sensitivity (shopping list, ranked)

Sensitivity (param +10%/+2dB -> max curve move): overall gain and notch
depth/f0 dominate (2dB each); shelf corners move 0.3-1.8dB; T2 notch f0 is
the sharpest (3dB). So the highest-value evidence is anything pinning the
T2 notch arms and the T5/T6 low-skirt RC.

1. All resistor values (both boards) — needs >=1200dpi band macros.
2. All red-box film cap values — needs top-face macros.
3. Poly-cap texts C1/C3/C4/C12 (fragments suggest 1800pF?/470pF?/3nF?/870pF? families).
4. RN1/SW1 DIP marking (network vs switch array) + switch-wafer backside traces
   (settles H1/H2 in switch_map.md; a continuity beep-test on a real unit is faster).
5. Sub-40Hz behavior: CSV vs IR disagree with 2-shelf theory — a borrowed-unit
   LF sweep (10-40Hz, T5) separates digitization floor from a genuinely
   steeper LF cut (topology unconfirmed).

## Verdict for follow-up

Conditional-GO for re-fit/validate: 5/6 tones agree ≤1dB with minimal passive
topologies, and the required-RC table above constrains the real network. T2's
1.08dB shoulder residual + zero clear transcribed values mean a DSP change is
NOT justified yet — recommend evidence-gathering first (items 1-4), then a
re-fit task mapping confirmed values onto these required products.

## Nodal adjudication (rsasgtr LTSpice netlists — DSP STAYS, all tones)

`nodal_tone.py` / `nodal_compare.py` (numpy nodal AC of the traced tone
netlists: source via R14 1k, all six cells coupled, open outputs) vs CSV
oracle vs shipped biquads, 1 kHz-normalized (netlists + values:
`tone_board_transcription.md`, `sim_values_groupdiy.md`):

| Tone | nodal-CSV max/RMS | nodal-biq max/RMS | biq-CSV max/RMS | Verdict |
|------|-------------------|--------------------|-----------------|---------|
| 1 | 1.58/0.72 | 1.54/0.73 | 0.17/0.06 | DSP stays |
| 2 | 2.23/1.36 | 2.19/1.34 | 0.14/0.05 | DSP stays |
| 3 | 1.62/0.74 | 1.57/0.71 | 0.16/0.05 | DSP stays |
| 4 | 3.74/0.99 | 3.59/0.97 | 0.21/0.07 | DSP stays |
| 5 | 2.43/1.17 | 2.44/1.18 | 0.09/0.03 | DSP stays |
| 6 | 4.34/2.40 | 4.28/2.37 | 0.16/0.08 | DSP stays |

Rule applied: move a section only where nodal agrees with one oracle
against the other. That set is EMPTY — nodal-CSV ≈ nodal-biq everywhere
because biq-CSV ≤ 0.27 dB on all tones. The 1–4 dB nodal gaps are
traced-values-vs-printed-chart disagreements (plus ~0.4 dB cross-loading
and ~0.3–0.8 dB probe-loading modeling uncertainty, quantified in-script),
not fit errors. Promoting the sim over the chart would be a re-spec
decision; the per-frequency deltas for it are in the script output, not
acted on.

## v2 axis-corrected oracle (printed-tick distortion removed — wash, no retune)

`redigit_tone.py` replicates Claude's trace extraction (v1 replica matches
the repo CSV to 0.007 dB except the hand-fixed T2 tip) and re-maps x with
true-log positions from the 10/100/1k/10k ticks, ignoring the drawn 20 kHz
tick (documented ~2x too wide in claudeoutput.txt). Output
`u5_tone_curves_digitized_v2.csv` is a RESEARCH oracle: v1 stays binding,
no gate points at v2. Top-octave v1→v2 shifts: T1 −0.9/−0.5, T4 −1.5/−1.6,
T6 +1.1/+3.1 dB @15k/20k (T2/T3/T5 ≤0.4). Validation vs nodal is a wash:
v2 closes ~40% of the T4 nodal gap but moves away on T6/T1, inside nodal's
own ±0.5–1 dB modeling uncertainty — so v2 does NOT promote over v1 and no
biquad moves on it. Decisive top-octave data would be digitized GroupDIY
Keysight photos (bot-walled for fetchers; needs a logged-in browser).

## Constrained hardware refinement (PROVEN: no joint position — DSP stays)

`fit_hardware.py`: same shipped types/counts, bounded moves (f0 ±40%,
gain ±4 dB) toward nodal ONLY in the 27 IR-confirmed win zones, CSV kept
elsewhere. Outcome per tone (win-nodal max / CSV-damage gate max):

| Tone | Win-nodal | CSV damage | Notes |
|------|-----------|------------|-------|
| 1 | 0.91 | 0.92 | no better than shipped either way |
| 2 | 7.17 DIVERGED | 6.83 | optimizer unstable; disregard except as no-solution proof |
| 3 | 0.80 | 0.92 | marginal both sides |
| 4 | 1.52 | 2.55 | worst of both |
| 5 | 0.39 | 2.41 | win small, damage fatal |
| 6 | 0.68 | 4.73 | win small, damage fatal |

Poles 0.9996-0.9998 (thinner than shipped), T1 rate 0.174 over budget.
Splitting the 1–4 dB chart-vs-hardware difference gets the worst of both —
a move needs a ruling to ABANDON chart agreement in that zone (new gates),
not a compromise fit. Unconstrained run (fit to nodal everywhere) also
recorded: pathological params (gains ±10 dB, bound-riding Q, T2 tip
destroyed) — chasing nodal modeling noise, rejected.

## Zone-targeted refits, user-ruled zones (FINAL: DSP stays on all 6)

v1 (10 Hz-floored zones) appeared to win T5/T6 low ends — then inspection
showed the wins came from sub-40 Hz features (T5 +5.3 dB micro-peak at
9.7 Hz) chasing nodal's least-trustworthy octave (open-output assumption,
Ruling-B band). v2 floored all zones at 40 Hz and widened T4 dip travel
to x[0.5,2.0]:

| Tone | Zone target | v2 zone err | v2 CSV damage | Outcome |
|------|-------------|-------------|---------------|---------|
| T4 dip→8.1k | 3-12k nodal | 1.52 | 1.52 | dip peak stuck at 5.6k (needs role change, not numbers); LS hit Q floor |
| T5 LF | 40-300 nodal | 1.09 | 1.09 | worse than v1; 40-63 hardware gap structurally unrenderable jointly with 100-300 |
| T6 LF+HF | 40-300, 3-16k nodal | 2.10 | 2.10 | worse than v1; micro-peak pathology persists at the 40 Hz edge |
| T1 HF / T2 HF | ruled zones | worse/diverged | — | rejected earlier, shipped stands |

With honest ≥40 Hz zones, no same-budget position beats shipped anywhere
without ≥1 dB CSV damage. The remaining gaps are STRUCTURAL (T5/T6
sub-40 steepness, T4 dip role, T2 shoulder section, T1/T6 top-octave
chart-vs-hardware split) — a numbers-only refit cannot close them.
Hardware-truth promotion now requires topology decisions (+sections or
re-roled stages) plus new gates, i.e. a full re-spec task, not a fit.

## Closing entry: partial reversal adopted for v1.1.0

The v2 verdict above stood for one day, then user rulings 2026-10-04/05
reversed it IN PART: rather than binding everything to the digitized chart,
T1/T4/T5/T6 ship hardware-fit RBJ coefficients + per-tone gains, gated
against the blue hardware oracle inside ruled zones and against the chart
outside them (`tests/ToneBankTest.cpp`: `inRuledZone`, `checkBlueZones`;
recorded deviations under Ruling 21/22, Task-24, Fix-2 precedent). T2/T3
stay v1, and the full topology re-spec stays parked. The alternative
per-rate-SOS path (per-rate coefficient tables + generator) was built,
evaluated, and NOT adopted — its files were removed before merge, so no
generator output is missing. Regime documented in
`analysis/IR_VALIDATION.md` §9.
