# Analysis index

Receipts. Every number the plugin claims is earned somewhere under here —
this file tells you where, so you can re-earn it yourself.

Scripts are Python 3 + numpy; plots add matplotlib. A few older fit
scripts also pull in scipy/PIL (mid-migration, not a rule). The `.nam`
captures and IR WAVs live on your machine, never in git (`*.nam` and
`*.wav` are ignored) — except `--self-test-only` and `--verify-port`,
which need no files at all.

- `ir_check.py` — the workhorse. Feeds sweep IRs, the manual chart, and
  the Python copy of the DSP through the same comparisons, and checks the
  copy against the real C++ while it's at it.
- `IR_VALIDATION.md` — the full IR story: methods, gates, rulings, and
  what's still open. Dated; read the top note first.
- `LEVELS.md` — the level bible: 0 dBFS = +24 dBu, THD-vs-boost maps per
  version, and an honesty section about the manual's spec.
- `u5_tone_*.csv/json`, `tone_targets.h` — the oracles. Digitized charts,
  blue-channel extractions, and the 60 hand-read header points the tests
  check against.
- `circuit_trace/` — teardown notes, component fits, and
  `plot_tone_curves.py`, which redraws the public tone plots. Same commit
  as any shape change, always.
- `fit_scripts/digitize.py` — the little helper that turns chart pixels
  into CSV rows.
- `nam/` — neural-capture research (own README inside). Drive fits, tilt
  fits, probes, plots. Captures live in your `Downloads/`.
- `ir_local/` — your local WAVs and `.nam` copies. Ignored, test-only.
