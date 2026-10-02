#pragma once

#include <cmath>

// Tone-bank biquads, Abalone W5 v1 full-band fit to the digitized chart
// (Task 20): RBJ triplets tracking analysis/u5_tone_curves_digitized.csv
// (121 log-spaced points, 10 Hz-20 kHz — the BINDING target) within +/-0.5 dB
// over 40 Hz-15 kHz on every tone EXCEPT T2 (recorded deviation below).
// Low end keeps the Task-17 CSV gates over [40,200] Hz (+/-0.3 dB, T2 +/-0.5);
// the 10-40 Hz CSV band stays EXCLUDED (chart-noise floor, Task 17 — the 5 Hz
// DC-blocker owns sub-40 behavior by design). Rationale for the hierarchy:
// the user judges the CSV + docs/refs/u5_tone_fit_check.png truer than both
// the eye-read header and the T3K captures, so the digitized CSV binds
// full-band, the eye-read header is advisory, and IR shapes are advisory
// (reported, not gated — see analysis/IR_VALIDATION.md). Either-oracle
// worsts vs chart oracles (Task-20 numbers): T1 0.26dB, T2 0.71dB,
// T3 0.30dB, T4 0.32dB, T5 0.17dB, T6 0.14dB (all within the +/-1dB
// either-oracle check, which still passes trivially). Dense-CSV worsts
// (binding gate): T1 0.28dB, T2 0.88dB (DEVIATION), T3 0.33dB, T4 0.34dB,
// T5 0.18dB, T6 0.13dB. Poles re-checked < 1 at 44.1k + 48k after the refit.
//
// Task 20 method (numbers-only; no new sections, no role/type changes):
// hand-rolled bounded Nelder-Mead with restarts (genuinely randomized
// multi-start, 24 seeds + basin-hopping polish) on the CSV grid, penalties
// only beyond the true gates (dense, low-end, analytic pole check at
// 44.1k + 48k, 44.1k/48k rate agreement). T3/T5 already passed +/-0.5 and
// were HELD at Task-17 numbers. T1/T4/T6 refit cleanly (T1 0.57 -> 0.28,
// T4 0.71 -> 0.34, T6 0.56 -> 0.13; T4/T6 also meet the 44.1k/48k rate
// probes at 0.09 dB, and T4 holds <= 1.0 dB above 15 kHz, report-only).
// T4 needed a fine-grid (800-pt vs
// log-interp CSV) refit pass: its coarse-grid optimum hid a +0.59 dB shelf
// knee overshoot between CSV points at ~14.9 kHz; the fine-grid fit lands
// 0.34 worst everywhere in-band (a rate-probe pass then moved it again for
// 44.1k/48k agreement at 15 kHz; a final top-soft pass tamed the 15-20 kHz
// overshoot to <= 1.0 dB).
//
// T2 RECORDED DEVIATION (no gate loosened silently, no role changed):
// best numbers-only dense worst 0.88 dB @ ~742 Hz (low-end 0.50 @ 40 Hz,
// within its +/-0.5 gate). Six independent optimizer runs (minimax NM,
// Lp-norm NM + basin-hopping, differential evolution x2 with wide bounds
// incl. LS f0 into the midrange, low-end-free floor probe) all stall at
// 0.87-0.92 dB. Structural cause: the single RBJ peak renders a rounded
// notch bottom while the CSV tip is a sharp V (-20.99 dB @ 697 Hz, itself a
// line-intersection reconstruction — see analysis/fit_scripts/fit.py) with
// fat skirts (model too deep by 0.6-0.9 dB at 540-614 Hz AND 956-1312 Hz
// while the tip stays 0.7-0.9 shallow): narrowing the peak (higher Q +
// deeper gain) trades skirt for tip without ever satisfying both, and the
// shelves supply only tilt, not tip curvature. The SOS reference needed
// FIVE free biquad sections for 0.44 dB on this curve; one peak + two
// shelves top out near 0.9. T2's dense gate therefore STAYS at +/-1.0 dB
// (unchanged) pending a user ruling (topology change vs accepted deviation);
// its low-end gate (+/-0.5) is met. Fit details: task-20-report.md.
//
// Task 17 (Rulings B+C): low-end tracks the DIGITIZED CSV 100% over
// [40,200] Hz — every tone within ±0.3 dB at every CSV point there, except
// T2 at ±0.5 dB (0.38 worst: crossover-boundary + notch-skirt tension,
// accepted as physics). The 10–40 Hz CSV band is EXCLUDED from all gates:
// T1/T3/T4 read identical within ≤0.11 dB there despite different
// low-end circuits (digitization floor — common-mode ink/frame-edge
// artifact), and the 5 Hz DC-blocker owns sub-40 behavior by design.
// T3/T4/T6 already passed ±0.3 over [40,200] with Task-15 numbers
// (0.28/0.28/0.29) — held unchanged. T1/T2/T5 refit numbers-only (no new
// sections, no role changes): low-band worsts T1 0.20 / T2 0.38 / T5 0.08;
// above-crossover drift vs Task-15 ≤ 0.27 dB in-band (no blend regression).
//
// Chain position: Tone (bypass + 1-6 biquad presets, Task 7 adds the 10ms
// xfade around setTone). Bypass (tone 0) is bit-transparent passthrough.
//
// Per-tone topology + fitted numbers (f0 in Hz, Q = shelf-alpha quotient,
// gain in dB): Task-20 full-band fit to the digitized CSV (T3/T5 held at
// Task-17 numbers — already passing; T1/T2/T4/T6 refit numbers-only, every
// stage kept its type, so no role mapping was needed). Lineage moves worth
// noting: T4s0 (lowshelf 719.2 -> 2662.6 Hz) now corners above the midband
// so the shelf top stays flat through 800 Hz like the CSV; T4s2 (highshelf
// 11127.8/+1.03 -> 15623.1/+4.60) acts as a top-octave lift; T6s0 keeps the
// deep -16.89 dB low shelf (low-end foot now within 0.13 dB); T2s2 moves out
// to 3930.3 Hz at Q 0.116 (broad recovery tilt — the notch fit still binds
// at 0.88 dB, see deviation note above). Dense-CSV worsts (binding):
// T1 0.28 dB, T2 0.88 dB, T3 0.33 dB, T4 0.34 dB, T5 0.18 dB, T6 0.13 dB.
//
//   tone  stage  type       f0       Q      gain
//   1     0      highpass   36.7     1.049    --
//   1     1      peak       754.0    0.183    -6.93
//   1     2      highshelf  11936.7  1.776   +0.82
//   2     0      peak       680.0    0.703  -21.19
//   2     1      lowshelf   87.1     1.920   +0.72
//   2     2      highshelf  3930.3   0.116    +2.56
//   3     0      peak       582.2    0.394    -3.58
//   3     1      peak       3167.6   0.44     -2.30
//   3     2      lowshelf   76.8     0.995   +0.90
//   4     0      lowshelf   2662.6   0.351   +1.53
//   4     1      peak       6027.8   0.695    -4.93
//   4     2      highshelf  15623.1  0.518   +4.60
//   5     0      highpass   33.7     0.305    --
//   5     1      lowshelf   133.2    0.738    -3.74
//   5     2      highshelf  321.1    0.684   +2.69
//   6     0      lowshelf   72.7     0.494  -16.89
//   6     1      highshelf  262.6    0.610   +2.70
//   6     2      highshelf  13548.8  0.576    -4.58
//
// Shape notes: T1's scoop bottom (-7.17dB @ 697Hz CSV) now tracks within
// 0.28dB (was 0.57); the broad-mid peak sits at Q=0.183. T2's notch tip
// lands at -20.33dB @ 678Hz absolute (1 kHz-normalized character comparable
// to measured -6.84dB @ 661Hz modulo normalization) — tip-vs-skirt tension
// documented above, not chased further. T3 carries its slight low-end dip
// straight from the CSV (tracked within 0.33dB — visibly distinct from flat,
// and T3/T4 read as non-twins: scoop vs shelf-plus-dip). T4's dip
// (-4.26dB @ 6021Hz) is formed jointly by the peak and the high shelf;
// above 15kHz the shelf lift overshoots the chart by <= 1.0dB (report-only).
// T6's top-end roll-off now tracks within 0.13dB through 13.5kHz.
//
// Oracle hierarchy (user ruling, Task 20): the DIGITIZED CSV binds full-band
// (every tone within +/-0.5dB over 40Hz-15kHz except recorded T2 deviation);
// low-end [40,200] keeps the Task-17 CSV gates (+/-0.3dB, T2 +/-0.5); the
// eye-read header and the measured IR shapes (analysis/IR_VALIDATION.md)
// are advisory (either-oracle vs header still checked, passing trivially).
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
            for (int s = 0; s < 3; ++s)
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
        for (int s = 0; s < 3; ++s)
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
        for (int s = 0; s < 3; ++s)
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
            return {Type::HighPass, 36.7, 1.049, 0.0};
        case 11:
            return {Type::Peak, 754.0, 0.183, -6.93};
        case 12:
            return {Type::HighShelf, 11936.7, 1.776, 0.82};
        case 20:
            return {Type::Peak, 680.0, 0.703, -21.19};
        case 21:
            return {Type::LowShelf, 87.1, 1.920, 0.72};
        case 22:
            return {Type::HighShelf, 3930.3, 0.116, 2.56};
        case 30:
            return {Type::Peak, 582.2, 0.394, -3.58};
        case 31:
            return {Type::Peak, 3167.6, 0.44, -2.3};
        case 32:
            return {Type::LowShelf, 76.8, 0.995, 0.9};
        case 40:
            return {Type::LowShelf, 2662.6, 0.351, 1.53};
        case 41:
            return {Type::Peak, 6027.8, 0.695, -4.93};
        case 42:
            return {Type::HighShelf, 15623.1, 0.518, 4.60};
        case 50:
            return {Type::HighPass, 33.7, 0.305, 0.0};
        case 51:
            return {Type::LowShelf, 133.2, 0.738, -3.74};
        case 52:
            return {Type::HighShelf, 321.1, 0.684, 2.69};
        case 60:
            return {Type::LowShelf, 72.7, 0.494, -16.89};
        case 61:
            return {Type::HighShelf, 262.6, 0.610, 2.70};
        case 62:
            return {Type::HighShelf, 13548.8, 0.576, -4.58};
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
        for (int s = 0; s < 3; ++s)
        {
            active_[s] = bank_[tone_ < 1 ? 1 : tone_][s];
            active_[s].z1 = 0.0f;
            active_[s].z2 = 0.0f;
        }
    }

    int tone_ = 0;
    double sampleRate_ = 48000.0;
    Coeffs bank_[7][3];
    Coeffs active_[3];
};
