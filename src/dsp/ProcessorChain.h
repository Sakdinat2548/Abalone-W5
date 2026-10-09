// Abalone W5 - U5-inspired clean DI.
// Copyright (C) 2026 Sakdinat2548.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <cmath>

#include "ColorStage.h"
#include "GainStage.h"
#include "HighCut.h"
#include "ToneBank.h"

// Testable U5 signal chain (Abalone W5 v1). JUCE-free, header-only C++17
// (<cmath> + <atomic> only); owns every DSP stage so CTest can drive the
// full chain without a plugin host. The processor delegates per sample.
//
// Chain order: GainStage (boost + 5Hz DC-block) -> ToneBank A/B xfade ->
// ColorStage -> post-color 2Hz DC-block -> HighCut -> trim gain ->
// fixed hardware tilt (LS120/HS8k, engaged only) -> peak tracker.
//
// Tone xfade: two ToneBank instances (active + shadow). On
// setTone(new) != target, the shadow takes the new tone from a clean state
// and output crossfades equal-power over 10ms (xfadeLen = round(0.01*fs):
// 480 samples @48k, 441 @44.1k) from old to new. During the fade both
// instances process the same boosted input; gains are cos/sin of
// (pi/2 * pos/len), so old starts at 1 and new at 0: no discontinuity.
// A retarget mid-fade re-arms the shadow from the still-active old tone.
// Single-threaded by contract: the audio thread owns the chain; the editor
// only reads the atomic peak (reset-on-read).
//
// No heap in the audio path: fixed members only. Trim is a plain float
// multiply. Peak is max |post-trim| since the last getLastPeak() call.
//
// 1x/2x/4x oversampling around the ColorStage ONLY (OS mini-knob,
// `osfactor` Choice 1x/2x/4x, default 1x). The tanh is the sole nonlinear
// stage, so it alone runs hot: zero-stuff up -> linear-phase FIR (x2
// passband gain per doubling) -> the ColorStage transfer at the hot rate ->
// the identical FIR -> decimate. All linear stages stay at 1x: no wasted
// cycles, and biquads keep host-rate coefficients (no tone-shape change
// with rate doubling).
//
// Filter: 81-tap windowed-sinc lowpass (Hamming, fc = 0.25 cycles/sample
// at the hot rate = the upsampled Nyquist, the midpoint of the pass/stop
// transition for both 44.1k and 48k), DC gain normalized to 1. The 33-tap/0.23
// prototype drooped -3.2dB@20kHz/48k and -10.2dB@20kHz/44.1k (measured
// linear cascade, no gate covered it); the 81-tap holds +/-0.04dB to 20kHz
// at both rates (the ChainTest top-octave gates pin 1x-vs-2x and 1x-vs-4x
// within +/-0.5dB at 15/20kHz, both rates).
// The 4x path cascades 2x->2x with the SAME taps (no new filter design):
// the second doubling needs cutoff = its own input Nyquist = 0.25 at the
// 4x rate, which is exactly the shared fc, so the taps are correct at both
// hot rates by construction.
// Latency: each FIR delays (81-1)/2 samples at its hot rate. 2x: up+down
// at 2x = (81-1)/4 each = 20+20 = 40 host samples, rate-independent.
// 4x: up1+dn1 at 2x (20+20) plus up2+dn2 at 4x ((81-1)/8 = 10+10) = 60
// host samples, rate-independent (cutoffs are fractions of the hot rates,
// so the same taps serve 44.1k and 48k). Reported via getLatencySamples()
// for the processor's setLatencySamples; the ChainTest factor-latency gate
// pins reported == measured peak == 0/40/60 per factor per rate.
//
// Factor switching: setOsFactor() arms a 5ms equal-power crossfade
// (cos/sin, the tone-xfade idiom) between the from-factor and to-factor
// paths from the next sample; the processor calls it once per block, so the
// switch lands on a block boundary, never mid-block. On landing, the delay
// lines of every factor except the landing target are zeroed (never at arm:
// wiping at arm would collapse the live lines the fade-out source still
// reads), so every entry starts clean and the fade covers the entry
// transient either way. 1x steady output is exactly the old direct path
// (byte-identical default behavior).
// setOversampled(bool) is the Task-18 compat shim (false->1x, true->2x);
// new code uses setOsFactor(1/2/4).
struct ProcessorChain
{
    // 2x resampler taps (windowed-sinc lowpass, see note above) and the
    // exact hot-path group delays in host samples: each FIR delays
    // (kOsTaps-1)/2 samples at its hot rate, so 2x up + down in series =
    // (kOsTaps-1)/2 = 40, and the 4x cascade adds a second doubling at
    // half the host-rate cost: 20+10+10+20 = 60.
    static constexpr int kOsTaps = 81;
    static constexpr int kOsLatencySamples = (kOsTaps - 1) / 2;       // 2x: 40.
    static constexpr int kOsLatency4xSamples = 3 * (kOsTaps - 1) / 4; // 4x: 60.

    ProcessorChain ()
    {
        initOsFir();
        setSampleRate (48000.0);
        gain_.setStep (1);
        hardSelectTone (3);
        trimLin_ = 1.0f;
    }

    void setSampleRate (double sampleRate)
    {
        if (!(sampleRate > 0.0))
            sampleRate = 48000.0;

        sampleRate_ = sampleRate;
        gain_.setSampleRate (sampleRate);
        banks_[0].setSampleRate (sampleRate);
        banks_[1].setSampleRate (sampleRate);
        highcut_.setSampleRate (sampleRate);
        // Post-color DC-block retune (same smoother family as GainStage).
        // 2Hz not 5Hz: gates pass as-written, DC dies the same (corner only sets ~0.4s settling).
        constexpr double postFc = 2.0;
        constexpr double postTwoPi = 6.28318530717958647692;
        postHpCoeff_ = static_cast<float> (1.0 - std::exp (-postTwoPi * postFc / sampleRate_));
        // Fixed hardware tilt (engaged path only — ACTIVE-off passthrough
        // returns before processSample's tail, so bypass stays bit-exact):
        // LS 75Hz +0.91dB Q0.54 + HS 5441Hz -0.34dB Q0.62 + peak 180Hz
        // +0.11dB Q1.97, Q-parametrized RBJ (same cookbook as
        // ToneBank::cook; the ChainTest tilt oracle pins it). States flushed
        // like the OS lines: history is meaningless across rates.
        cookTiltShelf (false, 75.0, 0.54, 0.91, sampleRate_, tiltB_[0], tiltA_[0]);
        cookTiltShelf (true, 5441.0, 0.62, -0.34, sampleRate_, tiltB_[1], tiltA_[1]);
        cookTiltPeak (180.0, 1.97, 0.11, sampleRate_, tiltB_[2], tiltA_[2]);
        tiltZ_[0][0] = tiltZ_[0][1] = tiltZ_[1][0] = tiltZ_[1][1] = tiltZ_[2][0] = tiltZ_[2][1] = 0.0f;
        xfadeLen_ = static_cast<int> (std::lround (0.01 * sampleRate));
        if (xfadeLen_ < 1)
            xfadeLen_ = 1;
        if (xfadePos_ > xfadeLen_)
            xfadePos_ = xfadeLen_;
        // Rate change flushes the hot delay lines (content is signal
        // history, meaningless across rates); taps are rate-independent.
        osUpD_.fill (0.0f);
        osDnD_.fill (0.0f);
        os4Up1D_.fill (0.0f);
        os4Up2D_.fill (0.0f);
        os4Dn2D_.fill (0.0f);
        os4Dn1D_.fill (0.0f);
        osLen_ = static_cast<int> (std::lround (0.005 * sampleRate));
        if (osLen_ < 1)
            osLen_ = 1;
        if (osPos_ > osLen_)
            osPos_ = osLen_;
    }

    void setBoostStep (int step) { gain_.setStep (step); }

    // Tone 0-6, 0 = bypass. Out-of-range values clamp. Arms the 10ms
    // equal-power xfade; a no-op when the tone is already targeted.
    void setTone (int tone)
    {
        if (tone < 0)
            tone = 0;
        if (tone > 6)
            tone = 6;
        if (tone == targetTone_)
            return;

        targetTone_ = tone;
        banks_[1 - active_].setTone (tone); // shadow starts clean.
        xfadePos_ = 0;
        xfading_ = true;
    }

    void setHighcut (bool enabled) { highcut_.setEnabled (enabled); }

    void setColorEnabled (bool enabled) { color_.setEnabled (enabled); }

    // Oversample factor for the ColorStage path (1, 2, or 4; anything else
    // clamps to the nearest). Arms the 5ms equal-power xfade from the
    // previous target; a no-op when already targeted. Called once per block
    // by the processor (never mid-block). The FIR states are NOT wiped here
    // (see note above); they are zeroed on landing for every factor except
    // the landing target instead.
    void setOsFactor (int factor)
    {
        const int target = (factor <= 1) ? 1 : (factor == 2) ? 2 : (factor < 4) ? 2 : 4;
        if (target == osTarget_)
            return;
        osBlendFrom_ = osTarget_;
        osTarget_ = target;
        osPos_ = 0;
        osBlending_ = true;
    }

    int getOsFactor () const { return osTarget_; }

    // Task-18 compat shim: false->1x, true->2x.
    void setOversampled (bool oversampled) { setOsFactor (oversampled ? 2 : 1); }

    bool isOversampled () const { return osTarget_ != 1; }

    // Latency to report to the DAW: 0 at 1x, the exact FIR group delay at
    // 2x (40) or 4x (60). Follows the factor target immediately (the 5ms
    // entry blend is transient; the steady path carries the full delay).
    int getLatencySamples () const
    {
        return (osTarget_ == 4) ? kOsLatency4xSamples : (osTarget_ == 2) ? kOsLatencySamples : 0;
    }

    void setTrimDb (float trimDb) { trimLin_ = std::pow (10.0f, trimDb / 20.0f); }

    float processSample (float x)
    {
        const float boosted = gain_.processSample (x);

        float shaped;
        if (xfading_)
        {
            const float yOld = banks_[active_].processSample (boosted);
            const float yNew = banks_[1 - active_].processSample (boosted);
            ++xfadePos_;
            float t = static_cast<float> (xfadePos_) / static_cast<float> (xfadeLen_);
            if (t > 1.0f)
                t = 1.0f;
            constexpr float halfPi = 1.57079632679489661923f;
            shaped = std::cos (halfPi * t) * yOld + std::sin (halfPi * t) * yNew;
            if (xfadePos_ >= xfadeLen_)
            {
                xfading_ = false;
                active_ = 1 - active_;
            }
        }
        else
        {
            shaped = banks_[active_].processSample (boosted);
        }

        const float colored = colorWithOsBlend (shaped);
        // Post-color DC-block: kills the ColorStage `a*x^2` rectification
        // (~3mV at +10dB, hot above) before it reaches the output. One-pole
        // 2Hz highpass, same smoother form as the input block (deliberately
        // copied, not shared: two uses do not earn an abstraction).
        postLp_ += postHpCoeff_ * (colored - postLp_);
        if (std::fabs (postLp_) < 1.0e-15f)
            postLp_ = 0.0f;
        const float deblocked = colored - postLp_;
        const float cut = highcut_.processSample (deblocked);
        const float out = cut * trimLin_;
        // Fixed hardware tilt (see setSampleRate): three TDF-II sections
        // (LS + HS + low-mid peak). Denormal snap mirrors ToneBank (states
        // freeze exact at silence).
        float tilted = out;
        for (int s = 0; s < 3; ++s)
        {
            const float y = tiltB_[s][0] * tilted + tiltZ_[s][0];
            tiltZ_[s][0] = tiltB_[s][1] * tilted - tiltA_[s][0] * y + tiltZ_[s][1];
            tiltZ_[s][1] = tiltB_[s][2] * tilted - tiltA_[s][1] * y;
            if (std::fabs (tiltZ_[s][0]) < 1.0e-15f)
                tiltZ_[s][0] = 0.0f;
            if (std::fabs (tiltZ_[s][1]) < 1.0e-15f)
                tiltZ_[s][1] = 0.0f;
            tilted = y;
        }

        // Pre-trim tap (post-HighCut, pre-trim-gain peak) for the SIGNAL LED:
        // the hardware has no trim so its LED can't see one; the pre-trim tap
        // keeps Boost staging readable at -18dBFS workflows. Tracked alongside
        // (never instead of) the legacy post-trim peak below, so existing
        // peak-tracker behavior is unchanged.
        const float preMag = std::fabs (cut);
        if (preMag > prePeak_.load (std::memory_order_relaxed))
            prePeak_.store (preMag, std::memory_order_relaxed);

        const float mag = std::fabs (tilted);
        if (mag > peak_.load (std::memory_order_relaxed))
            peak_.store (mag, std::memory_order_relaxed);
        return tilted;
    }

    // Max |post-trim| since the last call; resets to 0 on read. Legacy tap,
    // kept for the unit-tested peak-tracker behavior; the editor SIGNAL LED
    // reads getLastPreTrimPeak() instead (drained here alongside).
    float getLastPeak () const { return peak_.exchange (0.0f, std::memory_order_relaxed); }

    // Max |pre-trim| (post-HighCut, pre-trim-gain) since the last call;
    // resets to 0 on read. This is the SIGNAL LED tap in normal operation.
    float getLastPreTrimPeak () const { return prePeak_.exchange (0.0f, std::memory_order_relaxed); }

private:
    // Color section with the hot-factor path. Steady state runs exactly one
    // path (1x direct = the historical code path); during the 5ms entry
    // blend both run and mix equal-power, old at weight cos, new at sin.
    float colorWithOsBlend (float shaped)
    {
        if (!osBlending_)
            return processFactor (osActive_, shaped);

        const float yOld = processFactor (osBlendFrom_, shaped);
        const float yNew = processFactor (osTarget_, shaped);
        ++osPos_;
        float t = static_cast<float> (osPos_) / static_cast<float> (osLen_);
        if (t > 1.0f)
            t = 1.0f;
        constexpr float halfPi = 1.57079632679489661923f;
        const float out = std::cos (halfPi * t) * yOld + std::sin (halfPi * t) * yNew;
        if (osPos_ >= osLen_)
        {
            osBlending_ = false;
            osActive_ = osTarget_;
            // Landed: zero the delay lines of every factor except the
            // landing target, now that nothing reads them, so the next
            // entry into any hot factor starts clean (see setOsFactor).
            if (osTarget_ != 2)
            {
                osUpD_.fill (0.0f);
                osDnD_.fill (0.0f);
            }
            if (osTarget_ != 4)
            {
                os4Up1D_.fill (0.0f);
                os4Up2D_.fill (0.0f);
                os4Dn2D_.fill (0.0f);
                os4Dn1D_.fill (0.0f);
            }
        }
        return out;
    }

    float processFactor (int factor, float x)
    {
        return (factor == 4)   ? processOversampled4x (x)
               : (factor == 2) ? processOversampled (x)
                               : color_.processSample (x);
    }

    // One host-rate sample through the 2x color path: zero-stuff up (x2 to
    // preserve passband gain), saturate both 2x phases, lowpass + take the
    // on-time phase down. The output dot is sampled on the even 2x phase
    // (after pushing z0, before pushing z1), so the cascade delay is the
    // integer (kOsTaps-1)/2 host samples the latency API reports; sampling
    // the odd phase would sit the whole path half a sample early.
    // Delay lines are newest-at-[0].
    float processOversampled (float x)
    {
        firPush (osUpD_, x);
        const float y0 = 2.0f * firDot (osFir_, osUpD_);
        firPush (osUpD_, 0.0f);
        const float y1 = 2.0f * firDot (osFir_, osUpD_);
        firPush (osDnD_, color_.processSample (y0));
        const float out = firDot (osFir_, osDnD_);
        firPush (osDnD_, color_.processSample (y1));
        return out;
    }

    // One host-rate sample through the 4x color path: the 2x stage above is
    // cascaded 2x->2x with the same taps (up1 at the 2x rate, up2/down2 at
    // the 4x rate, dn1 at the 2x rate; x2 per zero-stuff doubling, downs
    // unscaled). Each decimation samples the even phase (push even, dot,
    // push odd), so the cascade delay is the integer 20+10+10+20 = 60 host
    // samples kOsLatency4xSamples reports. Delay lines newest-at-[0];
    // dedicated 4x lines so the 2x path is never disturbed.
    float processOversampled4x (float x)
    {
        firPush (os4Up1D_, x);
        const float u0 = 2.0f * firDot (osFir_, os4Up1D_);
        firPush (os4Up1D_, 0.0f);
        const float u1 = 2.0f * firDot (osFir_, os4Up1D_);

        firPush (os4Up2D_, u0);
        const float w00 = 2.0f * firDot (osFir_, os4Up2D_);
        firPush (os4Up2D_, 0.0f);
        const float w01 = 2.0f * firDot (osFir_, os4Up2D_);
        firPush (os4Up2D_, u1);
        const float w10 = 2.0f * firDot (osFir_, os4Up2D_);
        firPush (os4Up2D_, 0.0f);
        const float w11 = 2.0f * firDot (osFir_, os4Up2D_);

        firPush (os4Dn2D_, color_.processSample (w00));
        const float v0 = firDot (osFir_, os4Dn2D_);
        firPush (os4Dn2D_, color_.processSample (w01));
        firPush (os4Dn2D_, color_.processSample (w10));
        const float v1 = firDot (osFir_, os4Dn2D_);
        firPush (os4Dn2D_, color_.processSample (w11));

        firPush (os4Dn1D_, v0);
        const float out = firDot (osFir_, os4Dn1D_);
        firPush (os4Dn1D_, v1);
        return out;
    }

    static float firDot (const std::array<float, kOsTaps>& h, const std::array<float, kOsTaps>& d)
    {
        float s = 0.0f;
        for (int i = 0; i < kOsTaps; ++i)
            s += h[i] * d[i];
        return s;
    }

    static void firPush (std::array<float, kOsTaps>& d, float v)
    {
        for (int i = kOsTaps - 1; i > 0; --i)
            d[i] = d[i - 1];
        d[0] = v;
    }

    // Builds the shared up/down lowpass taps once (Hamming-windowed sinc,
    // DC gain normalized to 1). Cutoff is a fraction of the hot rate
    // (0.25 at 2x AND at 4x, each stage's own input Nyquist), so the taps
    // are rate-independent; no audio-thread use after the ctor.
    void initOsFir ()
    {
        constexpr double fc = 0.25;
        constexpr double center = static_cast<double> (kOsTaps - 1) / 2.0;
        double sum = 0.0;
        for (int n = 0; n < kOsTaps; ++n)
        {
            const double m = static_cast<double> (n) - center;
            double h = (m == 0.0) ? 2.0 * fc : std::sin (2.0 * kPiOs_ * fc * m) / (kPiOs_ * m);
            h *= 0.54 - 0.46 * std::cos (2.0 * kPiOs_ * static_cast<double> (n) / static_cast<double> (kOsTaps - 1));
            osFir_[n] = static_cast<float> (h);
            sum += h;
        }
        for (int n = 0; n < kOsTaps; ++n)
            osFir_[n] = static_cast<float> (static_cast<double> (osFir_[n]) / sum);
    }

    // First selection (constructor): no audible past, so select directly.
    void hardSelectTone (int tone)
    {
        banks_[0].setTone (tone);
        banks_[1].setTone (tone);
        active_ = 0;
        targetTone_ = tone;
        xfading_ = false;
        xfadePos_ = 0;
    }

    double sampleRate_ = 48000.0;
    GainStage gain_;
    ToneBank banks_[2];
    ColorStage color_;
    float postHpCoeff_ = 0.0f;
    float postLp_ = 0.0f;
    // Fixed hardware tilt: RBJ sections (b = b0..b2 normalized, a = a1..a2)
    // plus TDF-II states. Cooked in setSampleRate, applied in processSample.
    float tiltB_[3][3] = {};
    float tiltA_[3][2] = {};
    float tiltZ_[3][2] = {};

    // Q-parametrized RBJ shelf/peak cook, cookbook-identical to
    // ToneBank::cook (same formulae, same normalization) so the test oracle
    // and the chain can only disagree by float rounding.
    static void cookTiltShelf (bool high, double f0, double q, double gainDb, double sampleRate, float (&b)[3],
                               float (&a)[2])
    {
        constexpr double twoPi = 6.28318530717958647692;
        const double w0 = twoPi * f0 / sampleRate;
        const double cw = std::cos (w0);
        const double sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double sq = 2.0 * std::sqrt (A) * alpha;
        double b0, b1, b2, a0, a1, a2;
        if (!high)
        {
            b0 = A * ((A + 1.0) - (A - 1.0) * cw + sq);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
            b2 = A * ((A + 1.0) - (A - 1.0) * cw - sq);
            a0 = (A + 1.0) + (A - 1.0) * cw + sq;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
            a2 = (A + 1.0) + (A - 1.0) * cw - sq;
        }
        else
        {
            b0 = A * ((A + 1.0) + (A - 1.0) * cw + sq);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
            b2 = A * ((A + 1.0) + (A - 1.0) * cw - sq);
            a0 = (A + 1.0) - (A - 1.0) * cw + sq;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
            a2 = (A + 1.0) - (A - 1.0) * cw - sq;
        }
        b[0] = static_cast<float> (b0 / a0);
        b[1] = static_cast<float> (b1 / a0);
        b[2] = static_cast<float> (b2 / a0);
        a[0] = static_cast<float> (a1 / a0);
        a[1] = static_cast<float> (a2 / a0);
    }

    // Q-parametrized RBJ peak cook, cookbook-identical to ToneBank::cook.
    static void cookTiltPeak (double f0, double q, double gainDb, double sampleRate, float (&b)[3], float (&a)[2])
    {
        constexpr double twoPi = 6.28318530717958647692;
        const double w0 = twoPi * f0 / sampleRate;
        const double cw = std::cos (w0);
        const double sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double b0 = 1.0 + alpha * A;
        const double b1 = -2.0 * cw;
        const double b2 = 1.0 - alpha * A;
        const double a0 = 1.0 + alpha / A;
        const double a1 = -2.0 * cw;
        const double a2 = 1.0 - alpha / A;
        b[0] = static_cast<float> (b0 / a0);
        b[1] = static_cast<float> (b1 / a0);
        b[2] = static_cast<float> (b2 / a0);
        a[0] = static_cast<float> (a1 / a0);
        a[1] = static_cast<float> (a2 / a0);
    }
    HighCut highcut_;
    float trimLin_ = 1.0f;
    mutable std::atomic<float> peak_{0.0f};
    mutable std::atomic<float> prePeak_{0.0f};

    int active_ = 0;
    int targetTone_ = 3;
    bool xfading_ = false;
    int xfadePos_ = 0;
    int xfadeLen_ = 480;

    static constexpr double kPiOs_ = 3.14159265358979323846;

    // Hot-factor resampler state: shared lowpass taps + per-filter delay
    // lines (fixed size, reset by fill, never allocated on the audio path).
    // The 2x path keeps its Task-18 lines; the 4x cascade has four dedicated
    // lines (up1/dn1 at the 2x rate, up2/dn2 at the 4x rate).
    std::array<float, kOsTaps> osFir_ = {};
    std::array<float, kOsTaps> osUpD_ = {};
    std::array<float, kOsTaps> osDnD_ = {};
    std::array<float, kOsTaps> os4Up1D_ = {};
    std::array<float, kOsTaps> os4Up2D_ = {};
    std::array<float, kOsTaps> os4Dn2D_ = {};
    std::array<float, kOsTaps> os4Dn1D_ = {};
    int osTarget_ = 1;
    int osActive_ = 1;
    bool osBlending_ = false;
    int osBlendFrom_ = 1;
    int osPos_ = 0;
    int osLen_ = 240; // 5ms @48k; recomputed per rate in setSampleRate.
};
