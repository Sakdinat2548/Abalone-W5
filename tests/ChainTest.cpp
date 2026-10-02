#include "../src/dsp/ProcessorChain.h"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace
{

constexpr double kPi = 3.14159265358979323846;

// (a) End-to-end bypass-flat: tone 0, highcut off, boost step 1 (+3dB),
// trim 0. Output must sit exactly on the +3dB line: 1kHz within +/-0.1dB,
// 20Hz-15kHz spots within +/-0.5dB. 5Hz is the DC-block corner (-3dB by
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
            const float tol = spots[f] == 1000.0 ? 0.1f : 0.5f;
            std::printf ("bypass-flat @ %.0fHz / %.0fHz: %+0.3fdB (expect %+0.1f +/- %0.1f)\n", sampleRate, spots[f],
                         gainDb, kStep1Db, tol);
            assert (std::fabs (gainDb - kStep1Db) < tol);
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

} // namespace

int main ()
{
    checkBypassFlat();
    checkToneSwitchNoClick();
    checkBoostAndTrim();
    checkHighcutEndToEnd();
    checkPeakTracker();
    std::puts ("ChainTest: all checks passed");
    return 0;
}
