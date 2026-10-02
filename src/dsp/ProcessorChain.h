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
// ColorStage -> HighCut -> trim gain -> peak tracker.
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
// 2x oversampling around the ColorStage ONLY (ABALONE header toggle,
// default off). The tanh is the sole nonlinear stage, so it alone runs at
// 2x: zero-stuff up -> linear-phase FIR (x2 passband gain) -> the
// ColorStage transfer at 2x -> the identical FIR -> decimate. All linear
// stages stay at 1x: no wasted cycles, and biquads keep host-rate
// coefficients (no tone-shape change with rate doubling).
//
// Filter: 33-tap windowed-sinc lowpass (Hamming, fc = 0.23 cycles/sample
// at the 2x rate; host Nyquist sits at 0.25), DC gain normalized to 1.
// Each FIR delays (33-1)/2 samples at the 2x rate = (33-1)/4 at the host
// rate; up + down in series = (33-1)/2 = 16 host samples, rate-independent
// (the cutoff is a fraction of the 2x rate, so the same taps serve 44.1k
// and 48k). Reported via getLatencySamples() for the processor's
// setLatencySamples; the ChainTest impulse gate pins reported == measured
// peak == kOsLatencySamples.
//
// Toggle: setOversampled() arms a 5ms equal-power crossfade (cos/sin, the
// tone-xfade idiom) between the 1x and 2x paths from the next sample; the
// processor calls it once per block, so the switch lands on a block
// boundary, never mid-block. The FIR states are zeroed when a blend lands
// back on 1x (never at arm: wiping at arm would collapse the live 2x lines
// the fade-out source still reads), so every 1x->2x entry starts clean and
// the fade covers the entry transient either way. 1x steady output is
// exactly the old direct path (byte-identical default behavior).
struct ProcessorChain
{
    // 2x resampler taps (windowed-sinc lowpass, see note above) and the
    // exact 2x-path group delay in host samples: each FIR delays
    // (kOsTaps-1)/2 samples at the 2x rate = (kOsTaps-1)/4 at the host
    // rate, so up + down in series = (kOsTaps-1)/2 = 16.
    static constexpr int kOsTaps = 33;
    static constexpr int kOsLatencySamples = (kOsTaps - 1) / 2;

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
        xfadeLen_ = static_cast<int> (0.01 * sampleRate + 0.5);
        if (xfadeLen_ < 1)
            xfadeLen_ = 1;
        if (xfadePos_ > xfadeLen_)
            xfadePos_ = xfadeLen_;
        // Rate change flushes the 2x delay lines (content is signal
        // history, meaningless across rates); taps are rate-independent.
        osUpD_.fill (0.0f);
        osDnD_.fill (0.0f);
        osLen_ = static_cast<int> (0.005 * sampleRate + 0.5);
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

    // 2x toggle for the ColorStage path. Arms the 5ms equal-power xfade
    // from the other path; a no-op when already targeted. Called once per
    // block by the processor (never mid-block). The FIR states are NOT
    // wiped here: wiping at arm would collapse the live 2x delay lines
    // that the fade-out source is still reading (a full-scale jump, caught
    // by the toggle-click gate). They are zeroed when a blend lands back
    // on 1x instead, so every 1x->2x entry still starts from clean states
    // and the fade covers the entry transient either way.
    void setOversampled (bool oversampled)
    {
        if (oversampled == osTarget_)
            return;
        osTarget_ = oversampled;
        osBlendFrom_ = !osTarget_;
        osPos_ = 0;
        osBlending_ = true;
    }

    bool isOversampled () const { return osTarget_; }

    // Latency to report to the DAW: 0 at 1x, the exact FIR group delay at
    // 2x. Follows the toggle target immediately (the 5ms entry blend is
    // transient; the steady path carries the full delay).
    int getLatencySamples () const { return osTarget_ ? kOsLatencySamples : 0; }

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
        const float cut = highcut_.processSample (colored);
        const float out = cut * trimLin_;

        // Pre-trim tap (post-HighCut, pre-trim-gain peak) for the SIGNAL LED:
        // the hardware has no trim so its LED can't see one; the pre-trim tap
        // keeps Boost staging readable at -18dBFS workflows. Tracked alongside
        // (never instead of) the legacy post-trim peak below, so existing
        // peak-tracker behavior is unchanged.
        const float preMag = std::fabs (cut);
        if (preMag > prePeak_.load (std::memory_order_relaxed))
            prePeak_.store (preMag, std::memory_order_relaxed);

        const float mag = std::fabs (out);
        if (mag > peak_.load (std::memory_order_relaxed))
            peak_.store (mag, std::memory_order_relaxed);
        return out;
    }

    // Max |post-trim| since the last call; resets to 0 on read. Legacy tap,
    // kept for the unit-tested peak-tracker behavior; the editor SIGNAL LED
    // reads getLastPreTrimPeak() instead (drained here alongside).
    float getLastPeak () const { return peak_.exchange (0.0f, std::memory_order_relaxed); }

    // Max |pre-trim| (post-HighCut, pre-trim-gain) since the last call;
    // resets to 0 on read. This is the SIGNAL LED tap in normal operation.
    float getLastPreTrimPeak () const { return prePeak_.exchange (0.0f, std::memory_order_relaxed); }

private:
    // Color section with the 2x toggle path. Steady state runs exactly one
    // path (1x direct = the historical code path); during the 5ms entry
    // blend both run and mix equal-power, old at weight cos, new at sin.
    float colorWithOsBlend (float shaped)
    {
        if (!osBlending_)
            return osActive_ ? processOversampled (shaped) : color_.processSample (shaped);

        const float yDirect = color_.processSample (shaped);
        const float yOs = processOversampled (shaped);
        const float yOld = osBlendFrom_ ? yOs : yDirect;
        const float yNew = osBlendFrom_ ? yDirect : yOs;
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
            // Leaving 2x: zero the delay lines now that nothing reads them,
            // so the next 1x->2x entry starts clean (see setOversampled).
            if (!osTarget_)
            {
                osUpD_.fill (0.0f);
                osDnD_.fill (0.0f);
            }
        }
        return out;
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
    // DC gain normalized to 1). Cutoff is a fraction of the 2x rate, so
    // the taps are rate-independent; no audio-thread use after the ctor.
    void initOsFir ()
    {
        constexpr double fc = 0.23;
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

    // 2x resampler state: shared lowpass taps + per-filter delay lines
    // (fixed size, reset by fill, never allocated on the audio path).
    std::array<float, kOsTaps> osFir_ = {};
    std::array<float, kOsTaps> osUpD_ = {};
    std::array<float, kOsTaps> osDnD_ = {};
    bool osTarget_ = false;
    bool osActive_ = false;
    bool osBlending_ = false;
    bool osBlendFrom_ = false;
    int osPos_ = 0;
    int osLen_ = 240; // 5ms @48k; recomputed per rate in setSampleRate.
};
