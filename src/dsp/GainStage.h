#pragma once

#include <cmath>

// Stepped clean-gain stage with input-side DC-block (Abalone W5 v1).
//
// Chain position: Boost -> DC-block is folded into this struct: the input
// first passes a one-pole 5Hz highpass (blocks DC, like the hardware's
// high-voltage headroom), then clean float gain is applied. Never clips.
//
// Step mapping: 1-based Choice 1-10 -> +3..+30dB, getDb() = step * 3.0f.
// Out-of-range steps clamp to [1, 10].
//
// HP derivation (exponential-smoother lowpass subtraction):
//   Continuous first-order lowpass H(s) = 1 / (1 + s/wc), wc = 2*pi*fc.
//   Exact zero-order-hold discretization gives the smoother
//       lp[n] = lp[n-1] + a * (x[n] - lp[n-1]),  a = 1 - exp(-wc / fs),
//   and the highpass output hp[n] = x[n] - lp[n] (DC gain 0, HF gain 1).
//   fc = 5Hz; a is recomputed in setSampleRate() for any rate.
//   At 48kHz a ~= 6.544e-4 (tau ~= 31.8ms), so 1.0f DC settles to ~2e-14
//   after 1s; loss at 1kHz is ~0.0001dB (negligible).
//
// Header-only, dependency-free C++17 (<cmath> only). Denormal-safe by
// construction: the HP state snaps to 0 below 1e-15.
struct GainStage
{
    GainStage ()
    {
        setSampleRate (48000.0);
        setStep (1);
    }

    void setSampleRate (double sampleRate)
    {
        if (!(sampleRate > 0.0))
            sampleRate = 48000.0;

        sampleRate_ = sampleRate;
        constexpr double fc = 5.0;
        constexpr double twoPi = 6.28318530717958647692;
        hpCoeff_ = static_cast<float> (1.0 - std::exp (-twoPi * fc / sampleRate_));
    }

    void setStep (int step)
    {
        if (step < 1)
            step = 1;
        if (step > 10)
            step = 10;

        step_ = step;
        gainLin_ = std::pow (10.0f, getDb() / 20.0f);
    }

    float getDb () const { return static_cast<float> (step_) * 3.0f; }

    float processSample (float x)
    {
        lp_ += hpCoeff_ * (x - lp_);
        if (std::fabs (lp_) < 1.0e-15f)
            lp_ = 0.0f;
        return (x - lp_) * gainLin_;
    }

private:
    int step_ = 1;
    double sampleRate_ = 48000.0;
    float hpCoeff_ = 0.0f;
    float lp_ = 0.0f;
    float gainLin_ = 1.0f;
};
