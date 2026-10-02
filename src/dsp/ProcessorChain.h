#pragma once

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
struct ProcessorChain
{
    ProcessorChain ()
    {
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

        const float colored = color_.processSample (shaped);
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
};
