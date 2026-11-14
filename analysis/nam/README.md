# NAM-based voicing research (tone/v1.2 line)

Throwaway-grade probes and plots behind the v1.2 color-drive + tilt work.
Nothing here ships in the plugin; the gated evidence lives in `tests/`.

## The runner

- `nam_numpy.py` — exact numpy inference for TONE3000-packed WaveNet `.nam`
  files (single LayerArray, ungated LeakyReLU, flat float weights; validated
  <1 dB rung-for-rung against Reaper renders of the same input, and +12.24 dB
  across Boost3/Boost7 captures = the hardware's 4×3 dB steps). Needs only
  numpy. Validated runner, not a product dependency.

## Fits (spike probes)

- `drive_fit.py` — grid-searches ColorStage (k, a) against capture harmonic
  ladders (H2/H3, floor-hinged, known boost staging). Result: k≈0.064,
  a≈1.5e-3 (adopted rounded: k=0.06, a=1.5e-3).
- `tilt_fit.py` — fits LS+HS tilt to mean capture-vs-chain FR delta.
- `tilt_ls.py` / `tilt_hs.py` / `tilt_mid.py` (+ `.txt` data, `tilt_lsplot.png`,
  `tilt_hsplot.png`, `tilt_mid.png`, `tilt_candidates.png`): LS retune, HS
  retune, extra low-mid peak probes; `tilt_median.png` / `tilt_now.png` /
  `tilt_shipped.png` show shipped chain vs median target.
- `tiltchain.cpp`, `c6_probe.cpp`: end-to-end chain sweeps (tilt verification,
  C6 prediction).
  Result (superseded): LS 120 Hz +0.7 dB + HS 8 kHz −0.5 dB (residual 0.10 dB).
  Shipped instead: LS 75 Hz +0.91 / HS 5441 Hz −0.34 / PK 180 Hz +0.11
  (engaged-vs-bypass median fit; see src/dsp/ProcessorChain.h).
- `measure_tone_gain.py`, `alias_probe.cpp` — broadband per-tone gains;
  folded-alias census.

## Probes + figures (C++ probes drive the REAL dsp/ headers; plots are matplotlib)

- `harm_*` (THD vs freq/level), `spec_*` (analyzer spectra),
  `dyn_*` (compressor transfer curves), `blind_*` (DC/alias/IMD),
  `os_*` (1x/2x/4x alias census), `namcmp_*` + `namcmp.csv` (captures vs
  chain FR), `namh_*` (harmonic overlays), `namdi_*` + `namdi_fr.csv`
  (DI capture), `namt2_*` (Tone2/B4 comparison — probe built, run parked),
  `plot_drivefit.py`/`drivefit.png` (fitted vs captures vs current),
  `plot_tilt*.py`/`tilt*.png` (tilt fit vs chain vs captures),
  `nulltest.py` (Reaper-render null validation).
- `harm.csv`, `dyn.csv` — probe data.

## ColorStage remap / refit line (Oct 2026 — open research, no DSP change yet)

Question: does the shipped ColorStage (k, a, gated odd/even bumps) close the
capture harmonic ladders, and is the gate earning its 2 params?

- `color_remap.py` — numpy-inference captures (B3/B7/DI/T2 from
  `analysis/ir_local/`) vs a float64 mirror of `src/dsp/ColorStage.h`; coherent
  H1..H8 per (freq, level). Writes `remap.csv` (196 rows, derived numbers only).
- `fit_v2.py` — refit of the current architecture on "responding" B3/DI rows
  (gated vs static). `fit_v3.py` — nested M0/M1/M2 + BIC, hinge-scored on all
  B3/DI rows so floor-selection bias is visible.
- `t2_calcheck.py` / `t2_frlevels.py` — is the T2 residual tilt explained by
  Tone->Color ordering, and is T2's linear FR level-dependent?
- Gateway (Plugalyzer-driven, `Slim:1.00` full submodel) — supersedes the
  numpy rows because renders are settled:
  `run_gw_sweeps.py` (grid -> `gw_sweeps_all.csv`), `fit_settled.py` (M0/M1/
  M1+per-capture-offset/M2 + BIC on that CSV), `fr_cal.py` (low-drive FR per
  capture: B3/B7/DI are within ±0.4 dB 55 Hz-5 kHz, so input FR does not tilt
  drive), `hysteresis_probe.py` + `hysteresis_ctrl.py` (rising vs falling
  history at matched level; ctrl validates the protocol on our own gate and
  uses an asymmetric triangle to separate slope artifacts from memory),
  `shootout.py` (waveform overlay: our VST3 vs DI capture).

Status: `gw_sweeps_all.csv` / `fit_settled_results.txt` lived in the
machine-local temp dir and were not kept — rerun `run_gw_sweeps.py` then
`fit_settled.py` to regenerate. No model change is adopted from this line yet;
the shipped constants are unchanged.

Gateway scripts need Plugalyzer at `~/Code/Plugalyzer/build/...`, Gateway.vst3
installed, and `U5-*.vstpreset` files under
`~/Documents/VST3 Presets/Atkinson Advanced Modeling, LLC/Gateway/` (paths
resolve from `~`; outputs go to `~/AppData/Local/Temp/opencode/`).

## Captures (NEVER commit — licensed, machine-local)

`C:/Users/<you>/Downloads/Avalon U5 *.nam` (Boost3/Boost7 MicOut No EQ,
TONE 2 Boost4, DI Preamplifier) + Reaper renders under
`C:/Users/<you>/Documents/REAPER Media/nam_*.wav`. Scripts hardcode these
paths; adapt to your machine (`<you>` = your Windows username — no real
usernames are stored in this repo). No IR WAVs either (T3K license).

## Run

MSYS `python3` (numpy + matplotlib only — no torch anywhere):
`python3 analysis/nam/<script>.py` from the repo root. C++ probes compile
with `C:\msys64\ucrt64\bin\g++.exe -O2` (headers are JUCE-free).
