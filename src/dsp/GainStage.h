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
        // Start settled at the target: no fade-in on plugin load, and every
        // test that sets a step before processing measures exact gain from
        // sample 0. Only mid-stream setStep calls ramp (the musical case).
        curGainLin_ = targetGainLin_;
    }

    void setSampleRate (double sampleRate)
    {
        if (!(sampleRate > 0.0))
            sampleRate = 48000.0;

        sampleRate_ = sampleRate;
        constexpr double fc = 5.0;
        constexpr double twoPi = 6.28318530717958647692;
        hpCoeff_ = static_cast<float> (1.0 - std::exp (-twoPi * fc / sampleRate_));
        // Gain-smoothing follower, fc = 10Hz (~16ms TC, ~80ms to settle):
        // same smoother family as the HP above. Rate changes keep the
        // current gain (continuous); only the coefficient retunes.
        constexpr double smoothFc = 10.0;
        smoothCoeff_ = static_cast<float> (1.0 - std::exp (-twoPi * smoothFc / sampleRate_));
    }

    void setStep (int step)
    {
        if (step < 1)
            step = 1;
        if (step > 10)
            step = 10;

        step_ = step;
        // Order: the HP (DC-block) runs first in processSample, this gain after.
        // Target only: the per-sample gain slews toward it (see processSample),
        // so knob twists ramp instead of jumping (no zipper noise).
        targetGainLin_ = std::pow (10.0f, getDb() / 20.0f);
    }

    float getDb () const { return static_cast<float> (step_) * 3.0f; }

    float processSample (float x)
    {
        lp_ += hpCoeff_ * (x - lp_);
        // Float settle: snap near-zero state to exact 0 (denormal-safe).
        if (std::fabs (lp_) < 1.0e-15f)
            lp_ = 0.0f;
        // Smoothed stepped gain: one-pole follower (~16ms TC at fc = 10Hz),
        // so 3dB step twists ramp over ~80ms instead of clicking. Snaps exact
        // below 1e-6 so settled measurements see the textbook gain bit-exact.
        curGainLin_ += smoothCoeff_ * (targetGainLin_ - curGainLin_);
        if (std::fabs (targetGainLin_ - curGainLin_) < 1.0e-6f)
            curGainLin_ = targetGainLin_;
        return (x - lp_) * curGainLin_;
    }

private:
    int step_ = 1;
    double sampleRate_ = 48000.0;
    float hpCoeff_ = 0.0f;
    float lp_ = 0.0f;
    float targetGainLin_ = 1.0f;
    float curGainLin_ = 1.0f;
    float smoothCoeff_ = 0.0f;
};
