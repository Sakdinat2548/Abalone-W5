// Abalone W5 - U5-inspired clean bass DI.
// Copyright (C) 2026 Sakdinat2548.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cmath>

// Subtle fixed Class-A color stage (Abalone W5 v1). No knob in v1.
//
// Chain position: Tone -> Color -> HighCut. Adds ~0.1% THD at +10dB input
// (chosen operating point — see analysis/LEVELS.md honesty note; the
// manual's "0.1% at +10dB" carries no level reference, so no spec match is
// claimed. Features list <0.5% THD/IMD as the only hard ceiling.)
// Bypassable for test.
//
// Transfer (enabled):
//     y = tanh(k*x) / tanh(k)  +  a*x^2,   k = 0.03, a = 6e-4.
//
// Why not pure tanh: tanh is an odd function, so tanh(k*x)/tanh(k) produces
// ONLY odd harmonics (H2 = 0 to numerical precision, ~1e-15 relative). The
// brief's own test (f) requires H2 dominance (single-ended Class-A character),
// which is physically unachievable with any odd-symmetric curve. The small
// quadratic term supplies the even order; it is explicitly allowed by the
// spec ("single tanh(k) with small k, or 2nd-harmonic blend") and AGENTS.md
// ("tanh/2nd-harmonic"). Deviation from the brief's Step-3 formula is
// deliberate and documented here.
//
// k derivation (cubic-term rule): tanh(z) ~= z - z^3/3 gives H3/fund ~= k^2*A^2/12.
//   At +10dB (A = 10^(10/20) = 3.1623, A^2 = 10): k = 0.03 -> H3 ~= 0.03^2*10/12
//   = 7.5e-4 = 0.075%. Measured (coherent DFT): 0.0748%.
// a derivation: x = A*sin gives a*x^2 = a*A^2/2 (DC) - (a*A^2/2)*cos(2wt), so
//   H2/fund ~= a*A/2. a = 6e-4 -> H2 = 6e-4*3.1623/2 = 9.49e-4 = 0.095% at +10dB.
//   Total THD = sqrt(0.095^2 + 0.075^2) = 0.121%: inside [0.05%, 0.2%], H2/H3 = 1.26x.
//   At 0dB (A = 1): H2 = 0.030%, H3 = 0.0075% -> 0.031% < 0.05%. Verified numerically
//   (double-precision oracle) before baking the constants.
//
// Level convention: levelDb in thdAt is peak dB relative to peak amplitude 1.0,
// i.e. amp = 10^(levelDb/20). +10dB -> 3.1623 peak (post-Boost operating level).
// Chain calibration (Task 29, see analysis/LEVELS.md): 0 dBFS = +24 dBu, so
// stage dBu = input_dBFS + 24 + 3N for boost step N; k/a values frozen per map.
//
// Side effects of the blend (all negligible, measured):
// - Small-signal gain = k/tanh(k) = 1.00030 (+0.0026dB ~= 1, as brief requires).
// - DC offset = a*A^2/2: 3.0mV at +10dB, 75mV at +24dB hot. Hardware U5 output
//   is DC-coupled anyway; downstream Trim/HighCut pass it unchanged.
// - No oversampling needed: harmonics decay geometrically (H5 ~ -100dB rel fund);
//   spectrum above Nyquist*0.9 is DFT noise floor (test asserts < -80dB rel H1).
//
// Header-only, dependency-free C++17 (<cmath> only). Stateless per-sample
// function: no state to denormalize. JUCE-free, MSYS2-GCC-syntax-clean.
struct ColorStage
{
    ColorStage () : norm_ (std::tanh (kDrive_)) {}

    void setEnabled (bool enabled) { enabled_ = enabled; }

    float processSample (float x)
    {
        if (!enabled_)
            return x; // bit-transparent bypass.

        return std::tanh (kDrive_ * x) / norm_ + kEven_ * x * x;
    }

    // Measurement helper (not part of the audio path): drives the stage with a
    // sine of `freqHz` at peak amplitude 10^(levelDb/20) and returns the THD
    // ratio sqrt(sum(H2..H8)^2)/H1 via coherent correlation (1s @48kHz, so
    // 100Hz/1kHz/5kHz complete integer cycles). Respects the enabled flag:
    // enable the stage before measuring (a disabled stage reads THD ~= 0).
    float thdAt (float freqHz, float levelDb)
    {
        if (!(freqHz > 0.0f))
            return 0.0f;

        constexpr double fs = 48000.0;
        constexpr int total = 48000; // 1s window.
        constexpr int maxHarm = 8;
        const double amp = std::pow (10.0, static_cast<double> (levelDb) / 20.0);

        double magSq[maxHarm + 1] = {};
        for (int h = 1; h <= maxHarm; ++h)
        {
            double re = 0.0;
            double im = 0.0;
            for (int n = 0; n < total; ++n)
            {
                const float x = static_cast<float> (
                    amp * std::sin (2.0 * kPi_ * static_cast<double> (freqHz) * static_cast<double> (n) / fs));
                const double y = static_cast<double> (processSample (x));
                const double a =
                    2.0 * kPi_ * static_cast<double> (freqHz) * static_cast<double> (h) * static_cast<double> (n) / fs;
                re += y * std::cos (a);
                im += y * std::sin (a);
            }
            const double mag = 2.0 * std::hypot (re, im) / static_cast<double> (total);
            magSq[h] = mag * mag;
        }

        if (!(magSq[1] > 0.0))
            return 0.0f;

        double rest = 0.0;
        for (int h = 2; h <= maxHarm; ++h)
            rest += magSq[h];
        return static_cast<float> (std::sqrt (rest / magSq[1]));
    }

private:
    static constexpr double kPi_ = 3.14159265358979323846;
    static constexpr float kDrive_ = 0.03f;  // tanh drive: sets H3 (see derivation above).
    static constexpr float kEven_ = 0.0006f; // 2nd-harmonic blend: sets H2 (see derivation above).

    bool enabled_ = true;
    float norm_ = 1.0f; // tanh(k): normalizes x=1 gain to 1, small-signal gain ~= 1.
};
