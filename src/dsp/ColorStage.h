// Abalone W5 - U5-inspired clean DI.
// Copyright (C) 2026 Sakdinat2548.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cmath>

// Subtle fixed Class-A color stage (Abalone W5 v1). No knob in v1.
//
// Chain position: Tone -> Color -> HighCut. Adds ~0.47% THD at +10dB input
// (tanh core 0.38%, capture-fitted 2026-10-09, + gated mid-drive harmonics
// fitted 2026-10-12); bypassable for test. The manual's "0.1% at +10dB"
// carries no level reference, so no spec match is claimed either way.
// Features list <0.5% THD/IMD as the only hard ceiling.
//
// Transfer (enabled):
//     y = tanh(k*x) / tanh(k) + a*x^2 + gate(env) * branch(x),
//     k = 0.06, a = 1.5e-3, branch = odd+even resonant bumps (see below).
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
//   At +10dB (A = 10^(10/20) = 3.1623, A^2 = 10): k = 0.06 -> H3 ~= 0.0036*10/12
//   = 0.30% (measured coherent DFT: 0.2973%).
// a derivation: x = A*sin gives a*x^2 = a*A^2/2 (DC) - (a*A^2/2)*cos(2wt), so
//   H2/fund ~= a*A/2. a = 1.5e-3 -> H2 = 1.5e-3*3.1623/2 = 2.37e-3 = 0.237% at +10dB.
//   Total THD = sqrt(0.237^2 + 0.297^2) = 0.38%: H2/H3 = 0.80x (H3-led at this
//   drive; H2 dominates 2.5x at 0dB — Class-A character where it lives,
//   saturation takes over when driven, matching captures).
//   At 0dB (A = 1): H2 = 0.075%, H3 = 0.030% -> 0.081%. Verified numerically
//   (double-precision oracle) before baking the constants.
//
// Level convention: levelDb in thdAt is peak dB relative to peak amplitude 1.0,
// i.e. amp = 10^(levelDb/20). +10dB -> 3.1623 peak (post-Boost operating level).
// Chain calibration (Task 29, see analysis/LEVELS.md): 0 dBFS = +24 dBu, so
// stage dBu = input_dBFS + 24 + 3N for boost step N; k/a values frozen per map.
//
// XLR-cancellation caveat (web research 2026-10-05): the hardware's twin
// balanced Class-A outputs may partially cancel even-order harmonics at the
// XLR (matching-dependent, unknown amount). Our H2 level is therefore an
// upper bound on what the outputs deliver — modeled, not measured. No
// published H2/H3 spectrum exists to check against.
//
// Side effects of the blend (all negligible, measured):
// - Small-signal gain = k/tanh(k) = 1.00120 (+0.0104dB ~= 1, as brief requires).
// - DC offset = a*A^2/2: 7.5mV at +10dB, 188mV at +24dB hot (stripped by the
//   post-color 2Hz DC-blocker in real use). Hardware U5 output
//   is DC-coupled anyway; downstream Trim/HighCut pass it unchanged.
// - Harmonics decay fast past H8 (Gaussian tails); the gated branch adds
//   H3-H8 at mid drive only. Aliasing is measured, not assumed:
//   checkAliasCleanAt (1/3/5/8 kHz) requires non-harmonic content above
//   0.9*Nyquist < -80 dB rel H1 at 1x. If a future fit breaks it, the stage
//   rides the existing 2x/4x OS path (exact 0/40/60 latency untouched).
//
// Header-only, dependency-free C++17 (<cmath> only). One state (the gate
// envelope); denormal-safe by construction (snaps + FTZ-friendly zeroing).
// exp() underflows to exact 0 past |x| ~= 5.2 — by design, not a hazard
// (zero, never denormal). JUCE-free, MSYS2-GCC-syntax-clean.
struct ColorStage
{
    ColorStage () : norm_ (fastTanh (kDrive_)) { setSampleRate (48000.0); }

    void setEnabled (bool enabled)
    {
        enabled_ = enabled;
        reset();
    }

    // Output silence on the next block. Also clears the envelope (stale
    // level history is meaningless across resets).
    void reset () { env_ = 0.0f; }

    // Envelope time constants from rate (tau ~= 100 ms release, 0.1 ms
    // attack): identical seconds at 44.1/48 kHz and on the 2x/4x hot path
    // (the chain retunes on OS factor change, so hot-rate timing matches).
    void setSampleRate (double sampleRate)
    {
        if (!(sampleRate > 0.0))
            sampleRate = 48000.0;

        constexpr double kTauRel = 0.1;    // seconds.
        constexpr double kTauAtk = 0.0001; // seconds.
        kEnvRel_ = static_cast<float> (1.0 - std::exp (-1.0 / (kTauRel * sampleRate)));
        kEnvAtk_ = static_cast<float> (1.0 - std::exp (-1.0 / (kTauAtk * sampleRate)));
    }

    // Fast tanh (Lambert continued fraction, 5 levels): max abs error
    // 5.6e-7 vs std::tanh on [-2, 2], THD-identical to 4 decimals at every
    // operating point (gated in ColorStageTest). Valid here because the
    // argument z = k*x stays within |z| <= 1.9 (k = 0.06, |x| <= 31.6 =
    // 0 dBFS through Boost 10); never use it beyond +-2 without rechecking.
    static float fastTanh (float x)
    {
        const float x2 = x * x;
        float t = 9.0f + x2 / 11.0f;
        t = 7.0f + x2 / t;
        t = 5.0f + x2 / t;
        t = 3.0f + x2 / t;
        t = 1.0f + x2 / t;
        return x / t;
    }

    float processSample (float x)
    {
        if (!enabled_)
            return x; // bit-transparent bypass.

        // H-task 2026-10-12: envelope-gated harmonic branch (DE fit, loss
        // 1861 vs 170k+ for every static family). One gate scales odd+even
        // resonant bumps coherently: quiet -> shut (laws intact), mid-drive
        // -> open (paints H3-H6), hot -> relatively shut again (the branch /
        // tanh ratio shrinks with drive — Gaussian tails vanish while tanh
        // saturates; the gate itself saturates open, which is why hot
        // behavior still belongs to the core). Envelope is
        // peak-hold with fast attack + slow release (steady sine reads peak
        // amplitude, matching the fit); this is the stage's only state
        // (memory is what static curves provably cannot do — see fit notes).
        // Attack smoothing (0.1 ms) exists so sharp transients cannot step
        // the gate audibly (zipper); instant-vs-0.5 ms attack differs
        // <= -65 dBFS peak on slap material.
        // Non-finite input (NaN/Inf from a sick host): hold silence, touch
        // nothing, latch nothing. The old stateless stage recovered alone;
        // the envelope must not learn garbage.
        const float ax = std::fabs (x);
        if (!(ax <= 3.402823466e+38f))
            return 0.0f;
        if (ax > env_)
            env_ += kEnvAtk_ * (ax - env_);
        else
        {
            env_ += kEnvRel_ * (ax - env_);
            if (env_ < 1.0e-15f)
                env_ = 0.0f;
        }
        const float gate = std::pow (env_ / (env_ + kGateN0_), kGateQ_);
        const float uo = x / kOddW_;
        const float ue = x / kEvenW_;
        const float branch =
            gate * (kOddC_ * uo * uo * uo * std::exp (-uo * uo) + kEvenC_ * ue * ue * std::exp (-ue * ue));
        return fastTanh (kDrive_ * x) / norm_ + kEven_ * x * x + branch;
    }

    // Measurement helper (not part of the audio path): drives the stage with a
    // sine of `freqHz` at peak amplitude 10^(levelDb/20) and returns the THD
    // ratio sqrt(sum(H2..H12)^2)/H1 via coherent correlation (1s @48kHz, so
    // 100Hz/1kHz/5kHz complete integer cycles). Respects the enabled flag:
    // enable the stage before measuring (a disabled stage reads THD ~= 0).
    float thdAt (float freqHz, float levelDb)
    {
        if (!(freqHz > 0.0f))
            return 0.0f;

        constexpr double fs = 48000.0;
        constexpr int total = 48000; // 1s window.
        constexpr int maxHarm = 12;
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
    static constexpr float kDrive_ = 0.06f;  // tanh drive: sets H3 (see derivation above).
    static constexpr float kEven_ = 0.0015f; // 2nd-harmonic blend: sets H2 at low drive (see derivation above).
                                             // Minority H2 contributor above ~-36 dB stage, where the even bump
                                             // dominates by fit; kept (removing it would refit everything for
                                             // nothing audible).
    // H-task gated branch (DE fit 2026-10-12, loss 1861): odd bump paints
    // H3/H5, even bump paints H4/H6; gate opens mid-drive only. Hot-safe by
    // construction (Gaussian tails vanish for |x| >> w; gate saturates).
    static constexpr float kOddC_ = 0.03475f;
    static constexpr float kOddW_ = 0.56f;
    static constexpr float kEvenC_ = -0.05f;
    static constexpr float kEvenW_ = 0.82f;
    static constexpr float kGateN0_ = 10.526f;
    static constexpr float kGateQ_ = 0.60f;
    // Envelope release/attack coefficients (per-second taus, retuned in
    // setSampleRate): tau ~= 100 ms release (2% ripple at 440 Hz) and
    // ~= 0.1 ms attack (kills gate-step zipper, sonically identical).
    float kEnvRel_ = 0.000208f;
    float kEnvAtk_ = 0.19f;

    bool enabled_ = true;
    float env_ = 0.0f;  // peak-hold envelope for the harmonic gate (see processSample).
    float norm_ = 1.0f; // tanh(k): normalizes the tanh term to 1 at x=1
                        // (full transfer 1.0015 there with the blend;
                        // small-signal gain 1.0012).
};
