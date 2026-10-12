// Abalone W5 - U5-inspired clean DI.
// Copyright (C) 2026 Sakdinat2548.
// SPDX-License-Identifier: AGPL-3.0-or-later

// Docs tool (NOT a test — no gates, no ctest registration): dumps the
// steady-state boost transfer curves plotted as docs/dynamics.png. Drives
// the real ProcessorChain, so the plot can never drift from shipped DSP.
// Build once: cmake --build build --target DynDump
// Run: ./build/DynDump > /tmp/dyn.csv  (or analysis/circuit_trace/
// plot_dynamics.py, which runs it for you).
// Stimulus (fixed and documented): 1 kHz sine peak sweep -60..0 dBFS in
// 1 dB steps, tone bypass, highcut off, trim = -3N dB per curve (unity
// staging: the linear region overlays y = x, saturation reads as droop),
// 48 kHz, 1.0 s settle + 0.25 s peak window. Columns: inDbFS,B1,B3,B6,B8,B10
// (dB peak out).

#include <cmath>
#include <cstdio>

#include "dsp/ProcessorChain.h"

namespace
{

constexpr double kFs = 48000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr double kSettleSec = 1.0;
constexpr double kWindowSec = 0.25;

float settledPeakDb (int boostStep, float inDbFS)
{
    ProcessorChain chain;
    chain.setSampleRate (kFs);
    chain.setBoostStep (boostStep);
    chain.setTone (0);
    chain.setHighcut (false);
    chain.setTrimDb (-3.0f * static_cast<float> (boostStep)); // unity staging per curve.
    const float amp = std::pow (10.0f, inDbFS / 20.0f);
    const int settle = static_cast<int> (kFs * kSettleSec);
    const int window = static_cast<int> (kFs * kWindowSec);
    for (int n = 0; n < settle; ++n)
        chain.processSample (amp * static_cast<float> (std::sin (2.0 * kPi * 1000.0 * n / kFs)));
    float peak = 0.0f;
    for (int n = 0; n < window; ++n)
    {
        const float y =
            chain.processSample (amp * static_cast<float> (std::sin (2.0 * kPi * 1000.0 * (settle + n) / kFs)));
        const float a = std::fabs (y);
        if (a > peak)
            peak = a;
    }
    return 20.0f * std::log10 (peak);
}

} // namespace

int main ()
{
    const int steps[5] = {1, 3, 6, 8, 10};
    std::puts ("inDbFS,B1,B3,B6,B8,B10");
    for (int level = -60; level <= 0; ++level)
    {
        std::printf ("%d", level);
        for (int s = 0; s < 5; ++s)
            std::printf (",%.2f", settledPeakDb (steps[s], static_cast<float> (level)));
        std::puts ("");
    }
    return 0;
}
