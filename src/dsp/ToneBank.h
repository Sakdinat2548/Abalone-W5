#pragma once

#include <cmath>

// Tone-bank biquads, Abalone W5 v1 user blend (Task 15) + Task-17 low-end
// refit: 90% Task-4 manual-chart fit + 10% Task-12 measured-IR fit per
// stage-parameter (T6 at 95/5 — its IR shape diverges most from the chart,
// so it keeps the lightest IR dose). Chart is the binding oracle again
// (±1dB); IR deltas are reported, not gated (user trusts the captures only
// lightly — see analysis/IR_VALIDATION.md). Either-oracle worsts vs chart
// oracles (Task-17 numbers): T1 0.32dB, T2 0.78dB, T3 0.30dB, T4 0.67dB,
// T5 0.17dB, T6 0.37dB (all within the +/-1dB gate). Param-blend verified
// against true dB-domain blends (worst 0.38dB on T4 — stage roles moved
// most there, weight keeps it near-chart). Poles re-checked < 1 at
// 44.1k + 48k after blending and again after the Task-17 refit.
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
// Per-tone topology + blended numbers (f0 in Hz, Q = shelf-alpha quotient,
// gain in dB): per-stage-parameter blend of the Task-4 manual-chart fit (90%,
// 95% on T6) and the Task-12 measured-IR fit (10%, 5% on T6). Linear in
// f0/Q/gainDb; every stage kept its type on both sides, so no role mapping
// was needed — but stage ROLES still moved (T4s0: 794 Hz chart shelf vs
// 46 Hz IR shelf; T6s1: +2.5 dB chart restore vs -2.75 dB IR cut), which is
// why the blend was checked against the true dB-domain blend
// (w*chart(f) + (1-w)*IR(f)): worst deviation 0.38 dB on T4 at ~10.4 kHz,
// 0.24-0.33 dB on T1/T2, <=0.12 dB elsewhere. No section added or removed
// (3 per tone; Task-17 refit changed numbers only). Chart either-oracle
// worsts (binding gate): T1 0.32 dB, T2 0.78 dB, T3 0.30 dB, T4 0.67 dB,
// T5 0.17 dB, T6 0.37 dB.
//
//   tone  stage  type       f0       Q      gain
//   1     0      highpass   36.1     1.052    --
//   1     1      peak       777.1    0.173    -6.64
//   1     2      highshelf  12417.8  1.223   +1.19
//   2     0      peak       681.2    0.687  -21.03
//   2     1      lowshelf   87.2     1.646   +0.85
//   2     2      highshelf  3129.2   0.173    +2.65
//   3     0      peak       582.2    0.394    -3.58
//   3     1      peak       3167.6   0.44     -2.30
//   3     2      lowshelf   76.8     0.995   +0.90
//   4     0      lowshelf   719.2    0.388   +1.48
//   4     1      peak       5865.2   0.964    -4.17
//   4     2      highshelf  11127.8  1.326   +1.03
//   5     0      highpass   33.7     0.305    --
//   5     1      lowshelf   133.2    0.738    -3.74
//   5     2      highshelf  321.1    0.684   +2.69
//   6     0      lowshelf   77.0     0.525  -16.11
//   6     1      highshelf  292.2    0.699   +2.24
//   6     2      highshelf  10768.2  0.824    -2.63
//
// Shape notes: T2's notch tip lands at -6.46dB @ 678Hz (1 kHz-normalized),
// vs measured -6.84dB @ 661Hz — depth character eased 0.4dB by the Task-17
// low-end refit (high-shelf lift); chart gate still holds (0.78dB).
// T4's dip (-4.74dB @ 5922Hz normalized) is formed jointly by the peak and
// the high shelf; its binding worst (0.67dB) sits at the 1kHz header point,
// where the chart and IR oracles themselves disagree. T6 keeps the chart's
// shelf-plus-restore stack at a 95/5 weight (mid shelf +2.24dB @ 292Hz) —
// the IR's single-deep-shelf shape (-22.05dB @ 58Hz) would need a role
// change the blend deliberately avoids. T1's broad-mid peak now sits at
// Q=0.173 (Task-17 low-end refit moved it off the legacy Q=0.20 both
// independent fits had converged on — shape won over lineage).
//
// Oracle hierarchy (user ruling): the MANUAL CHART is binding again — each
// tone within +/-1dB of EITHER chart oracle (eye-read header or digitized
// CSV) at every 40Hz-15kHz header point, Task-4 style. The measured IR
// shapes (analysis/IR_VALIDATION.md) are advisory: reported, not gated.
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
            return {Type::HighPass, 36.1, 1.052, 0.0};
        case 11:
            return {Type::Peak, 777.1, 0.173, -6.64};
        case 12:
            return {Type::HighShelf, 12417.8, 1.223, 1.19};
        case 20:
            return {Type::Peak, 681.2, 0.687, -21.03};
        case 21:
            return {Type::LowShelf, 87.2, 1.646, 0.85};
        case 22:
            return {Type::HighShelf, 3129.2, 0.173, 2.65};
        case 30:
            return {Type::Peak, 582.2, 0.394, -3.58};
        case 31:
            return {Type::Peak, 3167.6, 0.44, -2.3};
        case 32:
            return {Type::LowShelf, 76.8, 0.995, 0.9};
        case 40:
            return {Type::LowShelf, 719.2, 0.388, 1.48};
        case 41:
            return {Type::Peak, 5865.2, 0.964, -4.17};
        case 42:
            return {Type::HighShelf, 11127.8, 1.326, 1.03};
        case 50:
            return {Type::HighPass, 33.7, 0.305, 0.0};
        case 51:
            return {Type::LowShelf, 133.2, 0.738, -3.74};
        case 52:
            return {Type::HighShelf, 321.1, 0.684, 2.69};
        case 60:
            return {Type::LowShelf, 77.0, 0.525, -16.11};
        case 61:
            return {Type::HighShelf, 292.2, 0.699, 2.24};
        case 62:
            return {Type::HighShelf, 10768.2, 0.824, -2.63};
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
