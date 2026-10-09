// Abalone W5 - U5-inspired clean DI.
// Copyright (C) 2026 Sakdinat2548.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "../src/dsp/ColorStage.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;
constexpr float kPlus10dB = 3.1622776601683795f; // 10^(+10/20): peak amplitude for thdAt levelDb=+10.

// Independent harmonic magnitude via coherent correlation (the test's own DFT;
// plain loops, double accumulation, no extra deps). Drives the stage itself.
float harmonicMag (ColorStage& stage, double freqHz, float ampPeak, int harmonic)
{
    const int total = static_cast<int> (kSampleRate); // 1s: integer cycles for 100/1k/5k.
    double re = 0.0;
    double im = 0.0;
    for (int n = 0; n < total; ++n)
    {
        const float x =
            ampPeak * static_cast<float> (std::sin (2.0 * kPi * freqHz * static_cast<double> (n) / kSampleRate));
        const double y = static_cast<double> (stage.processSample (x));
        const double a = 2.0 * kPi * freqHz * static_cast<double> (harmonic) * static_cast<double> (n) / kSampleRate;
        re += y * std::cos (a);
        im += y * std::sin (a);
    }
    return static_cast<float> (2.0 * std::hypot (re, im) / static_cast<double> (total));
}

void checkThdAtPlus10dB ()
{
    // (a) 1kHz @ +10dB THD in [0.3%, 0.5%] (capture-fitted drive, 2026-10-09).
    ColorStage stage;
    stage.setEnabled (true);
    const float thd = stage.thdAt (1000.0f, 10.0f);
    std::printf ("THD 1kHz @+10dB: %0.5f%% (expect 0.3-0.5)\n", 100.0 * thd);
    assert (thd >= 0.003f && thd <= 0.005f);
}

void checkCleanAtNominal ()
{
    // (b) 1kHz @ 0dB THD < 0.12% (clean at nominal level).
    ColorStage stage;
    stage.setEnabled (true);
    const float thd = stage.thdAt (1000.0f, 0.0f);
    std::printf ("THD 1kHz @0dB: %0.5f%% (expect < 0.12)\n", 100.0 * thd);
    assert (thd < 0.0012f);
}

void checkNoHarshBands ()
{
    // (c) 100Hz and 5kHz @ +10dB both < 0.5% (no harsh bands).
    ColorStage stage;
    stage.setEnabled (true);
    const float thdLo = stage.thdAt (100.0f, 10.0f);
    const float thdHi = stage.thdAt (5000.0f, 10.0f);
    std::printf ("THD 100Hz @+10dB: %0.5f%%, 5kHz @+10dB: %0.5f%% (expect both < 0.5)\n", 100.0 * thdLo, 100.0 * thdHi);
    assert (thdLo < 0.005f);
    assert (thdHi < 0.005f);
}

void checkDisabledBitTransparent ()
{
    // (d) disabled bypass is bit-transparent on a noise buffer.
    ColorStage stage;
    stage.setEnabled (false);

    const int total = static_cast<int> (kSampleRate);
    std::uint32_t rng = 0x12345678u;
    for (int i = 0; i < total; ++i)
    {
        rng = rng * 1664525u + 1013904223u;
        const float x = static_cast<float> (static_cast<double> (static_cast<std::int32_t> (rng)) / 2147483648.0);
        const float y = stage.processSample (x);
        assert (y == x);
    }
    std::puts ("disabled bypass bit-transparent on 48k noise samples");
}

void checkHotInputFinite ()
{
    // (e) hot input (+24dB sine) stays finite, no NaN/Inf.
    ColorStage stage;
    stage.setEnabled (true);

    const float hotAmp = 15.848931924611136f; // 10^(+24/20) peak.
    const int total = static_cast<int> (kSampleRate);
    float peak = 0.0f;
    for (int i = 0; i < total; ++i)
    {
        const float x =
            hotAmp * static_cast<float> (std::sin (2.0 * kPi * 1000.0 * static_cast<double> (i) / kSampleRate));
        const float y = stage.processSample (x);
        assert (std::isfinite (y));
        if (std::fabs (y) > peak)
            peak = std::fabs (y);
    }
    std::printf ("hot +24dB 1kHz: peak %0.3f (input peak %0.3f), all finite\n", peak, hotAmp);
    assert (peak < hotAmp); // gentle saturation never boosts a hot sine.
}

void checkSecondHarmonicDominates ()
{
    // (f) 2nd harmonic dominates at 0dB (H2 > H3+H4+...+H8): Class-A
    // even-order character where it lives. At +10dB H3 takes over (0.30%
    // vs 0.24% — saturation reality, matching captures), so the dominance
    // gate lives at nominal level, not at the operating point.
    ColorStage stage;
    stage.setEnabled (true);

    const float h1 = harmonicMag (stage, 1000.0, 1.0f, 1);
    float rest = 0.0f;
    for (int h = 3; h <= 8; ++h)
        rest += harmonicMag (stage, 1000.0, 1.0f, h);
    const float h2 = harmonicMag (stage, 1000.0, 1.0f, 2);
    std::printf ("1kHz @0dB: H1=%0.5f H2=%0.6f H3+..+H8=%0.6f (expect H2 > rest)\n", h1, h2, rest);
    assert (h1 > 0.0f);
    assert (h2 > rest);
}

void checkNoAliasEnergy ()
{
    // (g) 1kHz @ +10dB: no significant energy above Nyquist*0.9 (no-oversampling gate).
    ColorStage stage;
    stage.setEnabled (true);

    const int total = 4800; // 100 coherent 1kHz cycles @48k; 10Hz bin grid.
    const int loBin = 2160; // 0.9 * Nyquist / 10Hz.
    const int hiBin = 2400; // Nyquist / 10Hz.
    double h1re = 0.0;
    double h1im = 0.0;
    double worst = 0.0;
    for (int b = loBin; b <= hiBin; ++b)
    {
        double re = 0.0;
        double im = 0.0;
        for (int n = 0; n < total; ++n)
        {
            const float x =
                kPlus10dB * static_cast<float> (std::sin (2.0 * kPi * 1000.0 * static_cast<double> (n) / kSampleRate));
            const double y = static_cast<double> (stage.processSample (x));
            const double a = 2.0 * kPi * static_cast<double> (b * 10) * static_cast<double> (n) / kSampleRate;
            re += y * std::cos (a);
            im += y * std::sin (a);
        }
        const double mag = 2.0 * std::hypot (re, im) / static_cast<double> (total);
        if (mag > worst)
            worst = mag;
    }
    for (int n = 0; n < total; ++n)
    {
        const float x =
            kPlus10dB * static_cast<float> (std::sin (2.0 * kPi * 1000.0 * static_cast<double> (n) / kSampleRate));
        const double y = static_cast<double> (stage.processSample (x));
        const double a = 2.0 * kPi * 1000.0 * static_cast<double> (n) / kSampleRate;
        h1re += y * std::cos (a);
        h1im += y * std::sin (a);
    }
    const double h1 = 2.0 * std::hypot (h1re, h1im) / static_cast<double> (total);
    std::printf ("alias band 21.6-24kHz: worst %0.3e vs H1 %0.4f (ratio %0.3e, expect < 1e-4)\n", worst, h1,
                 worst / h1);
    assert (worst / h1 < 1e-4);
}

void checkCalibratedLevelMap ()
{
    // Task 29 pins (0 dBFS = +24 dBu; boost step N adds 3N dB; see analysis/LEVELS.md).
    // Re-derived 2026-10-09 for the capture-fitted drive (k=0.06, a=1.5e-3).
    // Quiet: boost 1 x -20 dBFS -> stage -17 dB, +7 dBu, ~= 0.011%.
    // Nominal: boost 5 x -10 dBFS -> stage +5 dB, +29 dBu, ~= 0.164%.
    // Hot: boost 10 x -20 dBFS -> stage +10 dB, +34 dBu, ~= 0.382%.
    ColorStage stage;
    stage.setEnabled (true);
    const float thdQuiet = stage.thdAt (1000.0f, -17.0f);
    const float thdNominal = stage.thdAt (1000.0f, 5.0f);
    const float thdHot = stage.thdAt (1000.0f, 10.0f);
    std::printf ("map quiet: %0.5f%%, nominal: %0.5f%%, hot: %0.5f%%\n", 100.0 * thdQuiet, 100.0 * thdNominal,
                 100.0 * thdHot);
    assert (thdQuiet >= 0.00008f && thdQuiet <= 0.00014f);
    assert (thdNominal >= 0.0013f && thdNominal <= 0.0020f);
    assert (thdHot >= 0.0030f && thdHot <= 0.0050f);
}

void checkFastTanhAccuracy ()
{
    // fastTanh must stay within 1e-6 of std::tanh over the stage's full
    // argument range (|z| <= 1.9: k = 0.06, |x| <= 31.6); anything worse
    // moves the fitted THD numbers above.
    float worst = 0.0f;
    for (int i = 0; i <= 4000; ++i)
    {
        const float x = -2.0f + 4.0f * static_cast<float> (i) / 4000.0f;
        const float d = std::fabs (ColorStage::fastTanh (x) - std::tanh (x));
        if (d > worst)
            worst = d;
    }
    std::printf ("fastTanh max abs err on [-2,2]: %0.3e (expect < 1e-6)\n", worst);
    assert (worst < 1e-6f);
}

} // namespace

int main ()
{
    checkThdAtPlus10dB();
    checkCleanAtNominal();
    checkNoHarshBands();
    checkDisabledBitTransparent();
    checkHotInputFinite();
    checkSecondHarmonicDominates();
    checkNoAliasEnergy();
    checkCalibratedLevelMap();
    checkFastTanhAccuracy();
    std::puts ("ColorStageTest: all checks passed");
    return 0;
}
