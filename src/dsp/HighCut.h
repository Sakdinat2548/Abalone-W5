// Abalone W5 - U5-inspired clean bass DI.
// Copyright (C) 2026 Sakdinat2548.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cmath>

// Switchable 8kHz high-cut (lowpass) stage (Abalone W5 v1).
//
// Chain position: ... -> Color -> HighCut -> Trim. One-pole causal smoother
//   y[n] = y[n-1] + a * (x[n] - y[n-1]),
// the lowpass dual of the exponential-smoother family used for GainStage's
// 5Hz highpass (there: hp = x - lp with the same smoother form). Plain causal
// one-pole, hence minimum-phase by construction (no linear-phase FIR).
//
// Coefficient derivation (exact -3dB match at 8kHz for any sample rate):
//   H(z) = a / (1 - b*z^-1), b = 1 - a.
//   |H(e^jw)|^2 = a^2 / (1 + b^2 - 2*b*cos w), w = 2*pi*8000/fs.
//   Setting |H|^2 = 1/2 at w with a = 1 - b:
//     2*(1 - 2b + b^2) = 1 + b^2 - 2b*cos w
//     b^2 - 2*(2 - cos w)*b + 1 = 0
//     b = (2 - cos w) - sqrt ((2 - cos w)^2 - 1)  (smaller root, 0 < b < 1),
//     a = 1 - b.
//   At 48kHz a ~= 0.6180, at 44.1kHz a ~= 0.6439; both measure -3.01dB at 8kHz.
//
// NOTE (deviation from the plan brief's suggested a = 1 - exp(-2*pi*8000/fs)):
// that matched-z form is the same smoother family but its -3dB point sits
// ~0.4dB high (-2.63dB @48k, -2.57dB @44.1k, verified numerically), outside
// the +/-0.3dB test tolerance and off the "-3dB at 8kHz" spec. The exact-match
// coefficient above keeps the identical filter structure and meets spec.
//
// Header-only, dependency-free C++17 (<cmath> only). No new/heap in the audio
// path. Denormal-safe by construction: state snaps to 0 below 1e-15.
// setEnabled clears the state, but only on transitions, so a per-block
// setEnabled (same value) never disturbs the settling filter; toggling
// re-arms from zero (click-safe; Task 7 owns crossfades).
struct HighCut
{
    HighCut ()
    {
        setSampleRate (48000.0);
        setEnabled (false);
    }

    void setSampleRate (double sampleRate)
    {
        if (!(sampleRate > 0.0))
            sampleRate = 48000.0;

        sampleRate_ = sampleRate;
        constexpr double fc = 8000.0;
        constexpr double twoPi = 6.28318530717958647692;
        const double w = twoPi * fc / sampleRate_;
        const double c = std::cos (w);
        const double t = 2.0 - c;
        const double b = t - std::sqrt (t * t - 1.0);
        coeff_ = static_cast<float> (1.0 - b);
    }

    void setEnabled (bool enabled)
    {
        if (enabled != enabled_)
        {
            enabled_ = enabled;
            state_ = 0.0f;
        }
    }

    float processSample (float x)
    {
        if (!enabled_)
            return x;

        state_ += coeff_ * (x - state_);
        // Float settle: snap near-zero state to exact 0 (denormal-safe).
        if (std::fabs (state_) < 1.0e-15f)
            state_ = 0.0f;
        return state_;
    }

private:
    double sampleRate_ = 48000.0;
    float coeff_ = 0.0f;
    float state_ = 0.0f;
    bool enabled_ = false;
};
