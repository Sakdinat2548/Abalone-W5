# SW1 switch map — seen vs inferred

Source photos: `docs/refs/circuits/u5_back.jpg` (main board 9520 solder side),
`docs/refs/circuits/u5_front.jpg` (main board 9520 component side),
`docs/refs/circuits/u5_boost.jpg` (9522 wafer board behind the BOOST knob),
`docs/refs/circuits/u5_filter.jpg` (9519 filter board behind the TONE knob),
`docs/refs/circuits/u5_back_front.jpg` (50/50 overlay, alignment ref only —
front-dominated, proves common framing not trace correspondence).
Front panel reference: `U5_Silver_Front_On_reflection2.png` (local AbaloneW5SAUCE folder, machine-local).

Orientation: front top edge = chassis rear (wire bundles, mains switch,
upright transformer label, ASSY 5600-9520 REV silkscreen); bottom edge =
front-panel side (pot shafts, push-switch bodies). Back presented rear-up
too, so back-image-left = front-image-right (mirror rule) — but no
fiducial pair is matchable at 640px, so NO front↔back continuity is
trusted until 2–3 fiducial pairs are matched mirror-aware.

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

- **H1 (contested): switch board = TONE selector.** Each of the 6 TONE
  positions routes the signal through a different subset of the filter-board
  RC via the bottom-edge wires; the 9 resistors are per-position
  series/shunt divider arms shared with the filter network. Consistent
  with: passive-only tone bank, caps concentrated on the filter board.
  Against: the wafer board sits physically behind the BOOST knob (placement
  evidence supersedes the old filename-based survey), and the `12` mark is
  a 12-way terminal number, neutral between hypotheses. Demoted from
  favored on placement grounds.
- **H2 (favored): switch board = BOOST ladder.** 9 resistors = 9 x 3dB
  steps for 10 boost positions in a series string, on a 12-way wafer behind
  the BOOST knob — placement + count fit exactly. The old "pad ring reads
  12 = 2-pole x 6" support for H1 is struck (terminal number, not a pole
  count). Remaining objection cuts both ways: under H1 the 10-detent BOOST
  selector is equally invisible — only ONE wafer exists for TWO detented
  knobs, so one detent mechanism is photographically unaccounted-for under
  EITHER hypothesis (new unknown, not disproof of either).
- **DIP package**: RN1 (resistor network, part of the divider ladder) vs SW1
  (DIP switch array selecting filter legs). Unresolved from photos (weak
  RN1 lean: printed top face, no visible sliders — sliders can hide at
  640px, so still unresolved; needs marking macro).

## Per-tone RC engagement table

| Tone | Engaged RC | Status |
|------|-----------|--------|
| 1-6  | unknown   | BLOCKED — pad-to-trace routing is not resolvable at 500-640px; traces under solder mask + the 9522 wafer underside unphotographed (`u5_back.jpg` is the main-board solder side, not the wafer back — do not misread it as wafer coverage) |

No per-tone RC combo can be honestly listed. The "engaged network" per tone
is instead characterized functionally by `derive.py` (required shelf corners
+ gains + notch per tone, i.e. required RC products any candidate network
must produce).

## What would disambiguate (ranked)

1. Continuity beep-test on a real unit: wiper-to-R per TONE detent (maps H1/H2 + per-tone arms in minutes).
2. Hi-res (>=1200dpi) crop of the 9522 switch-wafer UNDERSIDE traces (not satisfiable from `u5_back.jpg` — wrong board; needs new photography).
3. Hi-res macro of the DIP package marking (RN1 vs switch array).
4. Freestompboxes U5 thread (linked in AGENTS.md, not scraped): members may have traced this already — ask, don't scrape.
