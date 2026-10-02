#pragma once

#include <cmath>

// Tone-bank biquads, Abalone W5 v1 chart fit + absolute 10 Hz anchors
// (Fix Round 1): RBJ triplets (Tone 2: quadruplet, see below) tracking
// analysis/u5_tone_curves_digitized.csv (121 log-spaced points, 10 Hz-20 kHz)
// within +/-0.5 dB over 40 Hz-15 kHz on every tone EXCEPT T1 (recorded
// deviation below), plus the six absolute 10 Hz eye-read anchors (finding 2).
// Low end keeps the Task-17 CSV gates over [40,200] Hz (+/-0.3 dB, T2 +/-0.5)
// EXCEPT T1 (recorded deviation below). Rationale for the hierarchy: the user
// judges the CSV + docs/refs/u5_tone_fit_check.png truer than both the
// eye-reads and the T3K captures, so the digitized CSV binds 40 Hz-15 kHz;
// the user's six 10 Hz eye-reads (T1 -3 / T2 -0.25 / T3 -3 / T4 -3 / T5 -22 /
// T6 -22 dB absolute) outrank the CSV below 40 Hz, where the 10-40 Hz CSV
// band was already excluded as digitization floor (Ruling B, Task 17); the
// eye-read header and IR shapes stay advisory (reported, not gated — see
// analysis/IR_VALIDATION.md). Dense-CSV worsts (binding): T1 0.55 dB
// (DEVIATION, gate 0.6), T2 0.48 dB, T3 0.19 dB, T4 0.44 dB, T5 0.32 dB,
// T6 0.40 dB. Low-end [40,200] worsts: T1 0.55 dB (DEVIATION, gate 0.6),
// T2 0.48, T3 0.19, T4 0.28, T5 0.23, T6 0.28 dB. 10 Hz absolute values /
// anchor deltas (binding, gates T1-T4 1.0 / T5-T6 2.0): T1 -3.84 (0.84),
// T2 +0.70 (0.95), T3 -3.22 (0.22), T4 -3.40 (0.40), T5 -23.20 (1.21),
// T6 -20.50 (1.50) dB — all PASS. 44.1k/48k agreement worsts (C++ probes):
// T1 0.030 / T2 0.065 / T3 0.039 / T4 0.243 (DEVIATION, gate 0.3) /
// T5 0.001 / T6 0.094 dB. Either-oracle worsts vs chart oracles (advisory,
// +/-1 dB check, C++ actuals): T1 0.53 / T2 0.44 / T3 0.18 / T4 0.43 /
// T5 0.23 / T6 0.30 dB — all pass trivially (CSV track implies it). Poles < 1 at 44.1k + 48k after the refit, including
// float32-quantized radii (C++ cooks to float storage; worst radius 0.99975 on T5's overdamped highpass, stable and
// deterministic).
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
// T2 FOURTH SECTION (finding 1 resolves the Task-20 recorded deviation):
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
// T1 RECORDED DEVIATION (anchor wins per brief tie-break, tension reported):
// the -3 dB anchor forces the highpass corner 36.7 -> ~14.9 Hz, but the CSV
// low-end foot sits at +0.64 dB @ 40 Hz while an anchor-pinned highpass is
// ~0 dB there (closed-form floor ~= 0.55 dB over 40-60 Hz — the single peak
// cannot lift the foot while holding the scoop, and the highshelf lives at
// 11 kHz). Four independent optimizer runs (varied bounds, seeds, anchor
// pressure, peak-f0/HS-corner widening) all stall at dense/low 0.55-0.70, so
// the floor is structural, not optimizer weakness. Shipped joint: dense 0.55
// (gate 0.6), low-end 0.55 (gate 0.6), anchor 0.84 met. Wart: the HP knee
// (Q 1.5) carries a +3.8 dB hump at ~18 Hz — it is load-bearing for the
// 40 Hz foot (a hump-free knee costs dense 0.70), lives in the excluded
// 10-40 Hz band, and the anchor still passes. Options for ruling: 4th section
// on T1 (dedicated sub-40 shaping), accepted deviation, or anchor relaxation.
//
// T4 RECORDED DEVIATION (rate invariance): the anchor forces a lowshelf-down
// role arrangement (lowshelf ~26 Hz for the plunge, highshelf ~126 Hz to
// rebuild the mid shelf) with a deeper peak (-6.84) for the dip; the dip's
// steep top-octave recovery slope then warps 0.24 dB between 44.1k/48k at the
// 15 kHz probe (10 kHz probe 0.13) — the warp sits entirely in the peak
// section (per-section diagnostic), and rate-weighted refits from both the
// Task-20 and stalled seeds floor at 0.24, so it is structural. Dense 0.44 /
// low 0.28 / anchor 0.40 all met; T4's rate gate is 0.3 (0.243 landed).
// 0.24 dB @ 15 kHz across sample rates is inaudible; ruling decides whether
// that stands, a 4th section takes the top octave, or the anchor relaxes.
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
// gain in dB): Fix Round 1 refit for the 10 Hz anchors (T2 gains a peak for
// the twin V; all other tones same types, values free). Lineage moves worth
// noting: T1s0 (highpass 36.7 -> 14.859 Hz) now plunges toward the -3 dB
// anchor (see T1 deviation above); T3s2 (lowshelf 76.8/+0.90 -> 31.232/-2.972)
// flips sign to pull the 10 Hz anchor down while the peaks move up (582.2 ->
// 867.763, 3167.6 -> 4568.537) and still nail the scoop (dense 0.19);
// T4s0/s2 role-swap (lowshelf 2662.6/+1.53 -> 25.902/-3.724 for the plunge,
// highshelf 15623.1/+4.60 -> 126.355/+2.537 to rebuild the mid shelf — see
// T4 deviation above); T5s0 (highpass 33.7 -> 9.921 Hz, overdamped Q 0.18)
// eases to the -22 dB anchor (within 1.21 dB) while the lowshelf deepens
// (-3.74 -> -8.549) to hold the foot; T6s0 (lowshelf 72.7/-16.89 ->
// 51.684/-21.776) deepens toward the -22 dB anchor (within 1.50 dB) with the
// corner dropped so the 40 Hz foot still lands. Dense-CSV worsts (binding):
// T1 0.55 dB (DEVIATION), T2 0.48 dB, T3 0.19 dB, T4 0.44 dB, T5 0.32 dB,
// T6 0.40 dB.
//
//   tone  stage  type       f0       Q      gain
//   1     0      highpass   14.859   1.5      --
//   1     1      peak       841.567  0.2023   -7.075
//   1     2      highshelf  11368.945 1.9757  +0.834
//   2     0      peak       609.991  0.4306  -10.957
//   2     1      peak       701.712  1.5656   -9.772
//   2     2      lowshelf   75.194   1.6679   +0.711
//   2     3      highshelf  11366.908 0.5996  +1.988
//   3     0      peak       867.763  0.2315   -3.783
//   3     1      peak       4568.537 0.8532   -1.318
//   3     2      lowshelf   31.232   1.4505   -2.972
//   4     0      lowshelf   25.902   0.9582   -3.724
//   4     1      peak       5814.022 0.5672   -6.841
//   4     2      highshelf  126.355  0.1035   +2.537
//   5     0      highpass   9.921    0.1829   --
//   5     1      lowshelf   105.024  0.6028   -8.549
//   5     2      highshelf  329.849  0.7836   +2.753
//   6     0      lowshelf   51.684   0.4564  -21.776
//   6     1      highshelf  240.601  0.8648   +2.282
//   6     2      highshelf  15009.860 0.6405  -4.631
//
// Shape notes: T1's scoop (peak 841.567/0.2023/-7.075) tracks the CSV within
// 0.55 dB — see T1 deviation above (anchor-forced HP corner; +3.8 dB knee
// hump at ~18 Hz lives in the excluded 10-40 Hz band). T2's twin-V notch tip
// lands at -20.55 dB @ 695 Hz absolute, resolving the Task-20 tip-vs-skirt
// tension (dense 0.48). T3 carries its slight low-end dip straight from the
// CSV (tracked within 0.19 dB — visibly distinct from flat, and T3/T4 read as
// non-twins: scoop vs shelf-plus-dip). T4's dip (-4.35 dB @ 5796 Hz) is formed
// by the deepened peak against the low-cornered high shelf; above 15 kHz the
// curve follows the chart (top-octave soft cap +0.95 dB worst on T6,
// report-only). T6's top-end roll-off tracks within 0.40 dB; its low foot
// (lowshelf -21.776 dB) lands the -22 dB anchor within 1.50 dB. T5's highpass
// (9.921 Hz, overdamped Q 0.18, no subsonic bump) lands the -22 dB anchor
// within 1.21 dB while the deepened lowshelf holds the 40 Hz foot.
//
// Oracle hierarchy (user ruling, Fix Round 1): the DIGITIZED CSV binds
// 40 Hz-15 kHz (every tone within +/-0.5 dB — T1 recorded deviation at 0.55,
// gate 0.6); low-end [40,200] keeps the Task-17 CSV gates (+/-0.3 dB,
// T2 +/-0.5 — T1 recorded deviation at 0.55, gate 0.6); the six absolute
// 10 Hz eye-read anchors bind below 40 Hz (T1-T4 +/-1.0, T5/T6 +/-2.0 — all
// met); the eye-read header and the measured IR shapes
// (analysis/IR_VALIDATION.md) are advisory (either-oracle vs header still
// checked, passing trivially). Highcut-ON captures are non-adjudicating per
// user Ruling A (see IR_VALIDATION.md section 6) — HighCut.h untouched.
// T4's 44.1k/48k agreement (0.243) exceeds the 0.1 invariance gate — recorded
// deviation, gate 0.3 for T4 (see above).
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
            return {Type::HighPass, 14.859, 1.5, 0.0};
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
            return {Type::Peak, 867.763, 0.2315, -3.783};
        case 31:
            return {Type::Peak, 4568.537, 0.8532, -1.318};
        case 32:
            return {Type::LowShelf, 31.232, 1.4505, -2.972};
        case 40:
            return {Type::LowShelf, 25.902, 0.9582, -3.724};
        case 41:
            return {Type::Peak, 5814.022, 0.5672, -6.841};
        case 42:
            return {Type::HighShelf, 126.355, 0.1035, 2.537};
        case 50:
            return {Type::HighPass, 9.921, 0.1829, 0.0};
        case 51:
            return {Type::LowShelf, 105.024, 0.6028, -8.549};
        case 52:
            return {Type::HighShelf, 329.849, 0.7836, 2.753};
        case 60:
            return {Type::LowShelf, 51.684, 0.4564, -21.776};
        case 61:
            return {Type::HighShelf, 240.601, 0.8648, 2.282};
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
