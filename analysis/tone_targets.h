#pragma once

#include <vector>

// Test oracle for Task 4: dB targets transcribed by eye from the Avalon U5
// manual tone chart (Tone images.png, log axis 10Hz-20kHz, frame +6/-24dB).
// 10 log-spaced points per tone, tones 1-6, covering 40Hz-15kHz.
// Tolerance convention: fitted biquads must land within +/-1dB of these.

struct ToneTarget
{
    int tone;
    float freqHz;
    float db;
};

inline std::vector<ToneTarget> getToneTargets ()
{
    return {
        // Tone 1: bass/mid scoop, minimum ~-6.5dB near 1kHz.
        {1, 40.0f, 1.0f},
        {1, 80.0f, 0.5f},
        {1, 150.0f, -2.5f},
        {1, 400.0f, -5.5f},
        {1, 700.0f, -6.3f},
        {1, 1000.0f, -6.5f},
        {1, 2000.0f, -5.5f},
        {1, 4000.0f, -4.0f},
        {1, 10000.0f, -1.5f},
        {1, 15000.0f, 0.5f},
        // Tone 2: deep narrow notch, ~-21dB just below 1kHz.
        {2, 40.0f, 1.0f},
        {2, 80.0f, 0.3f},
        {2, 150.0f, -2.0f},
        {2, 400.0f, -8.5f},
        {2, 700.0f, -18.0f},
        {2, 1000.0f, -14.0f},
        {2, 2000.0f, -5.5f},
        {2, 4000.0f, -1.8f},
        {2, 10000.0f, 0.5f},
        {2, 15000.0f, 1.0f},
        // Tone 3: gentle wide scoop, minimum ~-3dB near 1kHz.
        {3, 40.0f, 0.5f},
        {3, 80.0f, 0.2f},
        {3, 150.0f, -0.8f},
        {3, 400.0f, -2.3f},
        {3, 700.0f, -3.0f},
        {3, 1000.0f, -3.2f},
        {3, 2000.0f, -2.8f},
        {3, 4000.0f, -2.2f},
        {3, 10000.0f, -0.5f},
        {3, 15000.0f, 0.0f},
        // Tone 4: flat +1.5dB shelf with a dip (~-3.5dB) near 6kHz.
        {4, 40.0f, 1.0f},
        {4, 80.0f, 1.5f},
        {4, 150.0f, 1.5f},
        {4, 400.0f, 1.5f},
        {4, 700.0f, 1.3f},
        {4, 1000.0f, 1.2f},
        {4, 2000.0f, 0.5f},
        {4, 4000.0f, -1.5f},
        {4, 10000.0f, -0.5f},
        {4, 15000.0f, 1.5f},
        // Tone 5: high-pass, flat +2dB shelf above ~700Hz.
        {5, 40.0f, -12.0f},
        {5, 80.0f, -7.5f},
        {5, 150.0f, -4.0f},
        {5, 400.0f, -0.5f},
        {5, 700.0f, 1.0f},
        {5, 1000.0f, 2.0f},
        {5, 2000.0f, 2.0f},
        {5, 4000.0f, 2.0f},
        {5, 10000.0f, 2.0f},
        {5, 15000.0f, 2.0f},
        // Tone 6: high-pass with top-end roll-off (~-3dB at 20kHz).
        {6, 40.0f, -12.5f},
        {6, 80.0f, -8.0f},
        {6, 150.0f, -4.5f},
        {6, 400.0f, 1.0f},
        {6, 700.0f, 2.2f},
        {6, 1000.0f, 2.5f},
        {6, 2000.0f, 2.5f},
        {6, 4000.0f, 2.3f},
        {6, 10000.0f, 1.0f},
        {6, 15000.0f, -0.5f},
    };
}
