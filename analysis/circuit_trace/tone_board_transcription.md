# Tone-board RC transcription (rsasgtr "Avalon U5 Tone Board", Tone.kicad_sch Rev 0.1, 2024-12-09)

Source: freestompboxes.org t=3273 attachment (2480x3507). Transcribed from
max-magnification crops; illegible chars marked UNCERTAIN, never guessed.
All RN1 sections are Dale MDP1603104G (10k x8 isolated array).

Common: every cell left = INPUT bus; per-tone output J1-J6 (pin 1) into an
"SP12T rotary switch used as SP6T"; J7 = OUTPUT; J17 = INPUT, J18 = OUTPUT,
J19 = GND.

## TONE 1 (J1)
RN1H (9-8), RN1B (15-2) from INPUT; RN1G (10-7) mid-to-output;
C4 180p INPUT-to-output; C10 22n; C11 22n; R6 1M; R7 26k4 to GND; R13 1M output to GND.

## TONE 2 (J2)
R4 56k2 series; C9 22n series; C3 470p top bypass INPUT-to-output;
C8 22n shunt to GND; R11 1M shunt to GND; RN1F (11-6, left digit blurry) to output.

## TONE 3 (J3)
100p series (designator UNCERTAIN, ~C7) + R5 39k2 to output;
10k series (designator UNCERTAIN, ~R3); R2 39k1 shunt;
C6 50n* bridging; R8 49?9 (k vs R UNCERTAIN) to R9 node;
C13 220n; R9 10k to GND.
Note: "*Based on simulations. The original value could also be 22nF."
(C6 carries the *; GroupDIY corroboration: C6 changed 22n -> 50n.)

## TONE 4 (J4)
R1 26k4 series; C5 22n series; C1 180p top bypass INPUT-to-output;
C12 820p shunt to GND; R10 1M shunt to GND; RN1A (14-1) to output.

## TONE 5 (J5)
10n series (designator UNCERTAIN) INPUT-to-output; RN1C shunt to GND
(pins UNCERTAIN).

## TONE 6 (J6)
R12 10k series; C14 10n series; RN1E shunt to GND (pins UNCERTAIN);
RN1D (13-4) series to output; C15 180p output shunt to GND.

## Blocking uncertainties (source-limited, need new photography or beep-test)
- T3 R8: 49k9 vs 49R9 (1000x apart — blocks T3 corner math).
- T5 RN1C pins: divider ratio unknown (blocks T5 level math).
- T6 RN1E pins: shunt value unknown.
- T3 100p/10k designators (values legible, refs cosmetic).
- T5 series cap designator (10n legible).
