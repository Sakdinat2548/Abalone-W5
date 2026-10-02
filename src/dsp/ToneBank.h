#pragma once

#include <cmath>

// Tone-bank biquads fitted to the Avalon U5 manual tone chart (Abalone W5 v1).
//
// Chain position: Tone (bypass + 1-6 biquad presets, Task 7 adds the 10ms
// xfade around setTone). Bypass (tone 0) is bit-transparent passthrough.
//
// Per-tone topology + fitted numbers (f0 in Hz, Q = shelf-alpha quotient, gain
// in dB). Fit method: hand-seeded differential-evolution polish of RBJ
// parametric sections against analysis/u5_tone_curves_digitized.csv
// (121 log-spaced points/tone, 10Hz-20kHz); worst dense-CSV deltas in
// 40Hz-15kHz: T1 0.48dB, T2 0.85dB, T3 0.33dB, T4 0.50dB, T5 0.13dB,
// T6 0.23dB (all within +/-1dB). Task 8 re-tunes these from IR FFTs.
//
//   tone  stage  type       f0       Q      gain
//   1     0      highpass   22       1.00     --
//   1     1      peak       800      0.20    -6.8
//   1     2      highshelf  13000    1.30    +1.2
//   2     0      peak       680      0.73   -21.0
//   2     1      lowshelf   100      1.60    +0.9
//   2     2      highshelf  3200     0.20    +2.3
//   3     0      peak       600      0.40    -3.7
//   3     1      peak       3200     0.45    -2.3
//   3     2      lowshelf   75       1.00    +0.9
//   4     0      lowshelf   794      0.37    +1.8
//   4     1      peak       5715     0.98    -4.2
//   4     2      highshelf  11576    1.44    +1.4
//   5     0      highpass   33       0.30     --
//   5     1      lowshelf   130      0.75    -3.7
//   5     2      highshelf  335      0.70    +2.6
//   6     0      lowshelf   78       0.53   -15.8
//   6     1      highshelf  284      0.72    +2.5
//   6     2      highshelf  10842    0.83    -2.5
//
// Shape notes: T2's deep notch (~-21dB @ ~700Hz) is one Q~0.73 peak core
// plus a +0.9dB low shelf (lifts the 40Hz foot back to +1dB) and a broad
// high shelf that shapes the 1-4kHz wall. T4's ~6kHz dip tip falls between
// oracle points (bracketed by -1.5dB@4k / -0.5dB@10k in tone_targets.h);
// the -4.2dB peak @ 5715Hz fits the chart dip, the header only brackets it.
// T5/T6 share the low HP+shelf architecture; T6 adds a top-end roll-off
// (-2.5dB high shelf @ ~10.8kHz, ~-3dB at 20kHz).
//
// Oracle conflicts (measured): the Task 2 eye-read header and the machine
// CSV disagree by >1dB at 14 points (up to 3.5dB at T2@400Hz); the JSON
// reference SOS fits reproduce the CSV there, so the fits track the CSV.
// The test asserts each header point within +/-1dB of EITHER oracle.
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
            return {Type::HighPass, 22.0, 1.0, 0.0};
        case 11:
            return {Type::Peak, 800.0, 0.2, -6.8};
        case 12:
            return {Type::HighShelf, 13000.0, 1.3, 1.2};
        case 20:
            return {Type::Peak, 680.0, 0.73, -21.0};
        case 21:
            return {Type::LowShelf, 100.0, 1.6, 0.9};
        case 22:
            return {Type::HighShelf, 3200.0, 0.2, 2.3};
        case 30:
            return {Type::Peak, 600.0, 0.4, -3.7};
        case 31:
            return {Type::Peak, 3200.0, 0.45, -2.3};
        case 32:
            return {Type::LowShelf, 75.0, 1.0, 0.9};
        case 40:
            return {Type::LowShelf, 794.0, 0.37, 1.8};
        case 41:
            return {Type::Peak, 5715.0, 0.98, -4.2};
        case 42:
            return {Type::HighShelf, 11576.0, 1.44, 1.4};
        case 50:
            return {Type::HighPass, 33.0, 0.3, 0.0};
        case 51:
            return {Type::LowShelf, 130.0, 0.75, -3.7};
        case 52:
            return {Type::HighShelf, 335.0, 0.7, 2.6};
        case 60:
            return {Type::LowShelf, 78.0, 0.53, -15.8};
        case 61:
            return {Type::HighShelf, 284.0, 0.72, 2.5};
        case 62:
            return {Type::HighShelf, 10842.0, 0.83, -2.5};
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
