# rsasgtr LTSpice sim values (GroupDIY "Avalon U5 Freq/Phase Curves", schematic.png)

Sim source V1 AC 0.316 through R14 1k. Values in parentheses are rsasgtr's
tolerance tweaks to match amplexus's Keysight EDU-X measurements
(Jan 2023, 10 Hz-100 kHz); untweaked = as-traced.

CORRECTION to thread lore: MDP1603104G is 100k x8, NOT 10k ("104" = 10e4).
Phil Smith's "10 kOhm" read is wrong by 10x; the sim uses 100k sections
and matches measured. Our earlier 10k-based corner math is superseded by
the table below.

## TONE 1
C4 180p (top bypass); RN1 sections 100k x3 (nets V2/V4/V5);
C10 22n; C11 22n; R6 1M (says 1Meg); R7 27.32k; R13 1Meg to GND.

## TONE 2
C3 470p (top bypass); R4 56k; C9 22n; RN16 100k;
C8 22n shunt; R11 1Meg shunt.

## TONE 3
C2 180p; R5 39k2; R3 10k; C6 {22n+28n} = 50n (was 22n on PCB: 47n+6%);
C13 220n; R8 50k (resolves R8 49k9-vs-49R9: k wins); R2 39k1; R9 10k.

## TONE 4
C1 180p (top bypass); R1 26k4; C5 22n; RN11 100k;
C12 820p shunt; R10 1Meg shunt.

## TONE 5
C7 {10n*.9} = 9n series; RN13 {100k*.95} = 95k shunt to GND.
9n x 95k = 186 Hz highpass: -3 dB @186, ~-25 dB @10 Hz. MATCHES measured.

## TONE 6
R12 9k9 series; C14 10n series; RN14 100k; RN15 {100k*.95} = 95k;
C15 180p output shunt to GND.

## Measured-vs-sim agreement (amplexus Keysight + rsasgtr overlay, eye-read)
- T1: tracks; marker -18.62 dB / 69.21 deg @10 Hz.
- T2: notch ~-35 dB @~700 Hz both; marker -18.58 dB @10 Hz.
- T3: tracks; marker -19.16 dB @10 Hz.
- T4: dip ~-18 dB @~5 kHz both; marker -18.26 dB @10.5 Hz.
- T5: tracks; marker -35.98 dB @10 Hz.
- T6: tracks; marker -37.36 dB @10 Hz.
Absolute (unnormalized) markers preserved above: sim and measurement share
them, so level reference is absolute here, not 1 kHz-normalized.
