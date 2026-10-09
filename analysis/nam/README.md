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
  Result: LS 120 Hz +0.7 dB + HS 8 kHz −0.5 dB (residual 0.10 dB).
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
