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
// (<cmath> + <array> + <atomic>); owns every DSP stage so CTest can drive the
// full chain without a plugin host. The processor delegates per sample.
//
// Chain order: GainStage (boost + 5Hz DC-block) -> ToneBank A/B xfade ->
// ColorStage -> post-color 2Hz DC-block -> HighCut -> trim gain ->
// fixed hardware voicing (LS120/HS8k successors, engaged only) -> peak tracker.
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
// Threading contract (audio-thread-only): the processor calls every setter
// from processBlock/prepareToPlay on the audio thread. setSampleRate() and
// reset() additionally require audio stopped (the host guarantees prepare is
// never concurrent with process); never call any method concurrently with
// processSample(). reset() itself allocates nothing, but like prepare it is
// a stopped-audio call: it rebuilds the Gain/HighCut stages.
//
// No heap in the audio path: fixed members only. Trim is a slewed gain (a
// one-pole follower toward the setTrimDb target, GainStage idiom). Peak is
// max |post-trim| since the last getLastPeak() call.
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
        // Hot-rate retune per factor instance (see armOsFactor): every
        // instance keeps its own rate's taus, so nothing retunes on factor
        // switches and re-prepare cannot strand the wrong timing (hosts
        // re-prepare on every transport start).
        color1x_.setSampleRate (sampleRate);
        color2x_.setSampleRate (sampleRate * 2.0);
        color4x_.setSampleRate (sampleRate * 4.0);
        banks_[0].setSampleRate (sampleRate);
        banks_[1].setSampleRate (sampleRate);
        highcut_.setSampleRate (sampleRate);
        // Post-color DC-block retune (same smoother family as GainStage).
        // 2Hz not 5Hz: gates pass as-written, DC dies the same (corner only sets ~0.4s settling).
        constexpr double postFc = 2.0;
        constexpr double postTwoPi = 6.28318530717958647692;
        postHpCoeff_ = static_cast<float> (1.0 - std::exp (-postTwoPi * postFc / sampleRate_));
        // Fixed hardware voicing (engaged path only — ACTIVE-off passthrough
        // returns before processSample's tail, so bypass stays bit-exact):
        // LS 75Hz +0.91dB Q0.54 + HS 5441Hz -0.34dB Q0.62 + peak 180Hz
        // +0.11dB Q1.97, Q-parametrized RBJ (same cookbook as
        // ToneBank::cook; the ChainTest voicing oracle pins it).
        cookVoiceShelf (false, 75.0, 0.54, 0.91, sampleRate_, voiceB_[0], voiceA_[0]);
        cookVoiceShelf (true, 5441.0, 0.62, -0.34, sampleRate_, voiceB_[1], voiceA_[1]);
        cookVoicePeak (180.0, 1.97, 0.11, sampleRate_, voiceB_[2], voiceA_[2]);
        // Trim follower retune (GainStage 10Hz idiom: ~16ms TC, ~80ms to
        // settle; rate changes keep the current gain, only the coeff retunes).
        constexpr double trimFc = 10.0;
        constexpr double trimTwoPi = 6.28318530717958647692;
        trimCoeff_ = static_cast<float> (1.0 - std::exp (-trimTwoPi * trimFc / sampleRate_));
        xfadeLen_ = static_cast<int> (std::lround (0.01 * sampleRate));
        if (xfadeLen_ < 1)
            xfadeLen_ = 1;
        // Rate change flushes all state history (content is signal history,
        // meaningless across rates); taps/coeffs above are rate-retuned and
        // every setting (boost/tone/highcut/color/trim-target/OS-target) is
        // preserved by reset().
        osLen_ = static_cast<int> (std::lround (0.005 * sampleRate));
        if (osLen_ < 1)
            osLen_ = 1;
        reset();
    }

    void setBoostStep (int step) { gain_.setStep (step); }

    // Full state clear (transport restart / prepare): every stage state to
    // zero, entry blends cancelled, all settings preserved (boost step, tone
    // target, highcut/color enables, trim target, OS target). Called at the
    // end of setSampleRate; prepareToPlay/transport paths should call it
    // AFTER pushing params (boost/tone/highcut/trim/osfactor) so the chain
    // starts settled (trim/gain followers snap to target, no fade-in).
    // Audio-stopped call (see threading contract above), never concurrent
    // with processSample.
    void reset ()
    {
        // GainStage exposes no state reset: rebuild from fresh, restore rate
        // + step, then slew-settle the gain follower to target with zeros
        // (lp_ stays exactly 0 throughout — zeros never disturb it).
        const int step = static_cast<int> (std::lround (gain_.getDb() / 3.0f));
        gain_ = GainStage();
        gain_.setSampleRate (sampleRate_);
        gain_.setStep (step);
        const int settle = static_cast<int> (0.5 * sampleRate_) + 16;
        for (int i = 0; i < settle; ++i)
            gain_.processSample (0.0f);
        // ToneBank states clear on select: re-select the target on both banks
        // (also parks the fade state — reset means no audible past).
        hardSelectTone (targetTone_);
        // Per-factor color envelopes (the only ColorStage state).
        color1x_.reset();
        color2x_.reset();
        color4x_.reset();
        // HighCut exposes no state reset: rebuild + restore rate + enable.
        highcut_ = HighCut();
        highcut_.setSampleRate (sampleRate_);
        highcut_.setEnabled (highcutOn_);
        // Chain-owned states.
        postLp_ = 0.0f;
        for (int s = 0; s < 3; ++s)
            voiceZ_[s][0] = voiceZ_[s][1] = 0.0f;
        osUpD_.fill (0.0f);
        osDnD_.fill (0.0f);
        os4Up1D_.fill (0.0f);
        os4Up2D_.fill (0.0f);
        os4Dn2D_.fill (0.0f);
        os4Dn1D_.fill (0.0f);
        // Start settled (GainStage ctor idiom): no fade-in after reset.
        trimLin_ = trimTargetLin_;
        // Cancel entry blends at their targets (no audible past to blend).
        osBlending_ = false;
        osActive_ = osTarget_;
        osPos_ = 0;
        hasOsPending_ = false;
        peak_.store (0.0f, std::memory_order_release);
        prePeak_.store (0.0f, std::memory_order_release);
    }

    // Tone 0-6, 0 = bypass. Out-of-range values clamp. Arms the 10ms
    // equal-power xfade; a no-op when the tone is already targeted.
    // Retarget mid-fade defers (latest wins): the new value parks in
    // tonePending_ and arms at landing, so the shadow is never restarted
    // mid-flight (that restart stepped ~1.8x). Setting the fading-in
    // target again cancels the pending change. A retarget before the fade
    // consumes any sample (xfadePos_ == 0) still re-arms immediately:
    // nothing was output under the new blend, so there is nothing to be
    // discontinuous about — this keeps back-to-back synchronous calls
    // (compat shim, prepare sequences) exact.
    void setTone (int tone)
    {
        if (tone < 0)
            tone = 0;
        if (tone > 6)
            tone = 6;
        if (tone == targetTone_)
        {
            hasTonePending_ = false;
            return;
        }
        if (xfading_ && xfadePos_ > 0)
        {
            tonePending_ = tone;
            hasTonePending_ = true;
            return;
        }
        armTone (tone);
    }

    int getToneTarget () const { return targetTone_; }

    void setHighcut (bool enabled)
    {
        highcutOn_ = enabled; // tracked: reset() rebuilds HighCut (no state reset API).
        highcut_.setEnabled (enabled);
    }

    void setColorEnabled (bool enabled)
    {
        color1x_.setEnabled (enabled);
        color2x_.setEnabled (enabled);
        color4x_.setEnabled (enabled);
    }

    // Oversample factor for the ColorStage path (1, 2, or 4; anything else
    // clamps to the nearest valid factor, ties (3) round up — explicit, no
    // silent fall-through). Arms the 5ms equal-power xfade from the
    // previous target; a no-op when already targeted. Called once per block
    // by the processor (never mid-block). Retarget mid-blend defers like
    // setTone (latest wins, cancels on set-back, arms at landing — the
    // mid-flight source swap stepped ~4.4x). Latency reporting follows the
    // LIVE path (osActive_), never the armed or pending one, so the host
    // only compensates the delay the output actually carries: the report
    // flips at blend landing, not at arm. A retarget before the blend consumes any sample
    // (osPos_ == 0) still re-arms immediately, same discontinuity-free
    // reasoning as setTone above. The FIR states are NOT wiped here
    // (see note above); they are zeroed on landing for every factor except
    // the landing target instead.
    void setOsFactor (int factor)
    {
        int target;
        if (factor <= 1)
            target = 1;
        else if (factor == 2)
            target = 2;
        else
            target = 4; // 3 rounds up, >= 4 clamps to 4.
        if (target == osTarget_)
        {
            hasOsPending_ = false;
            return;
        }
        if (osBlending_ && osPos_ > 0)
        {
            osPending_ = target;
            hasOsPending_ = true;
            return;
        }
        armOsFactor (target);
    }

    int getOsFactor () const { return osTarget_; }

    // Test tap: the ColorStage instance behind a factor leg (pins the
    // no-cross-slew structure — each leg owns its envelope; the gate below
    // asserts pairwise-distinct instances).
    const ColorStage& colorLegForTest (int factor) const
    {
        return (factor == 4) ? color4x_ : (factor == 2) ? color2x_ : color1x_;
    }

    // Test tap: the color envelope release coefficient at the live factor
    // (pins hot-rate timing across re-prepare — the re-prepare regression
    // had no observable gate without it).
    float colorEnvReleaseCoeff () const
    {
        return (osTarget_ == 4)   ? color4x_.envReleaseCoeff()
               : (osTarget_ == 2) ? color2x_.envReleaseCoeff()
                                  : color1x_.envReleaseCoeff();
    }

    // Task-18 compat shim: false->1x, true->2x.
    void setOversampled (bool oversampled) { setOsFactor (oversampled ? 2 : 1); }

    bool isOversampled () const { return osTarget_ != 1; }

    // Latency to report to the DAW: 0 at 1x, the exact FIR group delay at
    // 2x (40) or 4x (60). Follows the live path while blending (osActive_)
    // and the target once settled (identical then): the 5ms entry blend is
    // mostly the old path, so reporting the armed target early would have
    // the host compensate a delay the output does not yet carry.
    int getLatencySamples () const
    {
        const int live = osBlending_ ? osActive_ : osTarget_;
        return (live == 4) ? kOsLatency4xSamples : (live == 2) ? kOsLatencySamples : 0;
    }

    void setTrimDb (float trimDb)
    {
        // Guard: pow(10, trimDb/20) turns NaN/Inf/1e30 into NaN/Inf, which
        // latches every downstream state. Clamp to the APVTS range
        // [-32,+6]dB; non-finite falls back to the -3dB default. Target only:
        // the per-sample gain slews toward it (see processSample), so knob
        // twists ramp instead of zipping.
        if (!std::isfinite (trimDb))
            trimDb = -3.0f;
        if (trimDb < -32.0f)
            trimDb = -32.0f;
        else if (trimDb > 6.0f)
            trimDb = 6.0f;
        trimTargetLin_ = std::pow (10.0f, trimDb / 20.0f);
    }

    float processSample (float x)
    {
        // Chain-entry sanitize: a non-finite sample (sick host) becomes
        // silence HERE, so no stage state (GainStage lp_, biquad z1/z2,
        // HighCut, voicing, post blocker) can ever latch NaN — one choke
        // point where all channels route through beats a guard in every
        // stage, and no state reset is needed: states never learn garbage,
        // so output recovers on the very next finite sample.
        // Huge-finite clamp (+/-120 dBFS): magnitudes past 1e6 can never be
        // real signal (documented max is 31.6); without it the x31.6 boost
        // overflows to Inf and latches every downstream state the same way.
        // 1e6 keeps all hot behavior bit-identical (saturation lives <= 50).
        if (!std::isfinite (x))
            x = 0.0f;
        else if (x > 1.0e6f)
            x = 1.0e6f;
        else if (x < -1.0e6f)
            x = -1.0e6f;
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
                // Deferred retarget: arm the latest pending tone (or none).
                // The just-landed bank becomes the new shadow from a clean
                // state — same idiom as the initial arm, never a restart.
                if (hasTonePending_)
                {
                    hasTonePending_ = false;
                    if (tonePending_ != targetTone_)
                        armTone (tonePending_);
                }
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
        // Slewed trim (GainStage idiom, fc = 10Hz): preset/knob jumps ramp
        // over ~80ms instead of multiplying instantly (zipper). Snaps below
        // 1e-6 linear, so post-settle gain is exact even at the -32dB floor.
        trimLin_ += trimCoeff_ * (trimTargetLin_ - trimLin_);
        if (std::fabs (trimTargetLin_ - trimLin_) < 1.0e-6f)
            trimLin_ = trimTargetLin_;
        const float out = cut * trimLin_;
        // Fixed hardware voicing (see setSampleRate): three TDF-II sections
        // (LS + HS + low-mid peak). Denormal snap mirrors ToneBank (states
        // freeze exact at silence).
        float voiced = out;
        for (int s = 0; s < 3; ++s)
        {
            const float y = voiceB_[s][0] * voiced + voiceZ_[s][0];
            voiceZ_[s][0] = voiceB_[s][1] * voiced - voiceA_[s][0] * y + voiceZ_[s][1];
            voiceZ_[s][1] = voiceB_[s][2] * voiced - voiceA_[s][1] * y;
            if (std::fabs (voiceZ_[s][0]) < 1.0e-15f)
                voiceZ_[s][0] = 0.0f;
            if (std::fabs (voiceZ_[s][1]) < 1.0e-15f)
                voiceZ_[s][1] = 0.0f;
            voiced = y;
        }

        // Pre-trim tap (post-HighCut, pre-trim-gain peak) for the SIGNAL LED:
        // the hardware has no trim so its LED can't see one; the pre-trim tap
        // keeps Boost staging readable at -18dBFS workflows. Tracked alongside
        // (never instead of) the legacy post-trim peak below, so existing
        // peak-tracker behavior is unchanged.
        const float preMag = std::fabs (cut);
        if (preMag > prePeak_.load (std::memory_order_acquire))
            prePeak_.store (preMag, std::memory_order_release);

        const float mag = std::fabs (voiced);
        if (mag > peak_.load (std::memory_order_acquire))
            peak_.store (mag, std::memory_order_release);
        return voiced;
    }

    // Max |post-trim| since the last call; resets to 0 on read. Legacy tap,
    // kept for the unit-tested peak-tracker behavior; the editor SIGNAL LED
    // reads getLastPreTrimPeak() instead (drained here alongside).
    float getLastPeak () const { return peak_.exchange (0.0f, std::memory_order_acq_rel); }

    // Max |pre-trim| (post-HighCut, pre-trim-gain) since the last call;
    // resets to 0 on read. This is the SIGNAL LED tap in normal operation.
    float getLastPreTrimPeak () const { return prePeak_.exchange (0.0f, std::memory_order_acq_rel); }

private:
    // Color section with the hot-factor path. Steady state runs exactly one
    // path (1x direct = the historical code path); during the 5ms entry
    // blend both run and mix equal-power, old at weight cos, new at sin.
    // Each leg runs its own factor instance (never one shared stage), so the
    // fade-out leg keeps its own rate's taus and update cadence for the whole
    // blend — no cross-slew between legs.
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
            // Deferred retarget: arm the latest pending factor (or none).
            if (hasOsPending_)
            {
                hasOsPending_ = false;
                if (osPending_ != osTarget_)
                    armOsFactor (osPending_);
            }
        }
        return out;
    }

    float processFactor (int factor, float x)
    {
        return (factor == 4)   ? processOversampled4x (x)
               : (factor == 2) ? processOversampled (x)
                               : color1x_.processSample (x);
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
        firPush (osDnD_, color2x_.processSample (y0));
        const float out = firDot (osFir_, osDnD_);
        firPush (osDnD_, color2x_.processSample (y1));
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

        firPush (os4Dn2D_, color4x_.processSample (w00));
        const float v0 = firDot (osFir_, os4Dn2D_);
        firPush (os4Dn2D_, color4x_.processSample (w01));
        firPush (os4Dn2D_, color4x_.processSample (w10));
        const float v1 = firDot (osFir_, os4Dn2D_);
        firPush (os4Dn2D_, color4x_.processSample (w11));

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
        hasTonePending_ = false;
    }

    // Arm helpers: the single place a tone/OS change takes effect, used by
    // both the immediate path and the deferred landing path.
    void armTone (int tone)
    {
        targetTone_ = tone;
        banks_[1 - active_].setTone (tone); // shadow starts clean.
        xfadePos_ = 0;
        xfading_ = true;
    }

    void armOsFactor (int target)
    {
        osBlendFrom_ = osTarget_;
        osTarget_ = target;
        osPos_ = 0;
        osBlending_ = true;
        // No color retune here: each factor owns a permanently hot-tuned
        // instance (see setSampleRate), so both blend legs run correct taus
        // for the whole blend. The old shared-instance retune stranded the
        // fade-out leg on the target rate and churned one envelope 3-6x per
        // sample mid-blend.
    }

    double sampleRate_ = 48000.0;
    GainStage gain_;
    ToneBank banks_[2];
    // Per-factor color instances (no-cross-slew fix): each factor's path —
    // including its blend leg — runs its own envelope at its own hot-rate
    // taus, so the 1x leg does 1 update/sample at 1x timing while the 2x leg
    // does 2 at 2x timing, never churning shared state. Instances off the
    // live path sit out (their envelopes go stale, not wrong); the 5ms entry
    // blend covers the entry transient either way, same as the FIR lines.
    // Only the existing ColorStage interface is used (ctor/reset/
    // processSample/setSampleRate/setEnabled) — no new API.
    ColorStage color1x_;
    ColorStage color2x_;
    ColorStage color4x_;
    float postHpCoeff_ = 0.0f;
    float postLp_ = 0.0f;
    // Fixed hardware voicing: RBJ sections (b = b0..b2 normalized, a = a1..a2)
    // plus TDF-II states. Cooked in setSampleRate, applied in processSample.
    float voiceB_[3][3] = {};
    float voiceA_[3][2] = {};
    float voiceZ_[3][2] = {};

    // Q-parametrized RBJ shelf/peak cook, cookbook-identical to
    // ToneBank::cook (same formulae, same normalization) so the test oracle
    // and the chain can only disagree by float rounding.
    static void cookVoiceShelf (bool high, double f0, double q, double gainDb, double sampleRate, float (&b)[3],
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
    static void cookVoicePeak (double f0, double q, double gainDb, double sampleRate, float (&b)[3], float (&a)[2])
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
    bool highcutOn_ = false; // tracked: HighCut has no state reset or getter.
    float trimLin_ = 1.0f;
    float trimTargetLin_ = 1.0f; // setTrimDb target; trimLin_ slews toward it.
    float trimCoeff_ = 0.0f;     // one-pole follower coeff, retuned per rate.
    mutable std::atomic<float> peak_{0.0f};
    mutable std::atomic<float> prePeak_{0.0f};

    int active_ = 0;
    int targetTone_ = 3;
    bool xfading_ = false;
    int xfadePos_ = 0;
    int xfadeLen_ = 480;
    // Deferred tone retarget (latest wins, armed at landing).
    int tonePending_ = 3;
    bool hasTonePending_ = false;

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
    // Deferred OS retarget (latest wins, armed at landing).
    int osPending_ = 1;
    bool hasOsPending_ = false;
};
