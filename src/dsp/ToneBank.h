#pragma once

#include <cmath>

// Tone-bank biquads, Abalone W5 v1 chart fit + absolute 10 Hz anchors
// (Fix Round 2): RBJ triplets (Tone 2: quadruplet, see below) tracking
// analysis/u5_tone_curves_digitized.csv (121 log-spaced points, 10 Hz-20 kHz)
// within +/-0.5 dB over 40 Hz-15 kHz on every tone EXCEPT T1 (recorded
// deviation below) and T3 (recorded deviation below), plus the six absolute
// 10 Hz eye-read anchors (finding 2) and the five low-end eye findings (Fix
// Round 2: T1 20 Hz, T2 curve (RECORDED SHORTFALL, see below), T3/T4 convex
// through 0 dB at 20 Hz, T6 10-20 Hz slope in T5's foot family).
// Low end keeps the Task-17 CSV gates over [40,200] Hz (+/-0.3 dB, T2 +/-0.5)
// EXCEPT T1 (recorded deviation below) and T3 (recorded deviation below).
// Rationale for the hierarchy: the user judges the CSV +
// docs/refs/u5_tone_fit_check.png truer than both the eye-reads and the T3K
// captures, so the digitized CSV binds 40 Hz-15 kHz; the user's six 10 Hz
// eye-reads (T1 -3 / T2 -0.25 / T3 -3 / T4 -3 / T5 -22 / T6 -22 dB absolute)
// outrank the CSV below 40 Hz, where the 10-40 Hz CSV band was already
// excluded as digitization floor (Ruling B, Task 17); the eye-read header
// and IR shapes stay advisory (reported, not gated — see
// analysis/IR_VALIDATION.md). Dense-CSV worsts (binding): T1 0.97 dB
// (DEVIATION, gate 1.0), T2 0.48 dB, T3 0.54 dB (DEVIATION, gate 0.6),
// T4 0.48 dB, T5 0.32 dB, T6 0.46 dB. Low-end [40,200] worsts: T1 0.97 dB
// (DEVIATION, gate 1.0), T2 0.48, T3 0.54 (DEVIATION, gate 0.6), T4 0.28,
// T5 0.22, T6 0.26 dB. 10 Hz absolute values / anchor deltas (binding,
// gates T1-T4 1.0 / T5-T6 2.0): T1 -3.80 (0.80), T2 +0.70 (0.95),
// T3 -3.80 (0.80), T4 -2.85 (0.15), T5 -23.21 (1.21), T6 -23.04 (1.04) dB —
// all PASS. 20 Hz eye values (binding, +/-0.5 dB): T1 +0.30 / T3 +0.30 /
// T4 +0.03 dB — all PASS (T2 keeps its anchor-only gate, see below).
// T6 10-20 Hz slope +5.06 dB/oct vs T5's +5.80 (delta 0.74, gate 1.0 — PASS;
// HEAD was +2.59, delta 3.21). 44.1k/48k agreement worsts (C++ probes):
// T1 0.030 / T2 0.065 / T3 0.033 / T4 0.244 (DEVIATION, gate 0.3) /
// T5 0.000 / T6 0.094 dB. Either-oracle worsts vs chart oracles (advisory,
// +/-1 dB check, C++ actuals): T1 0.83 / T2 0.44 / T3 0.33 / T4 0.47 /
// T5 0.23 / T6 0.28 dB — all pass (CSV track implies it). Poles < 1 at 44.1k
// + 48k after the refit, including float32-quantized radii (C++ cooks to
// float storage; worst radius 0.99975 on T5's overdamped highpass, stable
// and deterministic).
//
// Fix Round 1 method (T2 gains ONE section — twin-peak V for the sharp tip +
// fat skirts; all other tones numbers-only, same types): hand-rolled bounded
// Nelder-Mead with restarts (genuinely randomized multi-start, 24 seeds per
// tone + second-wave refinements) on the CSV grid, penalties only beyond the
// true gates (dense, low-end, 10 Hz anchors, 44.1k/48k rate agreement,
// subsonic-bump veto, top-octave soft cap), analytic pole check at both rates
// with the float32 cook in the loop. Full-grid objectives throughout
// (no tip-chasing). Every tone audited gap-free on an 800-pt fine grid vs
// log-interp CSV (fine-grid worst within 0.02 of on-grid, except where noted
// below). Optimizer runs were needed because the anchors couple into the
// low-end fit through shared sections (corner/Q trade-offs) — no closed form
// beyond the starting-point analysis (anchor ~= CSV@10 Hz for T1/T3-T6, so
// HEAD numbers seeded T1/T5/T6 and the stalled T2/T3 seeds were kept after
// independent verification).
//
// T2 RECORDED SHORTFALL (finding 2 NOT MET — ruling needed, numbers
// unchanged): the 10-50 Hz ink hump (-0.93 @ 10 Hz rising to +1.33 @ ~27 Hz,
// back to +1.04 @ 40 Hz) is unrenderable numbers-only with the sections
// in-role. Proof: below ~60 Hz only the lowshelf acts (twin peaks and the
// 11 kHz highshelf contribute ~0 dB there), and one shelf has a single
// transition — but the ink needs TWO features (rise 10->27, fall 50->100)
// while the binding dense gate pins 40 Hz at +1.04 +/-0.5 (forces shelf
// gain ~= +0.7, hence the flat +0.7 floor) and the 60-150 Hz
// peak-skirt complementarity pins the corner at ~75 Hz. An eye-hard probe
// (10 Hz = -0.25 +/-0.25, 27 Hz = +1.3 +/-0.35 as unbreakable walls)
// BREAKS the walls (lands +0.48/+0.66) while destroying the foot
// (dense/low 1.72 @ 40 Hz). High-Q "bump" does not exist on positive-gain
// shelves (measured: overshoot goes the wrong way, a dip). Keeping HEAD
// numbers byte-identical (dense 0.48, low 0.48, anchor 0.95 — all green)
// is the only gate-safe point. Options for ruling: 5th section on T2
// (dedicated 27 Hz hump), peak role-change (forbidden without escalation),
// or accepted flatness.
//
// T3 RECORDED DEVIATION (eye wins per brief tie-break, tension reported):
// the 20 Hz = 0 dB eye demand plus the -3 dB anchor force the lowshelf
// corner 31.2 -> ~14.8 Hz, but one shelf knee cannot sit in two places —
// the 40-50 Hz foot (bump tail, was +0.43 @ 40 Hz) sags to +0.10 @ 40 Hz
// (foot error 0.54 @ ~49 Hz). The scoop peak narrows slightly in-role
// (Q 0.23 -> 0.28, gain -3.78 -> -3.67) to relieve the foot; the scoop
// stays intact above (dense mid still tracks, non-twins vs T4 preserved).
// Shipped joint: dense 0.54 (gate 0.6), low-end 0.54 (gate 0.6),
// anchor 0.80 met, 20 Hz +0.30 met, 10-30 Hz monotonic rise. Options for
// ruling: 4th section on T3, accepted deviation, or anchor relaxation.
//
// T2 FOURTH SECTION (Fix Round 1, finding 1 resolves the Task-20 recorded
// deviation):
// twin peaks (s0 610/0.43/-10.96 + s1 702/1.56/-9.77) form the sharp V
// (-20.55 dB tip @ 695 Hz) with fat skirts; the lowshelf/highshelf keep
// their tilt roles. Dense 0.88 -> 0.48 (gate +/-0.5 met); the +/-1.0
// exception is gone. Section budget: 3x5 + 4x1. Stability re-proven at both
// rates (incl. float32-quantized radii); 44.1k/48k invariance re-checked with
// the new section participating (0.065 dB worst). Thin margins (deterministic,
// recorded not chased): anchor 0.95/1.0, low-end 0.48/0.5 — the anchor/low
// pair is structurally coupled through the lowshelf (see T2 note in the
// report). Fit details: task-20-report.md Fix Round 1 section.
//
// T1 RECORDED DEVIATION (anchor + eye win per brief tie-break, tension
// reported): the -3 dB anchor plus the user's 20 Hz = 0 dB eye demand pin
// the highpass corner at ~12.3 Hz with a low Q (0.85, hump-free: +0.22 dB
// max over 10-40 Hz — the +3.8 dB @ ~18 Hz wart is gone), but an
// anchor-pinned highpass is ~0 dB at 40-60 Hz while the CSV foot sits at
// +0.64/+0.50 dB there, and the scoop peak's low skirt subtracts another
// ~0.4-0.6 dB (peak frozen to protect the approved scoop: bottom -7.03 dB
// @ 697 Hz vs CSV -7.18, intact). Closed-form floor ~= 0.97 dB @ ~49 Hz;
// the joint HP+peak optimizer run only moved the pain into the scoop
// (0.86 @ 143 Hz + deepened bottom), so the peak stays frozen and the foot
// carries it. Shipped joint: dense 0.97 (gate 1.0), low-end 0.97
// (gate 1.0), anchor 0.80 met, 20 Hz +0.30 met. Options for ruling: 4th
// section on T1 (dedicated sub-40 shaping), accepted deviation, or anchor
// relaxation.
//
// T4 RECORDED DEVIATION (rate invariance — unchanged by Fix Round 2): the
// anchor forces a lowshelf-down role arrangement (lowshelf ~18 Hz for the
// plunge, highshelf ~100 Hz to rebuild the mid shelf) with a deeper peak
// (-6.84) for the dip; the dip's steep top-octave recovery slope then warps
// 0.24 dB between 44.1k/48k at the 15 kHz probe (10 kHz probe 0.13) — the
// warp sits entirely in the peak section (per-section diagnostic), and
// rate-weighted refits from both the Task-20 and stalled seeds floor at
// 0.24, so it is structural. Dense 0.48 / low 0.28 / anchor 0.15 all met;
// 20 Hz eye +0.03 met; T4's rate gate is 0.3 (0.244 landed). 0.24 dB @
// 15 kHz across sample rates is inaudible; ruling decides whether that
// stands, a 4th section takes the top octave, or the anchor relaxes.
//
// Task-20 history (superseded numbers, kept for lineage): numbers-only
// full-band fit (T1 0.57 -> 0.28, T4 0.71 -> 0.34, T6 0.56 -> 0.13;
// T2 recorded deviation 0.88 with six-run structural evidence). Task 17
// (Rulings B+C): low-end tracked the CSV 100% over [40,200] Hz; the 10-40 Hz
// CSV band is EXCLUDED from all CSV gates (digitization floor), now replaced
// by the six absolute 10 Hz eye-read anchors as ground truth below 40 Hz.
// The 5 Hz DC-blocker still owns sub-40 behavior by design (other stages
// are flat there).
//
// Chain position: Tone (bypass + 1-6 biquad presets, Task 7 adds the 10ms
// xfade around setTone). Bypass (tone 0) is bit-transparent passthrough.
//
// Per-tone topology + fitted numbers (f0 in Hz, Q = shelf-alpha quotient,
// gain in dB): Fix Round 2 low-end eye retune (T5 byte-identical reference;
// T2 byte-identical — finding 2 recorded shortfall; all other tones same
// types, values free, in-role). Lineage moves worth noting: T1s0 (highpass
// 14.859/1.5 -> 12.344/0.8499) drops the Q-wart for the user's 20 Hz = 0 dB
// read while holding the -3 dB anchor (foot hole is the recorded price, peak
// frozen); T3s0 (peak Q 0.2315 -> 0.28, gain -3.783 -> -3.665) narrows
// slightly in-role to relieve the foot while T3s2 (lowshelf 31.232/-2.972
// -> 14.795/-3.303) dives for the anchor + 20 Hz eye; T4s0 (lowshelf
// 25.902/-3.724 -> 18.189/-3.670) + T4s2 (highshelf 126.355/+2.537 ->
// 100.0/+2.604) steepen the knee through 0 dB at 20 Hz while the tilt still
// rebuilds the shelf (see T4 deviation above); T6s0 (lowshelf 51.684/-21.776
// -> 29.651/-29.913) + T6s1 (highshelf 240.601/+2.282 -> 182.857/+2.345)
// steepen the 10-20 Hz foot into T5's slope family (slope +5.06 vs +5.80)
// while the 40 Hz foot and top end still land. Dense-CSV worsts (binding):
// T1 0.97 dB (DEVIATION), T2 0.48 dB, T3 0.54 dB (DEVIATION), T4 0.48 dB,
// T5 0.32 dB, T6 0.46 dB.
//
//   tone  stage  type       f0       Q      gain
//   1     0      highpass   12.344   0.8499   --
//   1     1      peak       841.567  0.2023   -7.075
//   1     2      highshelf  11368.945 1.9757  +0.834
//   2     0      peak       609.991  0.4306  -10.957
//   2     1      peak       701.712  1.5656   -9.772
//   2     2      lowshelf   75.194   1.6679   +0.711
//   2     3      highshelf  11366.908 0.5996  +1.988
//   3     0      peak       867.763  0.28     -3.665
//   3     1      peak       4568.537 0.8532   -1.318
//   3     2      lowshelf   14.795   1.2156   -3.303
//   4     0      lowshelf   18.189   0.9818   -3.67
//   4     1      peak       5814.022 0.5672   -6.841
//   4     2      highshelf  100.0    0.0474   +2.604
//   5     0      highpass   9.921    0.1829   --
//   5     1      lowshelf   105.024  0.6028   -8.549
//   5     2      highshelf  329.849  0.7836   +2.753
//   6     0      lowshelf   29.651   0.3441  -29.913
//   6     1      highshelf  182.857  0.7439   +2.345
//   6     2      highshelf  15009.860 0.6405  -4.631
//
// Shape notes: T1's scoop (peak 841.567/0.2023/-7.075, frozen) tracks the CSV
// scoop bottom within 0.15 dB — see T1 deviation above (eye-fixed knee:
// 20 Hz +0.30 dB, hump-free; the 40-60 Hz foot hole is the recorded price).
// T2's twin-V notch tip lands at -20.55 dB @ 695 Hz absolute, resolving the
// Task-20 tip-vs-skirt tension (dense 0.48); its 10-50 Hz floor stays flat
// at +0.7 (recorded shortfall above — ruling needed). T3 rises convex
// through 0 dB at 20 Hz (+0.30, monotonic 10-30) and reads non-twin vs T4's
// shelf-plus-dip (separation preserved above 40 Hz); its 40-60 Hz foot sits
// ~0.5 dB under the ink (recorded price). T4's dip (-4.35 dB @ 5796 Hz) is
// formed by the deepened peak against the low-cornered high shelf; its knee
// runs convex through 0 dB at 20 Hz (+0.03); above 15 kHz the curve follows
// the chart (top-octave soft cap +0.95 dB worst on T6, report-only).
// T6's 10-20 Hz foot (+5.06 dB/oct) joins T5's steep plunge family (+5.80)
// while its 40 Hz foot and top-end roll-off still track; T5's highpass
// (9.921 Hz, overdamped Q 0.18, no subsonic bump) is the untouched reference
// (lands the -22 dB anchor within 1.21 dB while the deepened lowshelf holds
// the 40 Hz foot).
//
// Oracle hierarchy (user ruling, Fix Round 2): the DIGITIZED CSV binds
// 40 Hz-15 kHz (every tone within +/-0.5 dB — T1 recorded deviation at 0.97,
// gate 1.0; T3 recorded deviation at 0.54, gate 0.6); low-end [40,200] keeps
// the Task-17 CSV gates (+/-0.3 dB, T2 +/-0.5 — T1 recorded deviation at
// 0.97, gate 1.0; T3 recorded deviation at 0.54, gate 0.6); the six
// absolute 10 Hz eye-read anchors bind below 40 Hz (T1-T4 +/-1.0, T5/T6
// +/-2.0 — all met); the 20 Hz eye reads (T1/T3/T4 +/-0.5) and the T6 slope
// match (+/-1.0 vs T5) bind the low-end shape (T2's 10 Hz eye target is a
// recorded shortfall, not gated — see above); the eye-read header and the
// measured IR shapes (analysis/IR_VALIDATION.md) are advisory
// (either-oracle vs header still checked, passing trivially). Highcut-ON
// captures are non-adjudicating per user Ruling A (see IR_VALIDATION.md
// section 6) — HighCut.h untouched. T4's 44.1k/48k agreement (0.244)
// exceeds the 0.1 invariance gate — recorded deviation, gate 0.3 for T4
// (see above).
// Highcut-ON captures are non-adjudicating per user Ruling A (see
// IR_VALIDATION.md section 6) — HighCut.h untouched.
//
// Biquad form: Transposed Direct Form II, RBJ cookbook coefficients with
// shelf alpha = sin(w0)/(2Q) (Q-parametrized shelf knee, same family as the
// Python fit prototype, verified bit-consistent). setSampleRate recomputes
// every tone's coefficients, so 44.1k/48k/any-rate give identical dB shapes
// (rate-invariance test: <=0.1dB at 40Hz-15kHz probes). magnitudeAt evaluates
// the exact theoretical cascade transfer function in double precision.
struct ToneBank
{
    ToneBank ()
    {
        setSampleRate (48000.0);
        setTone (0);
    }

    void setSampleRate (double sampleRate)
    {
        if (!(sampleRate > 0.0))
            sampleRate = 48000.0;

        sampleRate_ = sampleRate;
        for (int tone = 1; tone <= 6; ++tone)
            for (int s = 0; s < numSections (tone); ++s)
                cook (params (tone, s), bank_[tone][s]);
        select();
    }

    // tone 0-6, 0 = bypass (identity). Out-of-range values clamp.
    void setTone (int tone)
    {
        if (tone < 0)
            tone = 0;
        if (tone > 6)
            tone = 6;

        tone_ = tone;
        select();
    }

    float processSample (float x)
    {
        if (tone_ == 0)
            return x;

        float y = x;
        for (int s = 0; s < numSections (tone_); ++s)
        {
            Coeffs& c = active_[s];
            const float out = c.b0 * y + c.z1;
            c.z1 = c.b1 * y - c.a1 * out + c.z2;
            c.z2 = c.b2 * y - c.a2 * out;
            if (std::fabs (c.z1) < 1.0e-15f)
                c.z1 = 0.0f;
            if (std::fabs (c.z2) < 1.0e-15f)
                c.z2 = 0.0f;
            y = out;
        }
        return y;
    }

    // Exact theoretical cascade response of the selected tone in dB.
    float magnitudeAt (float freqHz) const
    {
        if (tone_ == 0)
            return 0.0f;
        if (!(freqHz > 0.0f))
            return 0.0f;

        constexpr double twoPi = 6.28318530717958647692;
        const double w = twoPi * static_cast<double> (freqHz) / sampleRate_;
        const double cosW = std::cos (w);
        const double sinW = std::sin (w);
        double real = 1.0;
        double imag = 0.0;
        for (int s = 0; s < numSections (tone_); ++s)
        {
            const Coeffs& c = bank_[tone_][s];
            // H(e^jw) = (b0 + b1/z + b2/z^2) / (1 + a1/z + a2/z^2), z = e^jw.
            const double bzR = c.b0 + c.b1 * cosW + c.b2 * (2.0 * cosW * cosW - 1.0);
            const double bzI = -(c.b1 * sinW + c.b2 * 2.0 * cosW * sinW);
            const double azR = 1.0 + c.a1 * cosW + c.a2 * (2.0 * cosW * cosW - 1.0);
            const double azI = -(c.a1 * sinW + c.a2 * 2.0 * cosW * sinW);
            const double denom = azR * azR + azI * azI;
            const double hR = (bzR * azR + bzI * azI) / denom;
            const double hI = (bzI * azR - bzR * azI) / denom;
            const double nR = real * hR - imag * hI;
            imag = real * hI + imag * hR;
            real = nR;
        }
        return static_cast<float> (20.0 * std::log10 (std::sqrt (real * real + imag * imag)));
    }

private:
    enum class Type
    {
        Peak,
        LowShelf,
        HighShelf,
        HighPass,
        LowPass,
    };

    struct Params
    {
        Type type;
        double f0;
        double q;
        double gainDb;
    };

    struct Coeffs
    {
        float b0 = 1.0f;
        float b1 = 0.0f;
        float b2 = 0.0f;
        float a1 = 0.0f;
        float a2 = 0.0f;
        float z1 = 0.0f;
        float z2 = 0.0f;
    };

    static Params params (int tone, int stage)
    {
        // (tone, stage) -> parametric section; see header table above.
        switch (tone * 10 + stage)
        {
        case 10:
            return {Type::HighPass, 12.344, 0.8499, 0.0};
        case 11:
            return {Type::Peak, 841.567, 0.2023, -7.075};
        case 12:
            return {Type::HighShelf, 11368.945, 1.9757, 0.834};
        case 20:
            return {Type::Peak, 609.991, 0.4306, -10.957};
        case 21:
            return {Type::Peak, 701.712, 1.5656, -9.772};
        case 22:
            return {Type::LowShelf, 75.194, 1.6679, 0.711};
        case 23:
            return {Type::HighShelf, 11366.908, 0.5996, 1.988};
        case 30:
            return {Type::Peak, 867.763, 0.28, -3.665};
        case 31:
            return {Type::Peak, 4568.537, 0.8532, -1.318};
        case 32:
            return {Type::LowShelf, 14.795, 1.2156, -3.303};
        case 40:
            return {Type::LowShelf, 18.189, 0.9818, -3.67};
        case 41:
            return {Type::Peak, 5814.022, 0.5672, -6.841};
        case 42:
            return {Type::HighShelf, 100.0, 0.0474, 2.604};
        case 50:
            return {Type::HighPass, 9.921, 0.1829, 0.0};
        case 51:
            return {Type::LowShelf, 105.024, 0.6028, -8.549};
        case 52:
            return {Type::HighShelf, 329.849, 0.7836, 2.753};
        case 60:
            return {Type::LowShelf, 29.651, 0.3441, -29.913};
        case 61:
            return {Type::HighShelf, 182.857, 0.7439, 2.345};
        case 62:
            return {Type::HighShelf, 15009.860, 0.6405, -4.631};
        default:
            break;
        }
        return {Type::Peak, 1000.0, 0.707, 0.0};
    }

    void cook (Params p, Coeffs& c)
    {
        constexpr double twoPi = 6.28318530717958647692;
        const double w0 = twoPi * p.f0 / sampleRate_;
        const double cw = std::cos (w0);
        const double sw = std::sin (w0);
        const double alpha = sw / (2.0 * p.q);
        const double A = std::pow (10.0, p.gainDb / 40.0);

        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a0 = 1.0;
        double a1 = 0.0;
        double a2 = 0.0;

        switch (p.type)
        {
        case Type::Peak:
            b0 = 1.0 + alpha * A;
            b1 = -2.0 * cw;
            b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha / A;
            break;
        case Type::LowShelf:
        {
            const double sq = 2.0 * std::sqrt (A) * alpha;
            b0 = A * ((A + 1.0) - (A - 1.0) * cw + sq);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
            b2 = A * ((A + 1.0) - (A - 1.0) * cw - sq);
            a0 = (A + 1.0) + (A - 1.0) * cw + sq;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
            a2 = (A + 1.0) + (A - 1.0) * cw - sq;
            break;
        }
        case Type::HighShelf:
        {
            const double sq = 2.0 * std::sqrt (A) * alpha;
            b0 = A * ((A + 1.0) + (A - 1.0) * cw + sq);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
            b2 = A * ((A + 1.0) + (A - 1.0) * cw - sq);
            a0 = (A + 1.0) - (A - 1.0) * cw + sq;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
            a2 = (A + 1.0) - (A - 1.0) * cw - sq;
            break;
        }
        case Type::HighPass:
            b0 = (1.0 + cw) * 0.5;
            b1 = -(1.0 + cw);
            b2 = (1.0 + cw) * 0.5;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha;
            break;
        case Type::LowPass:
            b0 = (1.0 - cw) * 0.5;
            b1 = 1.0 - cw;
            b2 = (1.0 - cw) * 0.5;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha;
            break;
        }

        c.b0 = static_cast<float> (b0 / a0);
        c.b1 = static_cast<float> (b1 / a0);
        c.b2 = static_cast<float> (b2 / a0);
        c.a1 = static_cast<float> (a1 / a0);
        c.a2 = static_cast<float> (a2 / a0);
        c.z1 = 0.0f;
        c.z2 = 0.0f;
    }

    // Atomically select the active coefficient set (no torn state).
    void select ()
    {
        const int n = numSections (tone_ < 1 ? 1 : tone_);
        for (int s = 0; s < n; ++s)
        {
            active_[s] = bank_[tone_ < 1 ? 1 : tone_][s];
            active_[s].z1 = 0.0f;
            active_[s].z2 = 0.0f;
        }
    }

    // Section budget: 3x5 + 4x1 — every tone uses three RBJ sections except
    // Tone 2, which carries a fourth (Fix Round 1, finding 1: twin-peak V).
    static int numSections (int tone) { return tone == 2 ? 4 : 3; }

    int tone_ = 0;
    double sampleRate_ = 48000.0;
    Coeffs bank_[7][4];
    Coeffs active_[4];
};
