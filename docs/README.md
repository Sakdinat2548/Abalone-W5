# Abalone W5 internals

Seven chapters, written the way the work actually happened: measure
something, do the math, then write the code. If you're learning from
this repo, read them in order — each one assumes the last.

- `01-signal-chain.md` — the full path and what each stage is for.
- `02-tones.md` — measuring six tone shapes and fitting them.
- `03-color.md` — saturation, harmonic laws, and the gated branch.
- `04-voicing-highcut.md` — the engaged fingerprint and the noise filter.
- `05-levels-staging.md` — staging math and why the gains are what they are.
- `06-validation.md` — what the gates prove, and what they don't.
- `07-releases.md` — branches, nightlies, versions, KVR.

Pictures sit alongside (`tone-curves.png`, `dynamics.png`,
`signal-flow.png`, `ui-preview.png`).
