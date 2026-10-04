# SW1 switch map — seen vs inferred

Source photos: `docs/refs/circuits/u5_boost.jpg` (switch board, 500x552),
`docs/refs/circuits/u5_filter.jpg` (filter board, 640x405).
Front panel reference: `U5_Silver_Front_On_reflection2.png` (local AbaloneU55SAUCE folder, machine-local).

## SEEN (photo evidence, no guessing)

1. The ONLY rotary wafer in either close-up is on the small switch board
   (`u5_boost.jpg`): a ring of ~10-12 solder pads in a circle, silkscreen
   `SW1`, refdes R1-R9 arranged around it (R1/R2/R3 top-right, R4/R5/R6
   top-left, R7/R8/R9 mid-right). One terminal area marked `12`.
2. The filter board has NO rotary: its `SW1` silkscreen sits next to a black
   DIP-16-like package that shares space with `RN1` silkscreen (one package,
   two candidate functions — see values.csv).
3. Filter board bottom edge: green + red + yellow wires leave the board toward
   a mating connector (destinations not visible).
4. Switch board bottom: twisted green/yellow pair leaves toward the lower
   chassis (destinations not visible).
5. Front panel: TONE knob has 6 detents (1-6); bypass is a separate TONE ON
   push-switch. BOOST knob has 10 detents (1-10).
6. Filter board C15 footprint is EMPTY (DNP) with a nearby wire jumper.

## INFERRED (hypotheses, ranked)

- **H1 (favored): switch board = TONE selector.** Prior photo survey in the
  project spec agrees ("filename says boost but photo is tone switch").
  Each of the 6 TONE positions routes the signal through a different subset
  of the filter-board RC via the bottom-edge wires; the 9 resistors are
  per-position series/shunt divider arms shared with the filter network.
  Consistent with: 6 detents, passive-only tone bank, caps concentrated on
  the filter board.
- **H2 (disfavored): switch board = BOOST ladder.** 9 resistors = 9 x 3dB
  steps for 10 boost positions in a series string. Disfavored because: the
  filter board (all the caps) would then have no visible selector, and the
  pad ring reads closer to 12 (2-pole x 6) than 10+common. Not ruled out.
- **DIP package**: RN1 (resistor network, part of the divider ladder) vs SW1
  (DIP switch array selecting filter legs). Unresolved from photos.

## Per-tone RC engagement table

| Tone | Engaged RC | Status |
|------|-----------|--------|
| 1-6  | unknown   | BLOCKED — pad-to-trace routing is not resolvable at 500-640px; traces under solder mask + backside not photographed for these boards |

No per-tone RC combo can be honestly listed. The "engaged network" per tone
is instead characterized functionally by `derive.py` (required shelf corners
+ gains + notch per tone, i.e. required RC products any candidate network
must produce).

## What would disambiguate (ranked)

1. Continuity beep-test on a real unit: wiper-to-R per TONE detent (maps H1/H2 + per-tone arms in minutes).
2. Hi-res (>=1200dpi) crop of the switch-wafer backside traces.
3. Hi-res macro of the DIP package marking (RN1 vs switch array).
4. Freestompboxes U5 thread (linked in AGENTS.md, not scraped): members may have traced this already — ask, don't scrape.
