#pragma once

#include <cmath>

// Tone-bank biquads fitted to measured Avalon U5 impulse responses (Abalone
// W5 v1, Task 12 re-fit; supersedes the Task 4 manual-chart fit where the IR
// adjudicates — see analysis/IR_VALIDATION.md).
//
// Chain position: Tone (bypass + 1-6 biquad presets, Task 7 adds the 10ms
// xfade around setTone). Bypass (tone 0) is bit-transparent passthrough.
//
// Per-tone topology + fitted numbers (f0 in Hz, Q = shelf-alpha quotient, gain
// in dB). Fit method: differential-evolution + coordinate-descent polish of
// RBJ parametric sections against Welch cross-spectral relative shapes
// (each highcut-off capture vs the TONE0 capture, 28 x ~11s Hann segments,
// coherence >= 0.9997 everywhere in-band, 1 kHz-normalized, 40 Hz-15 kHz);
// worst dense-grid (2000-pt) deltas vs measured in 40Hz-15kHz:
// T1 0.016dB, T2 0.012dB, T3 0.056dB, T4 0.246dB, T5 0.017dB, T6 0.306dB
// (all within the +/-1dB gate). No section was added or removed (3 per tone);
// stage roles were kept, except T6's mid shelf, which changed from a low-end
// restore (+2.5dB @ 284Hz) to a mid cut (-2.75dB @ 447Hz) because the measured
// low end needs a single deeper shelf (-22.05dB @ 58Hz) instead of the
// chart's shelf-plus-restore stack.
//
//   tone  stage  type       f0       Q      gain
//   1     0      highpass   19.4     0.662    --
//   1     1      peak       831      0.20    -7.63
//   1     2      highshelf  7178     0.530   +1.08
//   2     0      peak       659      0.697  -20.67
//   2     1      lowshelf   38.9     0.637   -1.35
//   2     2      highshelf  3385     0.498   +2.33
//   3     0      peak       422      0.338   -2.48
//   3     1      peak       2876     0.347   -2.26
//   3     2      lowshelf   92.7     0.947   +0.90
//   4     0      lowshelf   46.3     0.550   -1.40
//   4     1      peak       7217     0.823   -3.91
//   4     2      highshelf  7094     0.300   -2.31
//   5     0      highpass   31.3     0.355    --
//   5     1      lowshelf   93.7     0.593   -3.45
//   5     2      highshelf  195.8    0.537   +3.54
//   6     0      lowshelf   58.2     0.438  -22.05
//   6     1      highshelf  447      0.300   -2.75
//   6     2      highshelf  9367     0.705   -5.06
//
// Shape notes: T2's notch tip lands at -6.85dB @ 658Hz (1 kHz-normalized),
// vs measured -6.84dB @ 661Hz — depth character matched, single-bin tip not
// chased. T4's dip (-4.82dB @ 7332Hz vs measured -4.59dB @ 7558Hz) is formed
// jointly by the peak and the high shelf at nearly the same f0; its worst
// residual (0.246dB) sits at the 15kHz gate endpoint, where the measured
// +3.5dB/oct recovery slope also binds the 44.1k/48k invariance probe (0.070
// vs the 0.1dB test limit). T6's top cut (-5.06dB shelf @ 9367Hz) is the same
// trade: residual 0.306dB at ~12.1kHz, invariance 0.069dB at 15kHz —
// near-Nyquist knees warp slightly across rates, so both tones spend most of
// the invariance budget there (deterministic, not noise; recorded for future
// edits). T1 keeps its legacy Q=0.20 broad-mid peak (both independent fits
// converged on it).
//
// Oracle conflicts (measured): the Task 2 eye-read header and the machine
// CSV disagree with the IR at many points (e.g. T6@150Hz: header -7.00,
// CSV -5.58, IR -3.17); the fits track the IR. The test asserts each tone
// within +/-1dB of the MEASURED shape (40Hz-15kHz, 1kHz-normalized); the old
// either-oracle check is kept report-only for the Python-port verification
// chain. Highcut-ON captures are non-adjudicating per user Ruling A (see
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
            return {Type::HighPass, 19.4, 0.662, 0.0};
        case 11:
            return {Type::Peak, 831.0, 0.2, -7.63};
        case 12:
            return {Type::HighShelf, 7178.0, 0.53, 1.08};
        case 20:
            return {Type::Peak, 659.0, 0.697, -20.67};
        case 21:
            return {Type::LowShelf, 38.9, 0.637, -1.35};
        case 22:
            return {Type::HighShelf, 3385.0, 0.498, 2.33};
        case 30:
            return {Type::Peak, 422.0, 0.338, -2.48};
        case 31:
            return {Type::Peak, 2876.0, 0.347, -2.26};
        case 32:
            return {Type::LowShelf, 92.7, 0.947, 0.9};
        case 40:
            return {Type::LowShelf, 46.3, 0.55, -1.4};
        case 41:
            return {Type::Peak, 7217.0, 0.823, -3.91};
        case 42:
            return {Type::HighShelf, 7094.0, 0.3, -2.31};
        case 50:
            return {Type::HighPass, 31.3, 0.355, 0.0};
        case 51:
            return {Type::LowShelf, 93.7, 0.593, -3.45};
        case 52:
            return {Type::HighShelf, 195.8, 0.537, 3.54};
        case 60:
            return {Type::LowShelf, 58.2, 0.438, -22.05};
        case 61:
            return {Type::HighShelf, 447.0, 0.3, -2.75};
        case 62:
            return {Type::HighShelf, 9367.0, 0.705, -5.06};
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
