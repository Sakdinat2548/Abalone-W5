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
