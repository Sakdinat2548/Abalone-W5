#include "../src/dsp/GainStage.h"

#include <cassert>
#include <cmath>
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

void checkSteppedGainAtBothRates ()
{
    // (a) 1kHz sine through setStep(5) measures +15dB +/-0.2dB vs bypass.
    // Run at both 48kHz and 44.1kHz: identical dB shape across rates.
    const double rates[2] = {48000.0, 44100.0};
    for (int r = 0; r < 2; ++r)
    {
        const double sampleRate = rates[r];
        GainStage stage;
        stage.setSampleRate (sampleRate);
        stage.setStep (5);

        const int total = static_cast<int> (sampleRate * 2.0);
        const int skip = static_cast<int> (sampleRate);
        static float in[96000];
        static float out[96000];
        assert (total <= 96000);
        fillSine (in, total, sampleRate, 1000.0, 0.5f);
        for (int i = 0; i < total; ++i)
            out[i] = stage.processSample (in[i]);

        const float measuredDb = 20.0f * std::log10 (rms (out, skip, total - skip) / rms (in, skip, total - skip));
        std::printf ("step5 @ %.0fHz: measured %+0.3fdB (expect +15)\n", sampleRate, measuredDb);
        assert (std::fabs (measuredDb - 15.0f) < 0.2f);
    }
}

void checkHotInputStaysFinite ()
{
    // (b) hot input (+24dBu-equivalent float amplitude) at setStep(10):
    // finite, no NaN/Inf, peak ~= +30dB.
    GainStage stage;
    stage.setSampleRate (48000.0);
    stage.setStep (10);

    const float hotAmp = 8.0f; // ~+18dB over 1.0 peak: drives output to ~250x
    const int total = 48000;
    const float expectedPeak = hotAmp * std::pow (10.0f, 30.0f / 20.0f);
    float peak = 0.0f;
    for (int i = 0; i < total; ++i)
    {
        const float x = hotAmp * static_cast<float> (std::sin (2.0 * kPi * 1000.0 * static_cast<double> (i) / 48000.0));
        const float y = stage.processSample (x);
        assert (std::isfinite (y));
        if (i >= total / 2 && std::fabs (y) > peak)
            peak = std::fabs (y);
    }
    std::printf ("step10 hot: peak %0.2f (expect %0.2f)\n", peak, expectedPeak);
    assert (std::fabs (peak / expectedPeak - 1.0f) < 0.02f);
}

void checkDcBlocked ()
{
    // (c) 1.0f DC for 1s at 48kHz decays to <0.01f.
    GainStage stage;
    stage.setSampleRate (48000.0);
    stage.setStep (5);

    float y = 0.0f;
    for (int i = 0; i < 48000; ++i)
        y = stage.processSample (1.0f);
    std::printf ("DC after 1s: %0.6f (expect |y| < 0.01)\n", y);
    assert (std::fabs (y) < 0.01f);
}

void checkDbMapping ()
{
    // (d) getDb() returns 3.0f/15.0f/30.0f for steps 1/5/10; clamps to [1,10].
    GainStage stage;
    stage.setStep (1);
    assert (stage.getDb() == 3.0f);
    stage.setStep (5);
    assert (stage.getDb() == 15.0f);
    stage.setStep (10);
    assert (stage.getDb() == 30.0f);
    stage.setStep (0);
    assert (stage.getDb() == 3.0f);
    stage.setStep (11);
    assert (stage.getDb() == 30.0f);
    stage.setStep (-5);
    assert (stage.getDb() == 3.0f);
    stage.setStep (100);
    assert (stage.getDb() == 30.0f);
    std::puts ("db mapping + clamp ok");
}

void checkGainStepIsSmoothed ()
{
    // (e) 1->10 step change must NOT jump instantly (zipper noise): output
    // RMS over the first 2ms after the switch stays near the old level
    // (smoothed ramp toward the new gain), then converges to +30dB.
    GainStage stage;
    stage.setSampleRate (48000.0);
    stage.setStep (1);

    const int total = 48000 * 3;
    static float in[144000];
    static float out[144000];
    assert (total <= 144000);
    fillSine (in, total, 48000.0, 1000.0, 0.5f);
    for (int i = 0; i < 48000; ++i)
        out[i] = stage.processSample (in[i]); // settle at step 1 (+3dB)

    const float preRms = rms (out, 48000 - 4800, 4800);
    stage.setStep (10); // +30dB: instant would jump ~22x here
    for (int i = 48000; i < total; ++i)
        out[i] = stage.processSample (in[i]);

    const float jumpRms = rms (out, 48000, 96); // first 2ms after switch
    std::printf ("step1->10: 2ms-post RMS ratio %0.2f (expect < 5, instant would be ~22)\n", jumpRms / preRms);
    assert (jumpRms / preRms < 5.0f);

    const float postDb = 20.0f * std::log10 (rms (out, total - 48000, 48000) / rms (in, total - 48000, 48000));
    std::printf ("step1->10: settled %+0.3fdB (expect +30)\n", postDb);
    assert (std::fabs (postDb - 30.0f) < 0.5f);
}

} // namespace

int main ()
{
    checkDbMapping();
    checkSteppedGainAtBothRates();
    checkHotInputStaysFinite();
    checkDcBlocked();
    checkGainStepIsSmoothed();
    std::puts ("GainStageTest: all checks passed");
    return 0;
}
