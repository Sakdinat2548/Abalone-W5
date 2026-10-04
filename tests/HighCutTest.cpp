// Abalone W5 - U5-inspired clean bass DI.
// Copyright (C) 2026 Sakdinat2548.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "../src/dsp/HighCut.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace
{

constexpr double kPi = 3.14159265358979323846;

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

// Steady-state sine level through an enabled HighCut, in dB relative to input.
// Measures RMS over the second half (filter settles in microseconds).
float levelDb (double sampleRate, double freqHz)
{
    HighCut hc;
    hc.setSampleRate (sampleRate);
    hc.setEnabled (true);

    const int total = static_cast<int> (sampleRate * 2.0);
    const int skip = total / 2;
    static float in[192000];
    static float out[192000];
    assert (total <= 192000);
    fillSine (in, total, sampleRate, freqHz, 0.5f);
    for (int i = 0; i < total; ++i)
        out[i] = hc.processSample (in[i]);

    return 20.0f * std::log10 (rms (out, skip, total - skip) / rms (in, skip, total - skip));
}

void checkMinus3dBAt8k ()
{
    // (a)+(b): 8kHz sine measures -3dB +/-0.3dB vs bypass when enabled,
    // at 48kHz, 44.1kHz and 96kHz (proves setSampleRate retunes).
    const double rates[3] = {48000.0, 44100.0, 96000.0};
    for (int r = 0; r < 3; ++r)
    {
        const float db = levelDb (rates[r], 8000.0);
        std::printf ("8kHz @ %.0fHz: %+0.3fdB (expect -3.0)\n", rates[r], db);
        assert (std::fabs (db + 3.0f) < 0.3f);
    }
}

void checkDisabledPassthrough ()
{
    // (c): disabled = bit-transparent passthrough (null difference on noise).
    // Also covers the enable->disable toggle: state is cleared, so the first
    // bypassed sample already equals the input exactly.
    HighCut hc;
    hc.setSampleRate (48000.0);
    hc.setEnabled (true);

    uint32_t rng = 0x12345678u;
    const int total = 48000;
    float maxDiff = 0.0f;
    for (int i = 0; i < total; ++i)
    {
        rng = rng * 1664525u + 1013904223u;
        const float x = static_cast<float> (rng >> 9) / static_cast<float> (1u << 23) - 1.0f;
        if (i == total / 2)
            hc.setEnabled (false);
        const float y = hc.processSample (x);
        if (i >= total / 2)
        {
            assert (y == x);
            const float d = std::fabs (y - x);
            if (d > maxDiff)
                maxDiff = d;
        }
    }
    std::printf ("disabled passthrough: max diff %0.1e (expect 0)\n", maxDiff);
    assert (maxDiff == 0.0f);
}

void checkStopbandSlope ()
{
    // (d): stopband sanity. NOTE: deviation from the brief's literal
    // "16kHz < -8dB": no true one-pole lowpass with -3dB at 8kHz can reach
    // -8dB at 16kHz (exact digital response is -6.02dB @48k, -5.84dB @44.1k;
    // analog prototype 1/sqrt(5) = -6.99dB). Asserts the implementable intent:
    // 16kHz is strictly more attenuated than the 8kHz point (one-pole slope,
    // ~3dB/oct between fc and 2fc) with real stopband attenuation.
    const double rates[2] = {48000.0, 44100.0};
    for (int r = 0; r < 2; ++r)
    {
        const float db8 = levelDb (rates[r], 8000.0);
        const float db16 = levelDb (rates[r], 16000.0);
        std::printf ("stopband @ %.0fHz: 8k %+0.3fdB, 16k %+0.3fdB\n", rates[r], db8, db16);
        assert (db16 < db8);
        assert (db8 - db16 > 2.0f);
        assert (db16 < -5.0f);
    }
}

void checkPassbandFlat ()
{
    // (e): 100Hz passes at ~0dB +/-0.2dB.
    const double rates[2] = {48000.0, 44100.0};
    for (int r = 0; r < 2; ++r)
    {
        const float db = levelDb (rates[r], 100.0);
        std::printf ("100Hz @ %.0fHz: %+0.3fdB (expect 0)\n", rates[r], db);
        assert (std::fabs (db) < 0.2f);
    }
}

} // namespace

int main ()
{
    checkMinus3dBAt8k();
    checkDisabledPassthrough();
    checkStopbandSlope();
    checkPassbandFlat();
    std::puts ("HighCutTest: all checks passed");
    return 0;
}
