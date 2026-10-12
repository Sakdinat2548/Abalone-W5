# 05 — Levels and staging

## The one anchor

**0 dBFS = +24 dBu.** The hardware's maximum input, and the only level
anyone states without hedging. Everything hangs off it:

- +4 dBu nominal = −20 dBFS.
- Boost step N adds 3N dB (+3 at step 1 … +30 at step 10).
- Stage level: `dBu = input_dBFS + 24 + 3N`.

## Gain staging in practice

Raw passive pickups land around −35…−25 dBFS RMS — down where the color
stage is basically asleep. That's why the box carries +30 dB of boost:
quiet instruments need carrying, and Boost *is* the drive knob.

- Sensible tracking (−20 RMS) at Boost 1 runs the gated pins
  (`thdAt(1000,−17)` ≈ 0.028%): clean with an even-order sheen.
- Same signal at Boost 6 lands mid-ladder (~0.2% region): pushed clean.
- Boost 10 limits with flavor — peaks shaved up to −5 dB, THD toward
  the 0.5% ceiling.

## Tone gains: staging, not tone

Every shape carries a fixed gain picked so pink noise measures the same
in and out (throwaway-verified ±0.01 dB, not gated). Hopping shapes
shouldn't jump loudness — on pink-like sources, it doesn't.

The honest footnote: pink is one guess at "typical." Feed it something
mid-heavy and the scoops go quiet anyway (T2 −4.4 vs T6 +2.6 — measured,
saved in `analysis/nam/tone_gain_spread.csv`). No fixed number survives
every source, which is why Trim (−32…+6) stays manual, deliberately.
Auto gain-comp would fight the boost-as-drive design; it sits parked as
a v1.4 opt-in proposal — proposal, not plan.

## The manual's number, and why we don't chase it

"0.05–0.1% THD at +10 dB" — no input level, no boost step, no frequency,
no load, no output. A ratio with the endpoints missing is not a spec,
it's marketing with numbers. We fit captures instead (0.47% at a fully
defined +10 dB stage point). Amusing footnote: if "+10 dB" ever meant
+10 dBu *at the output*, the stage behind it sits far lower and our
numbers land near it — one more reason the figure can't adjudicate
anything. Full maps and the long version: `analysis/LEVELS.md`.
