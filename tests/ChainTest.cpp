// Abalone W5 - U5-inspired clean bass DI.
// Copyright (C) 2026 Sakdinat2548.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "../src/dsp/ProcessorChain.h"

#include <cassert>
#include <cmath>
#include <complex>
#include <cstdio>

namespace
{

constexpr double kPi = 3.14159265358979323846;

// One-pole highpass loss in dB (same smoother family as the chain's
// DC-blockers; mirrors the audit oracle below).
float blockerTheoryDb (double fc, double freqHz, double sampleRate)
{
    constexpr double twoPi = 6.28318530717958647692;
    const double a = 1.0 - std::exp (-twoPi * fc / sampleRate);
    const double w = twoPi * freqHz / sampleRate;
    const std::complex<double> z = std::exp (std::complex<double> (0.0, -w));
    const std::complex<double> h = 1.0 - a / (1.0 - (1.0 - a) * z);
    return static_cast<float> (20.0 * std::log10 (std::abs (h)));
}

// Tilt oracle (fitted hardware tilt, engaged path only): LS 75Hz +0.91dB
// Q0.54 + HS 5441Hz -0.34dB Q0.62 + peak 180Hz +0.11dB Q1.97,
// Q-parametrized RBJ (same cookbook family as ToneBank::cook). Theoretical
// magnitude in dB — an independent frequency-domain path from the chain's
// time-domain biquads, same pattern as the blocker-loss oracle in
// audit10HzAnchors below.
float tiltTheoryDb (double freqHz, double sampleRate)
{
    constexpr double twoPi = 6.28318530717958647692;
    auto section = [&] (int kind, double f0, double q, double gainDb)
    {
        const double w0 = twoPi * f0 / sampleRate;
        const double cw = std::cos (w0);
        const double sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double sq = 2.0 * std::sqrt (A) * alpha;
        double b0, b1, b2, a0, a1, a2;
        if (kind == 0) // low shelf
        {
            b0 = A * ((A + 1.0) - (A - 1.0) * cw + sq);
            b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
            b2 = A * ((A + 1.0) - (A - 1.0) * cw - sq);
            a0 = (A + 1.0) + (A - 1.0) * cw + sq;
            a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cw);
            a2 = (A + 1.0) + (A - 1.0) * cw - sq;
        }
        else if (kind == 1) // high shelf
        {
            b0 = A * ((A + 1.0) + (A - 1.0) * cw + sq);
            b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
            b2 = A * ((A + 1.0) + (A - 1.0) * cw - sq);
            a0 = (A + 1.0) - (A - 1.0) * cw + sq;
            a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cw);
            a2 = (A + 1.0) - (A - 1.0) * cw - sq;
        }
        else // peak
        {
            b0 = 1.0 + alpha * A;
            b1 = -2.0 * cw;
            b2 = 1.0 - alpha * A;
            a0 = 1.0 + alpha / A;
            a1 = -2.0 * cw;
            a2 = 1.0 - alpha / A;
        }
        const double w = twoPi * freqHz / sampleRate;
        const std::complex<double> z = std::exp (std::complex<double> (0.0, -w));
        const std::complex<double> h = (b0 + b1 * z + b2 * z * z) / (a0 + a1 * z + a2 * z * z);
        return 20.0 * std::log10 (std::abs (h));
    };
    return static_cast<float> (section (0, 75.0, 0.54, 0.91) + section (1, 5441.0, 0.62, -0.34) +
                               section (2, 180.0, 1.97, 0.11));
}

// (a) End-to-end engaged-flat (tone 0, highcut off, boost step 1 (+3dB),
// trim 0): output sits on +3dB PLUS the fitted hardware tilt (LS75 +0.91 /
// HS5441 -0.34 + PK180 +0.11, engaged path only — ACTIVE-off passthrough
// stays bit-exact, see ProcessorIOTest). 1kHz within +/-0.1dB, 20Hz-15kHz
// spots within +/-0.2dB of theory. 5Hz is the DC-block corner (-3dB by
// design, see GainStage.h), so it asserts the block instead of flatness.
constexpr float kStep1Db = 3.0f;

float rms (const float* data, int start, int count)
{
    double sum = 0.0;
    for (int i = 0; i < count; ++i)
        sum += static_cast<double> (data[start + i]) * static_cast<double> (data[start + i]);
    return static_cast<float> (std::sqrt (sum / static_cast<double> (count)));
}

void fillSine (float* data, int count, double sampleRate, double freqHz, float amp)
{
    for (int i = 0; i < count; ++i)
        data[i] = amp * static_cast<float> (std::sin (2.0 * kPi * freqHz * static_cast<double> (i) / sampleRate));
}

// Steady-state gain in dB of `chain` at `freqHz` (1s settle + 1s measure).
float steadyGainDb (ProcessorChain& chain, double sampleRate, double freqHz, float amp)
{
    const int total = static_cast<int> (sampleRate * 2.0);
    const int skip = static_cast<int> (sampleRate);
    static float in[96000];
    static float out[96000];
    assert (total <= 96000);
    fillSine (in, total, sampleRate, freqHz, amp);
    for (int i = 0; i < total; ++i)
        out[i] = chain.processSample (in[i]);
    return 20.0f * std::log10 (rms (out, skip, total - skip) / rms (in, skip, total - skip));
}

void checkBypassFlat ()
{
    const double rates[2] = {48000.0, 44100.0};
    const double spots[6] = {20.0, 100.0, 1000.0, 5000.0, 10000.0, 15000.0};
    for (int r = 0; r < 2; ++r)
    {
        const double sampleRate = rates[r];
        for (int f = 0; f < 6; ++f)
        {
            ProcessorChain chain;
            chain.setSampleRate (sampleRate);
            chain.setBoostStep (1);
            chain.setTone (0);
            chain.setHighcut (false);
            chain.setTrimDb (0.0f);
            const float gainDb = steadyGainDb (chain, sampleRate, spots[f], 0.5f);
            // Expectation models the full engaged tail: boost + tilt +
            // both DC-blockers (5Hz in, 2Hz post-color).
            const float expectDb = kStep1Db + tiltTheoryDb (spots[f], sampleRate) +
                                   blockerTheoryDb (5.0, spots[f], sampleRate) +
                                   blockerTheoryDb (2.0, spots[f], sampleRate);
            const float tol = spots[f] == 1000.0 ? 0.1f : 0.2f;
            std::printf ("engaged-flat @ %.0fHz / %.0fHz: %+0.3fdB (expect %+0.3f +/- %0.1f)\n", sampleRate, spots[f],
                         gainDb, expectDb, tol);
            std::fflush (stdout);
            assert (std::fabs (gainDb - expectDb) < tol);
        }

        // 5Hz corner: the DC-block highpass owns this point (-3dB by design).
        ProcessorChain chain;
        chain.setSampleRate (sampleRate);
        chain.setBoostStep (1);
        chain.setTone (0);
        chain.setHighcut (false);
        chain.setTrimDb (0.0f);
        const float cornerDb = steadyGainDb (chain, sampleRate, 5.0, 0.5f);
        std::printf ("dc-block corner @ %.0fHz / 5Hz: %+0.3fdB (expect ~+0.0, i.e. +3dB boost minus ~3dB HP)\n",
                     sampleRate, cornerDb);
        assert (cornerDb > -1.0f && cornerDb < 1.0f);
    }
}

// (b) Tone-switch no-click: steady 220Hz bass sine, switch 3->2 mid-stream;
// no sample-to-sample jump above 0.3x local RMS inside the 50ms window.
void checkToneSwitchNoClick ()
{
    ProcessorChain chain;
    chain.setSampleRate (48000.0);
    chain.setBoostStep (1);
    chain.setTone (3);
    chain.setHighcut (false);
    chain.setTrimDb (0.0f);

    const int settle = 48000;
    for (int i = 0; i < settle; ++i)
        chain.processSample (0.5f *
                             static_cast<float> (std::sin (2.0 * kPi * 220.0 * static_cast<double> (i) / 48000.0)));

    float prev = chain.processSample (
        0.5f * static_cast<float> (std::sin (2.0 * kPi * 220.0 * static_cast<double> (settle) / 48000.0)));

    chain.setTone (2);

    const int window = 2400; // 50ms @48k.
    static float buf[2400];
    float maxJump = 0.0f;
    for (int i = 0; i < window; ++i)
    {
        const float x =
            0.5f * static_cast<float> (std::sin (2.0 * kPi * 220.0 * static_cast<double> (settle + 1 + i) / 48000.0));
        buf[i] = chain.processSample (x);
        const float jump = std::fabs (buf[i] - prev);
        if (jump > maxJump)
            maxJump = jump;
        prev = buf[i];
    }

    const float localRms = rms (buf, 0, window);
    std::printf ("tone-switch 3->2: maxJump %0.5f, localRMS %0.4f, limit %0.4f\n", maxJump, localRms, 0.3f * localRms);
    assert (maxJump < 0.3f * localRms);
}

// (c) Boost monotonicity + trim: step 10 vs step 1 = +27dB +/-0.3dB;
// trim +6dB measures +6dB +/-0.2dB. Small amp keeps ColorStage linear.
void checkBoostAndTrim ()
{
    ProcessorChain lo;
    lo.setSampleRate (48000.0);
    lo.setBoostStep (1);
    lo.setTone (0);
    lo.setHighcut (false);
    lo.setTrimDb (0.0f);
    const float loDb = steadyGainDb (lo, 48000.0, 1000.0, 0.05f);

    ProcessorChain hi;
    hi.setSampleRate (48000.0);
    hi.setBoostStep (10);
    hi.setTone (0);
    hi.setHighcut (false);
    hi.setTrimDb (0.0f);
    const float hiDb = steadyGainDb (hi, 48000.0, 1000.0, 0.05f);

    std::printf ("boost 10-vs-1: %+0.3fdB (expect +27.0 +/- 0.3)\n", hiDb - loDb);
    assert (std::fabs ((hiDb - loDb) - 27.0f) < 0.3f);

    ProcessorChain flat;
    flat.setSampleRate (48000.0);
    flat.setBoostStep (1);
    flat.setTone (0);
    flat.setHighcut (false);
    flat.setTrimDb (0.0f);
    const float refDb = steadyGainDb (flat, 48000.0, 1000.0, 0.1f);

    ProcessorChain hot;
    hot.setSampleRate (48000.0);
    hot.setBoostStep (1);
    hot.setTone (0);
    hot.setHighcut (false);
    hot.setTrimDb (6.0f);
    const float gotDb = steadyGainDb (hot, 48000.0, 1000.0, 0.1f);

    std::printf ("trim +6dB: %+0.3fdB (expect +6.0 +/- 0.2)\n", gotDb - refDb);
    assert (std::fabs ((gotDb - refDb) - 6.0f) < 0.2f);
}

// (d) Highcut end-to-end (chain-level sanity, not a Task 5 retest): 12kHz
// sine, on-vs-off. One-pole theory at 48k is -4.77dB, so the window is
// -4.8 +/- 1.0dB (the brief's -6dB assumed a steeper slope; see report).
void checkHighcutEndToEnd ()
{
    ProcessorChain off;
    off.setSampleRate (48000.0);
    off.setBoostStep (1);
    off.setTone (0);
    off.setHighcut (false);
    off.setTrimDb (0.0f);
    const float offDb = steadyGainDb (off, 48000.0, 12000.0, 0.1f);

    ProcessorChain on;
    on.setSampleRate (48000.0);
    on.setBoostStep (1);
    on.setTone (0);
    on.setHighcut (true);
    on.setTrimDb (0.0f);
    const float onDb = steadyGainDb (on, 48000.0, 12000.0, 0.1f);

    std::printf ("highcut 12kHz on-vs-off: %+0.3fdB (expect -4.8 +/- 1.0)\n", onDb - offDb);
    assert (std::fabs ((onDb - offDb) + 4.8f) < 1.0f);
}

// (e) Peak tracker: silence -> 0; driven sine -> within 5% of true peak
// (+3dB boost on 0.5 peak); reset-on-read verified.
void checkPeakTracker ()
{
    ProcessorChain chain;
    chain.setSampleRate (48000.0);
    chain.setBoostStep (1);
    chain.setTone (0);
    chain.setHighcut (false);
    chain.setTrimDb (0.0f);

    for (int i = 0; i < 1000; ++i)
        chain.processSample (0.0f);
    const float silent = chain.getLastPeak();
    std::printf ("peak silence: %0.6f (expect 0)\n", silent);
    assert (silent == 0.0f);

    for (int i = 0; i < 48000; ++i)
        chain.processSample (0.5f *
                             static_cast<float> (std::sin (2.0 * kPi * 1000.0 * static_cast<double> (i) / 48000.0)));
    for (int i = 0; i < 4800; ++i)
        chain.processSample (0.5f *
                             static_cast<float> (std::sin (2.0 * kPi * 1000.0 * static_cast<double> (i) / 48000.0)));
    const float peak = chain.getLastPeak();
    const float expected = 0.5f * std::pow (10.0f, 3.0f / 20.0f);
    std::printf ("peak driven: %0.5f (expect %0.5f +/- 5%%)\n", peak, expected);
    assert (std::fabs (peak / expected - 1.0f) < 0.05f);

    const float after = chain.getLastPeak();
    std::printf ("peak after read: %0.6f (expect 0, reset-on-read)\n", after);
    assert (after == 0.0f);
}

// (f) Oversample equivalence: 1kHz sine driven to +10dB peak at the color
// stage (boost step 1 = +3dB, so the input peak is 10^(7/20) = 2.2387).
// 1x-vs-2x fundamental must agree within +/-0.1dB (the tanh is gentle, so
// 2x is future-proofing; the gate proves the resampler is transparent).
// Correlation magnitude is phase-independent, so the 2x group delay does
// not enter the comparison.
float fundAmp (ProcessorChain& chain, double sampleRate, double freqHz, float amp)
{
    const int total = static_cast<int> (sampleRate * 2.0);
    const int skip = static_cast<int> (sampleRate);
    double re = 0.0;
    double im = 0.0;
    for (int n = 0; n < total; ++n)
    {
        const float x = amp * static_cast<float> (std::sin (2.0 * kPi * freqHz * static_cast<double> (n) / sampleRate));
        const float y = chain.processSample (x);
        if (n >= skip)
        {
            const double a = 2.0 * kPi * freqHz * static_cast<double> (n) / sampleRate;
            re += static_cast<double> (y) * std::cos (a);
            im += static_cast<double> (y) * std::sin (a);
        }
    }
    const int measured = total - skip;
    return static_cast<float> (2.0 * std::hypot (re, im) / static_cast<double> (measured));
}

void checkOsEquivalence ()
{
    const double sampleRate = 48000.0;
    const float amp = std::pow (10.0f, 7.0f / 20.0f); // +7dB in, +3dB boost -> +10dB at color.

    ProcessorChain oneX;
    oneX.setSampleRate (sampleRate);
    oneX.setBoostStep (1);
    oneX.setTone (0);
    oneX.setHighcut (false);
    oneX.setTrimDb (0.0f);
    oneX.setOversampled (false);
    const float a1 = fundAmp (oneX, sampleRate, 1000.0, amp);

    ProcessorChain twoX;
    twoX.setSampleRate (sampleRate);
    twoX.setBoostStep (1);
    twoX.setTone (0);
    twoX.setHighcut (false);
    twoX.setTrimDb (0.0f);
    twoX.setOversampled (true);
    const float a2 = fundAmp (twoX, sampleRate, 1000.0, amp);

    const float diffDb = 20.0f * std::log10 (a2 / a1);
    std::printf ("os 1x-vs-2x fundamental @1kHz/+10dB: %+0.4fdB (expect 0 +/- 0.1)\n", diffDb);
    std::fflush (stdout);
    assert (std::fabs (diffDb) < 0.1f);
}

// (g) Latency truth: impulse through a tone-bypass/highcut-off chain; the
// output peak index must equal the reported latency (0 at 1x, the FIR
// group delay at 2x), at both rates. The default (untouched) chain must
// report 1x / zero latency.
void checkOsLatencyTruth ()
{
    {
        ProcessorChain fresh;
        std::printf ("os default: oversampled=%d latency=%d (expect 0 0)\n", static_cast<int> (fresh.isOversampled()),
                     fresh.getLatencySamples());
        assert (!fresh.isOversampled());
        assert (fresh.getLatencySamples() == 0);
    }

    const double rates[2] = {48000.0, 44100.0};
    for (int r = 0; r < 2; ++r)
    {
        const double sampleRate = rates[r];
        for (int os = 0; os < 2; ++os)
        {
            ProcessorChain chain;
            chain.setSampleRate (sampleRate);
            chain.setBoostStep (1);
            chain.setTone (0);
            chain.setHighcut (false);
            chain.setTrimDb (0.0f);
            chain.setOversampled (os != 0);

            // Flush past the entry blends (tone 10ms + os 5ms) with silence
            // so the impulse below exercises the steady path only: without
            // this the 5ms crossfade mixes the undelayed 1x path into the
            // first 240 samples and the peak trivially sits at 0.
            for (int n = 0; n < 4096; ++n)
                chain.processSample (0.0f);

            const int window = 256;
            static float buf[256];
            assert (window <= 256);
            for (int n = 0; n < window; ++n)
                buf[n] = chain.processSample (n == 0 ? 0.1f : 0.0f);

            int peakIdx = 0;
            float peakMag = 0.0f;
            for (int n = 0; n < window; ++n)
            {
                const float m = std::fabs (buf[n]);
                if (m > peakMag)
                {
                    peakMag = m;
                    peakIdx = n;
                }
            }

            const int reported = chain.getLatencySamples();
            const int expected = (os != 0) ? ProcessorChain::kOsLatencySamples : 0;
            std::printf ("os latency %s @ %.0fHz: peak at %d, reported %d (expect %d)\n", os != 0 ? "2x" : "1x",
                         sampleRate, peakIdx, reported, expected);
            std::fflush (stdout);
            assert (reported == expected);
            assert (peakIdx == reported);
        }
    }
}

// (h) Oversample-toggle no-click: steady 220Hz sine, flip 1x->2x then 2x->1x
// mid-stream; no sample-to-sample jump above 0.3x local RMS in the 50ms
// window after each flip (5ms equal-power xfade, same idiom as the tone
// switch, so the same limit applies).
void checkOsToggleOneFlip (bool toOversampled)
{
    ProcessorChain chain;
    chain.setSampleRate (48000.0);
    chain.setBoostStep (1);
    chain.setTone (3);
    chain.setHighcut (false);
    chain.setTrimDb (0.0f);
    chain.setOversampled (!toOversampled);
    // Settle past the arming-blend transient so the measured flip starts
    // from a steady path.
    for (int i = 0; i < 48000; ++i)
        chain.processSample (0.5f *
                             static_cast<float> (std::sin (2.0 * kPi * 220.0 * static_cast<double> (i) / 48000.0)));

    float prev = chain.processSample (
        0.5f * static_cast<float> (std::sin (2.0 * kPi * 220.0 * static_cast<double> (48000) / 48000.0)));

    chain.setOversampled (toOversampled);

    const int window = 2400; // 50ms @48k.
    static float buf[2400];
    float maxJump = 0.0f;
    for (int i = 0; i < window; ++i)
    {
        const float x =
            0.5f * static_cast<float> (std::sin (2.0 * kPi * 220.0 * static_cast<double> (48000 + 1 + i) / 48000.0));
        buf[i] = chain.processSample (x);
        const float jump = std::fabs (buf[i] - prev);
        if (jump > maxJump)
            maxJump = jump;
        prev = buf[i];
    }

    const float localRms = rms (buf, 0, window);
    std::printf ("os-toggle %s: maxJump %0.5f, localRMS %0.4f, limit %0.4f\n", toOversampled ? "1x->2x" : "2x->1x",
                 maxJump, localRms, 0.3f * localRms);
    std::fflush (stdout);
    assert (maxJump < 0.3f * localRms);
}

void checkOsToggleNoClick ()
{
    checkOsToggleOneFlip (true);
    checkOsToggleOneFlip (false);
}

// (i) Oversample top-octave transparency: 1x-vs-2x fundamental at 15kHz and
// 20kHz, both rates, same +10dB-at-color drive as (f). The 33-tap/0.23
// prototype drooped -3.2dB@20k/48k and -10.2dB@20k/44.1k (measured linear
// cascade); the 81-tap/0.25 holds +/-0.04dB, so +/-0.5dB leaves 10x margin
// while failing the old filter decisively. Correlation magnitude is
// phase-independent, so the 2x group delay does not enter the comparison;
// test tones complete integer cycles in the 1s window, so tanh harmonics
// (aliased at 1x, filtered at 2x) stay orthogonal to the fundamental.
void checkOsTopOctave ()
{
    const double rates[2] = {48000.0, 44100.0};
    const double freqs[2] = {15000.0, 20000.0};
    const float amp = std::pow (10.0f, 7.0f / 20.0f); // +7dB in, +3dB boost -> +10dB at color.
    for (int r = 0; r < 2; ++r)
    {
        for (int f = 0; f < 2; ++f)
        {
            ProcessorChain oneX;
            oneX.setSampleRate (rates[r]);
            oneX.setBoostStep (1);
            oneX.setTone (0);
            oneX.setHighcut (false);
            oneX.setTrimDb (0.0f);
            oneX.setOversampled (false);
            const float a1 = fundAmp (oneX, rates[r], freqs[f], amp);

            ProcessorChain twoX;
            twoX.setSampleRate (rates[r]);
            twoX.setBoostStep (1);
            twoX.setTone (0);
            twoX.setHighcut (false);
            twoX.setTrimDb (0.0f);
            twoX.setOversampled (true);
            const float a2 = fundAmp (twoX, rates[r], freqs[f], amp);

            const float diffDb = 20.0f * std::log10 (a2 / a1);
            std::printf ("os top-octave 1x-vs-2x @ %.0fHz/%.0fkHz: %+0.4fdB (expect 0 +/- 0.5)\n", rates[r],
                         freqs[f] / 1000.0, diffDb);
            std::fflush (stdout);
            assert (std::fabs (diffDb) < 0.5f);
        }
    }
}

// (j) Prepare-ordering contract (processor fix round 1): prepareToPlay must
// push the `oversample` param into the chains BEFORE reading
// getLatencySamples for setLatencySamples, or a restored oversampled session
// reports 0 until the first block. Chain side of that contract: the toggle
// target survives setSampleRate, so param-push before rate-change and rate
// before param-push both report the 2x delay immediately, at both rates;
// untouched chains still report 0 (1x default path unchanged).
void checkOsPrepareOrdering ()
{
    const double rates[2] = {48000.0, 44100.0};
    for (int r = 0; r < 2; ++r)
    {
        {
            ProcessorChain chain; // push param, then rate.
            chain.setOversampled (true);
            chain.setSampleRate (rates[r]);
            std::printf ("os prepare push-then-rate @ %.0fHz: latency %d (expect %d)\n", rates[r],
                         chain.getLatencySamples(), ProcessorChain::kOsLatencySamples);
            assert (chain.getLatencySamples() == ProcessorChain::kOsLatencySamples);
        }
        {
            ProcessorChain chain; // rate, then push param.
            chain.setSampleRate (rates[r]);
            chain.setOversampled (true);
            std::printf ("os prepare rate-then-push @ %.0fHz: latency %d (expect %d)\n", rates[r],
                         chain.getLatencySamples(), ProcessorChain::kOsLatencySamples);
            assert (chain.getLatencySamples() == ProcessorChain::kOsLatencySamples);
        }
        {
            ProcessorChain chain; // 1x default still zero after rate set.
            chain.setSampleRate (rates[r]);
            std::printf ("os prepare 1x @ %.0fHz: latency %d (expect 0)\n", rates[r], chain.getLatencySamples());
            assert (chain.getLatencySamples() == 0);
        }
    }
}

// (k) 4x equivalence: 1x-vs-4x fundamental @1kHz driven to +10dB peak at
// the color stage (same drive as (f)), both rates, within +/-0.1dB. The
// 4x path cascades the proven 81-tap 2x filter (2x->2x), so the resampler
// stays transparent at 4x too. Correlation magnitude is phase-independent,
// so the 4x group delay does not enter the comparison.
void checkOs4xEquivalence ()
{
    const double rates[2] = {48000.0, 44100.0};
    const float amp = std::pow (10.0f, 7.0f / 20.0f); // +7dB in, +3dB boost -> +10dB at color.
    for (int r = 0; r < 2; ++r)
    {
        ProcessorChain oneX;
        oneX.setSampleRate (rates[r]);
        oneX.setBoostStep (1);
        oneX.setTone (0);
        oneX.setHighcut (false);
        oneX.setTrimDb (0.0f);
        oneX.setOsFactor (1);
        const float a1 = fundAmp (oneX, rates[r], 1000.0, amp);

        ProcessorChain fourX;
        fourX.setSampleRate (rates[r]);
        fourX.setBoostStep (1);
        fourX.setTone (0);
        fourX.setHighcut (false);
        fourX.setTrimDb (0.0f);
        fourX.setOsFactor (4);
        const float a2 = fundAmp (fourX, rates[r], 1000.0, amp);

        const float diffDb = 20.0f * std::log10 (a2 / a1);
        std::printf ("os 1x-vs-4x fundamental @%.0fHz/1kHz/+10dB: %+0.4fdB (expect 0 +/- 0.1)\n", rates[r], diffDb);
        std::fflush (stdout);
        assert (std::fabs (diffDb) < 0.1f);
    }
}

// (l) Factor latency truth: impulse peak == reported latency per factor per
// rate (1x->0, 2x->kOsLatencySamples=40, 4x->kOsLatency4xSamples=60). The
// default (untouched) chain is factor 1 / zero latency, and the bool compat
// API still maps false->1x, true->2x.
void checkOsFactorLatencyTruth ()
{
    {
        ProcessorChain fresh;
        std::printf ("os default: factor=%d latency=%d (expect 1 0)\n", fresh.getOsFactor(), fresh.getLatencySamples());
        assert (fresh.getOsFactor() == 1);
        assert (fresh.getLatencySamples() == 0);
        assert (!fresh.isOversampled());
        ProcessorChain compat;
        compat.setOversampled (true);
        assert (compat.getOsFactor() == 2);
        assert (compat.isOversampled());
        compat.setOversampled (false);
        assert (compat.getOsFactor() == 1);
    }

    const double rates[2] = {48000.0, 44100.0};
    const int factors[3] = {1, 2, 4};
    for (int r = 0; r < 2; ++r)
    {
        const double sampleRate = rates[r];
        for (int f = 0; f < 3; ++f)
        {
            ProcessorChain chain;
            chain.setSampleRate (sampleRate);
            chain.setBoostStep (1);
            chain.setTone (0);
            chain.setHighcut (false);
            chain.setTrimDb (0.0f);
            chain.setOsFactor (factors[f]);

            // Flush past the entry blends with silence so the impulse below
            // exercises the steady path only (see (g)).
            for (int n = 0; n < 4096; ++n)
                chain.processSample (0.0f);

            const int window = 256;
            static float buf[256];
            assert (window <= 256);
            for (int n = 0; n < window; ++n)
                buf[n] = chain.processSample (n == 0 ? 0.1f : 0.0f);

            int peakIdx = 0;
            float peakMag = 0.0f;
            for (int n = 0; n < window; ++n)
            {
                const float m = std::fabs (buf[n]);
                if (m > peakMag)
                {
                    peakMag = m;
                    peakIdx = n;
                }
            }

            const int reported = chain.getLatencySamples();
            const int expected = (factors[f] == 4)   ? ProcessorChain::kOsLatency4xSamples
                                 : (factors[f] == 2) ? ProcessorChain::kOsLatencySamples
                                                     : 0;
            std::printf ("os latency %dx @ %.0fHz: peak at %d, reported %d (expect %d)\n", factors[f], sampleRate,
                         peakIdx, reported, expected);
            std::fflush (stdout);
            assert (reported == expected);
            assert (peakIdx == reported);
        }
    }
}

// (m) Factor-toggle no-click for the adjacent pair 2<->4, both directions:
// steady 220Hz sine, flip mid-stream; no sample-to-sample jump above 0.3x
// local RMS in the 50ms window (5ms equal-power xfade, same idiom as (h)).
void checkOsFactorFlip (int fromFactor, int toFactor)
{
    ProcessorChain chain;
    chain.setSampleRate (48000.0);
    chain.setBoostStep (1);
    chain.setTone (3);
    chain.setHighcut (false);
    chain.setTrimDb (0.0f);
    chain.setOsFactor (fromFactor);
    // Settle past the arming-blend transient so the measured flip starts
    // from a steady path.
    for (int i = 0; i < 48000; ++i)
        chain.processSample (0.5f *
                             static_cast<float> (std::sin (2.0 * kPi * 220.0 * static_cast<double> (i) / 48000.0)));

    float prev = chain.processSample (
        0.5f * static_cast<float> (std::sin (2.0 * kPi * 220.0 * static_cast<double> (48000) / 48000.0)));

    chain.setOsFactor (toFactor);

    const int window = 2400; // 50ms @48k.
    static float buf[2400];
    float maxJump = 0.0f;
    for (int i = 0; i < window; ++i)
    {
        const float x =
            0.5f * static_cast<float> (std::sin (2.0 * kPi * 220.0 * static_cast<double> (48000 + 1 + i) / 48000.0));
        buf[i] = chain.processSample (x);
        const float jump = std::fabs (buf[i] - prev);
        if (jump > maxJump)
            maxJump = jump;
        prev = buf[i];
    }

    const float localRms = rms (buf, 0, window);
    std::printf ("os-toggle %dx->%dx: maxJump %0.5f, localRMS %0.4f, limit %0.4f\n", fromFactor, toFactor, maxJump,
                 localRms, 0.3f * localRms);
    std::fflush (stdout);
    assert (maxJump < 0.3f * localRms);
}

void checkOsFactorToggleNoClick ()
{
    checkOsFactorFlip (2, 4);
    checkOsFactorFlip (4, 2);
}

// (n) 4x prepare-ordering contract (same as (j), factor 4): the toggle
// target survives setSampleRate, so param-push before rate-change and rate
// before param-push both report the 4x delay immediately, at both rates.
void checkOs4xPrepareOrdering ()
{
    const double rates[2] = {48000.0, 44100.0};
    for (int r = 0; r < 2; ++r)
    {
        {
            ProcessorChain chain; // push param, then rate.
            chain.setOsFactor (4);
            chain.setSampleRate (rates[r]);
            std::printf ("os4 prepare push-then-rate @ %.0fHz: latency %d (expect %d)\n", rates[r],
                         chain.getLatencySamples(), ProcessorChain::kOsLatency4xSamples);
            assert (chain.getLatencySamples() == ProcessorChain::kOsLatency4xSamples);
        }
        {
            ProcessorChain chain; // rate, then push param.
            chain.setSampleRate (rates[r]);
            chain.setOsFactor (4);
            std::printf ("os4 prepare rate-then-push @ %.0fHz: latency %d (expect %d)\n", rates[r],
                         chain.getLatencySamples(), ProcessorChain::kOsLatency4xSamples);
            assert (chain.getLatencySamples() == ProcessorChain::kOsLatency4xSamples);
        }
    }
}

// (o) 4x top-octave transparency: 1x-vs-4x fundamental at 15kHz and 20kHz,
// both rates, same +10dB-at-color drive as (f), limit +/-0.5dB (same margin
// idiom as (i); the cascade doubles the passband ripple at most).
void checkOs4xTopOctave ()
{
    const double rates[2] = {48000.0, 44100.0};
    const double freqs[2] = {15000.0, 20000.0};
    const float amp = std::pow (10.0f, 7.0f / 20.0f); // +7dB in, +3dB boost -> +10dB at color.
    for (int r = 0; r < 2; ++r)
    {
        for (int f = 0; f < 2; ++f)
        {
            ProcessorChain oneX;
            oneX.setSampleRate (rates[r]);
            oneX.setBoostStep (1);
            oneX.setTone (0);
            oneX.setHighcut (false);
            oneX.setTrimDb (0.0f);
            oneX.setOsFactor (1);
            const float a1 = fundAmp (oneX, rates[r], freqs[f], amp);

            ProcessorChain fourX;
            fourX.setSampleRate (rates[r]);
            fourX.setBoostStep (1);
            fourX.setTone (0);
            fourX.setHighcut (false);
            fourX.setTrimDb (0.0f);
            fourX.setOsFactor (4);
            const float a2 = fundAmp (fourX, rates[r], freqs[f], amp);

            const float diffDb = 20.0f * std::log10 (a2 / a1);
            std::printf ("os top-octave 1x-vs-4x @ %.0fHz/%.0fkHz: %+0.4fdB (expect 0 +/- 0.5)\n", rates[r],
                         freqs[f] / 1000.0, diffDb);
            std::fflush (stdout);
            assert (std::fabs (diffDb) < 0.5f);
        }
    }
}

// (p) Post-color DC discipline (Task 28, Ruling 30: 2Hz corner, not 5Hz —
// preserves the 5Hz-corner and 20Hz gates as-written; corner frequency only
// sets settling speed ~0.4s, and DC itself is attenuated infinitely at 0Hz
// regardless): the ColorStage `a*x^2` term rectifies (~3mV at +10dB, ~75mV
// hot). A post-color 2Hz blocker must kill it before the output. (a)
// DC-decay guard, (b) hot-sine DC/peak gate, (c) 1kHz transparency spot (the
// blocker must not move anything >= 20Hz).
void checkPostColorDcDecay ()
{
    ProcessorChain chain;
    chain.setSampleRate (48000.0);
    chain.setBoostStep (5);
    chain.setTone (3);
    chain.setHighcut (false);
    chain.setTrimDb (0.0f);
    double sum = 0.0;
    const int tail = 4800;
    for (int n = 0; n < 48000; ++n)
    {
        const float y = chain.processSample (1.0f);
        if (n >= 48000 - tail)
            sum += static_cast<double> (y);
    }
    const float mean = static_cast<float> (sum / static_cast<double> (tail));
    std::printf ("post-color dc decay: tail-mean %+0.6f (expect within +/- 0.01)\n", mean);
    std::fflush (stdout);
    assert (std::fabs (mean) < 0.01f);
}

void checkHotSineDc ()
{
    ProcessorChain chain;
    chain.setSampleRate (48000.0);
    chain.setBoostStep (10);
    chain.setTone (3);
    chain.setHighcut (false);
    chain.setTrimDb (0.0f);
    const float amp = std::pow (10.0f, 24.0f / 20.0f); // +24dB-equivalent float amplitude.
    const int total = 96000;                           // 2s @48k.
    const int skip = 48000;                            // measure the settled second.
    double sum = 0.0;
    double peak = 0.0;
    for (int n = 0; n < total; ++n)
    {
        const float x = amp * static_cast<float> (std::sin (2.0 * kPi * 220.0 * static_cast<double> (n) / 48000.0));
        const float y = chain.processSample (x);
        assert (std::isfinite (y));
        if (n >= skip)
        {
            sum += static_cast<double> (y);
            const double m = std::fabs (static_cast<double> (y));
            if (m > peak)
                peak = m;
        }
    }
    const double mean = sum / static_cast<double> (total - skip);
    const double ratio = (peak > 0.0) ? std::fabs (mean) / peak : 0.0;
    std::printf ("hot-sine dc: mean %+0.6f, peak %0.4f, dc/peak %0.6f (expect < 0.001)\n", mean, peak, ratio);
    std::fflush (stdout);
    assert (ratio < 0.001);
}

void checkPostColorTransparency1k ()
{
    ProcessorChain chain;
    chain.setSampleRate (48000.0);
    chain.setBoostStep (1);
    chain.setTone (3);
    chain.setHighcut (false);
    chain.setTrimDb (0.0f);
    const float measured = steadyGainDb (chain, 48000.0, 1000.0, 0.1f);

    ToneBank ref;
    ref.setSampleRate (48000.0);
    ref.setTone (3);
    // Boost step 1 (+3dB) + tone theory + color small-signal gain k/tanh(k)
    // (k = 0.06 post drive-fit) + engaged tilt theory (LS120/HS8k at 1kHz).
    const float expected = 3.0f + ref.magnitudeAt (1000.0f) + 20.0f * std::log10 (0.06f / std::tanh (0.06f)) +
                           tiltTheoryDb (1000.0, 48000.0);
    std::printf ("post-color transparency @1kHz/tone3: %+0.4fdB (expect %+0.4f +/- 0.05)\n", measured, expected);
    std::fflush (stdout);
    assert (std::fabs (measured - expected) < 0.05f);
}

// (q) 10Hz end-to-end anchor audit (Task 28, report-only by design: no
// assert — the controller rules compensate-vs-document from these numbers).
// Per tone (boost step 1, highcut off, trim 0): raw chain magnitude at 10Hz
// vs the absolute anchors (T1/T3/T4 -3, T2 -0.25, T5/T6 -22), with the exact
// blocker losses plus the engaged tilt shown separately (5Hz input blocker
// + 2Hz post-color blocker per Ruling 30 + LS120/HS8k tilt — one column per
// stage, so the audit names the actual chain).
void audit10HzAnchors ()
{
    constexpr double twoPi = 6.28318530717958647692;
    constexpr double fs = 48000.0;
    const double w = twoPi * 10.0 / fs;
    const std::complex<double> z = std::exp (std::complex<double> (0.0, -w));
    auto blockerLossDb = [&] (double fc)
    {
        const double a = 1.0 - std::exp (-twoPi * fc / fs);
        const std::complex<double> h = 1.0 - a / (1.0 - (1.0 - a) * z);
        return static_cast<float> (20.0 * std::log10 (std::abs (h)));
    };
    const float loss5 = blockerLossDb (5.0);
    const float loss2 = blockerLossDb (2.0);
    const float tilt10 = tiltTheoryDb (10.0, fs);

    const float anchors[7] = {0.0f, -3.0f, -0.25f, -3.0f, -3.0f, -22.0f, -22.0f};
    std::printf ("10Hz audit @48k (5Hz-blocker %+0.4fdB, 2Hz-blocker %+0.4fdB, tilt %+0.4fdB):\n", loss5, loss2,
                 tilt10);
    for (int tone = 1; tone <= 6; ++tone)
    {
        ProcessorChain chain;
        chain.setSampleRate (fs);
        chain.setBoostStep (1);
        chain.setTone (tone);
        chain.setHighcut (false);
        chain.setTrimDb (0.0f);
        const float raw = steadyGainDb (chain, fs, 10.0, 0.1f);
        // Chain adds boost (+3dB), color small-signal gain, the blockers,
        // and the engaged tilt.
        const float colorDb = 20.0f * std::log10 (0.06f / std::tanh (0.06f));
        const float tiltDb = tiltTheoryDb (10.0, fs);
        const float comp1 = raw - 3.0f - colorDb - loss5;
        const float comp2 = raw - 3.0f - colorDb - loss5 - loss2;
        const float comp3 = comp2 - tiltDb;
        std::printf ("  T%d: raw %+0.3f | -5Hz %+0.3f (d %+0.3f) | -both %+0.3f (d %+0.3f) | -tilt %+0.3f (d %+0.3f) | "
                     "anchor %+0.2f\n",
                     tone, raw, comp1, comp1 - anchors[tone], comp2, comp2 - anchors[tone], comp3,
                     comp3 - anchors[tone], anchors[tone]);
    }
    std::fflush (stdout);
}

} // namespace

int main ()
{
    checkBypassFlat();
    checkToneSwitchNoClick();
    checkBoostAndTrim();
    checkHighcutEndToEnd();
    checkPeakTracker();
    checkOsEquivalence();
    checkOsLatencyTruth();
    checkOsToggleNoClick();
    checkOsTopOctave();
    checkOsPrepareOrdering();
    checkOs4xEquivalence();
    checkOsFactorLatencyTruth();
    checkOsFactorToggleNoClick();
    checkOs4xPrepareOrdering();
    checkOs4xTopOctave();
    checkPostColorDcDecay();
    checkPostColorTransparency1k();
    audit10HzAnchors();
    checkHotSineDc();
    std::puts ("ChainTest: all checks passed");
    return 0;
}
