// Task 11: mono/dual-mono channel handling + host bypass, driven through the
// REAL AbaloneW5AudioProcessor headless (JUCE AudioBuffer, no host needed).
//
// Cases: (a) 1-in/2-out duplicates processed ch0 sample-exact; (b) 2-in/2-out
// stays independent; (c) 1-in/1-out unchanged; (d) host bypass bit-exact
// (stereo passthrough + mono duplication); (e) ACTIVE-off passthrough with
// the same mono rule (the host-bypass OR internal-bypass condition).
//
// NOTE on asserts: this target deliberately does NOT use add_dsp_test().
// That helper undefines NDEBUG, which would flip JUCE_DEBUG (and hence
// JUCE_CHECK_MEMORY_LEAKS, which adds a member to JUCE classes) in this TU
// while the linked AbaloneW5/JUCE objects were built Release WITH NDEBUG —
// a class-layout ODR mismatch. All checks below are explicit and
// NDEBUG-independent, so they stay live under ctest either way.

#include "../src/PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{

constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 512;
// processBlock runs under ScopedNoDenormals (FTZ/DAZ) while the reference
// chain here runs with default flags, so subnormal-adjacent samples may
// differ by ULPs: compare proc-vs-reference with a tolerance. The mono
// duplication itself is a copy, so ch0-vs-ch1 is asserted sample-EXACT.
constexpr float kTol = 1e-6f;
constexpr double kPi = 3.14159265358979323846;

int failures = 0;

#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            ++failures;                                                                                                \
            std::printf ("FAIL line %d: %s\n", __LINE__, #cond);                                                       \
        }                                                                                                              \
    } while (0)

void fillSine (juce::AudioBuffer<float>& buffer, int channel, double freqHz, float amp)
{
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        buffer.setSample (
            channel, i,
            amp * static_cast<float> (std::sin (2.0 * kPi * freqHz * static_cast<double> (i) / kSampleRate)));
}

void fillDc (juce::AudioBuffer<float>& buffer, int channel, float value)
{
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        buffer.setSample (channel, i, value);
}

float maxAbsDiff (const float* a, const float* b, int n)
{
    float m = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float d = std::fabs (a[i] - b[i]);
        if (d > m)
            m = d;
    }
    return m;
}

float peakOf (const float* data, int n)
{
    float m = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float a = std::fabs (data[i]);
        if (a > m)
            m = a;
    }
    return m;
}

void fillSineCont (juce::AudioBuffer<float>& buffer, int channel, double freqHz, float amp, int& phase)
{
    for (int i = 0; i < buffer.getNumSamples(); ++i, ++phase)
        buffer.setSample (
            channel, i,
            amp * static_cast<float> (std::sin (2.0 * kPi * freqHz * static_cast<double> (phase) / kSampleRate)));
}

int gWorstBlock = -1;
int gWorstIdx = -1;

float maxStepJump (const float* data, int n, int blockIdx)
{
    float m = 0.0f;
    for (int i = 1; i < n; ++i)
    {
        const float d = std::fabs (data[i] - data[i - 1]);
        if (d > m)
        {
            m = d;
            gWorstBlock = blockIdx;
            gWorstIdx = i;
        }
    }
    return m;
}

// Flat reference voice: boost step 1 (+3dB), tone bypass, highcut off,
// trim 0dB — mirrors setFlatParams() below.
void makeFlatReference (ProcessorChain& chain)
{
    chain.setSampleRate (kSampleRate);
    chain.setBoostStep (1);
    chain.setTone (0);
    chain.setHighcut (false);
    chain.setTrimDb (0.0f);
}

void setFlatParams (AbaloneW5AudioProcessor& proc)
{
    auto& apvts = proc.getApvts();
    *apvts.getRawParameterValue ("boost") = 0.0f; // Choice index 0 -> step 1 (+3dB)
    *apvts.getRawParameterValue ("tone") = 0.0f;  // Choice index 0 -> Bypass
    *apvts.getRawParameterValue ("toneIn") = 1.0f;
    *apvts.getRawParameterValue ("active") = 1.0f;
    *apvts.getRawParameterValue ("highcut") = 0.0f;
    *apvts.getRawParameterValue ("output") = 0.0f; // 0 dB trim
}

bool setLayout (AbaloneW5AudioProcessor& proc, const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (in);
    layout.outputBuses.add (out);
    return proc.setBusesLayout (layout);
}

// (a) 1-in/2-out: ch0 sine in, ch1 hostile DC in. ch1 out must equal ch0 out
// sample-exact, and ch0 out must sit on the flat reference (boosted) level.
void checkMonoInStereoOut ()
{
    AbaloneW5AudioProcessor proc;
    CHECK (setLayout (proc, juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo()));
    CHECK (proc.getMainBusNumInputChannels() == 1);
    proc.prepareToPlay (kSampleRate, kBlock);
    setFlatParams (proc);

    juce::AudioBuffer<float> buffer (2, kBlock);
    fillSine (buffer, 0, 1000.0, 0.5f);
    fillDc (buffer, 1, 0.9f); // garbage: the old code processed this to ch1.

    static float input0[kBlock];
    std::memcpy (input0, buffer.getReadPointer (0), sizeof (input0));

    juce::MidiBuffer midi;
    proc.processBlock (buffer, midi);

    const float* ch0 = buffer.getReadPointer (0);
    const float* ch1 = buffer.getReadPointer (1);
    const float dupErr = maxAbsDiff (ch0, ch1, kBlock);
    std::printf ("mono 1-in/2-out: max|ch1-ch0| = %.9f (expect 0, sample-exact copy)\n", dupErr);
    CHECK (dupErr == 0.0f);

    ProcessorChain ref;
    makeFlatReference (ref);
    static float expected[kBlock];
    for (int i = 0; i < kBlock; ++i)
        expected[i] = ref.processSample (input0[i]);
    const float refErr = maxAbsDiff (ch0, expected, kBlock);
    std::printf ("mono 1-in/2-out: max|ch0-ref| = %.9f (expect < %.0e), peak %.4f (in 0.5, boosted)\n", refErr,
                 static_cast<double> (kTol), static_cast<double> (peakOf (ch0, kBlock)));
    CHECK (refErr < kTol);
    CHECK (peakOf (ch0, kBlock) > 0.5f);
}

// (b) 2-in/2-out with different L/R content: channels stay independent.
void checkStereoIndependent ()
{
    AbaloneW5AudioProcessor proc; // default stereo-in/stereo-out buses.
    CHECK (proc.getMainBusNumInputChannels() == 2);
    proc.prepareToPlay (kSampleRate, kBlock);
    setFlatParams (proc);

    juce::AudioBuffer<float> buffer (2, kBlock);
    fillSine (buffer, 0, 1000.0, 0.5f);
    fillSine (buffer, 1, 100.0, 0.4f);

    static float input0[kBlock];
    static float input1[kBlock];
    std::memcpy (input0, buffer.getReadPointer (0), sizeof (input0));
    std::memcpy (input1, buffer.getReadPointer (1), sizeof (input1));

    juce::MidiBuffer midi;
    proc.processBlock (buffer, midi);

    ProcessorChain refL, refR;
    makeFlatReference (refL);
    makeFlatReference (refR);
    static float expectedL[kBlock];
    static float expectedR[kBlock];
    for (int i = 0; i < kBlock; ++i)
    {
        expectedL[i] = refL.processSample (input0[i]);
        expectedR[i] = refR.processSample (input1[i]);
    }

    const float* ch0 = buffer.getReadPointer (0);
    const float* ch1 = buffer.getReadPointer (1);
    const float errL = maxAbsDiff (ch0, expectedL, kBlock);
    const float errR = maxAbsDiff (ch1, expectedR, kBlock);
    const float separation = maxAbsDiff (ch0, ch1, kBlock);
    std::printf ("stereo 2-in/2-out: errL %.9f errR %.9f (expect < %.0e), L/R separation %.4f (expect > 0.1)\n", errL,
                 errR, static_cast<double> (kTol), static_cast<double> (separation));
    CHECK (errL < kTol);
    CHECK (errR < kTol);
    CHECK (separation > 0.1f);
}

// (c) 1-in/1-out: unchanged per-channel behavior.
void checkMonoInMonoOut ()
{
    AbaloneW5AudioProcessor proc;
    CHECK (setLayout (proc, juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono()));
    proc.prepareToPlay (kSampleRate, kBlock);
    setFlatParams (proc);

    juce::AudioBuffer<float> buffer (1, kBlock);
    fillSine (buffer, 0, 1000.0, 0.5f);

    static float input0[kBlock];
    std::memcpy (input0, buffer.getReadPointer (0), sizeof (input0));

    juce::MidiBuffer midi;
    proc.processBlock (buffer, midi);

    ProcessorChain ref;
    makeFlatReference (ref);
    static float expected[kBlock];
    for (int i = 0; i < kBlock; ++i)
        expected[i] = ref.processSample (input0[i]);

    const float err = maxAbsDiff (buffer.getReadPointer (0), expected, kBlock);
    std::printf ("mono 1-in/1-out: max|ch0-ref| = %.9f (expect < %.0e)\n", err, static_cast<double> (kTol));
    CHECK (err < kTol);
}

// (d) Host bypass: bit-transparent passthrough. Stereo passes untouched;
// mono-in duplicates ch0 to ch1 (the extra channel holds no input data).
void checkHostBypass ()
{
    {
        AbaloneW5AudioProcessor proc;
        proc.prepareToPlay (kSampleRate, kBlock);
        setFlatParams (proc);

        juce::AudioBuffer<float> buffer (2, kBlock);
        fillSine (buffer, 0, 1000.0, 0.5f);
        fillSine (buffer, 1, 100.0, 0.4f);

        static float snapshot0[kBlock];
        static float snapshot1[kBlock];
        std::memcpy (snapshot0, buffer.getReadPointer (0), sizeof (snapshot0));
        std::memcpy (snapshot1, buffer.getReadPointer (1), sizeof (snapshot1));

        juce::MidiBuffer midi;
        proc.processBlockBypassed (buffer, midi);

        CHECK (std::memcmp (snapshot0, buffer.getReadPointer (0), sizeof (snapshot0)) == 0);
        CHECK (std::memcmp (snapshot1, buffer.getReadPointer (1), sizeof (snapshot1)) == 0);
        std::puts ("bypass stereo 2-in/2-out: bit-exact passthrough");
    }

    {
        AbaloneW5AudioProcessor proc;
        CHECK (setLayout (proc, juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo()));
        proc.prepareToPlay (kSampleRate, kBlock);
        setFlatParams (proc);

        juce::AudioBuffer<float> buffer (2, kBlock);
        fillSine (buffer, 0, 1000.0, 0.5f);
        fillDc (buffer, 1, 0.9f);

        static float snapshot0[kBlock];
        std::memcpy (snapshot0, buffer.getReadPointer (0), sizeof (snapshot0));

        juce::MidiBuffer midi;
        proc.processBlockBypassed (buffer, midi);

        const float* ch0 = buffer.getReadPointer (0);
        const float* ch1 = buffer.getReadPointer (1);
        CHECK (std::memcmp (snapshot0, ch0, sizeof (snapshot0)) == 0);
        const float dupErr = maxAbsDiff (ch0, ch1, kBlock);
        std::printf ("bypass mono 1-in/2-out: ch0 untouched, max|ch1-ch0| = %.9f (expect 0)\n", dupErr);
        CHECK (dupErr == 0.0f);
    }
}

// (e) ACTIVE-off internal bypass: same passthrough contract as host bypass
// (the OR condition) — untouched buffer, plus the mono copy when 1-in.
void checkActiveOffPassthrough ()
{
    {
        AbaloneW5AudioProcessor proc;
        proc.prepareToPlay (kSampleRate, kBlock);
        setFlatParams (proc);
        *proc.getApvts().getRawParameterValue ("active") = 0.0f;

        juce::AudioBuffer<float> buffer (2, kBlock);
        fillSine (buffer, 0, 1000.0, 0.5f);
        fillSine (buffer, 1, 100.0, 0.4f);

        static float snapshot0[kBlock];
        static float snapshot1[kBlock];
        std::memcpy (snapshot0, buffer.getReadPointer (0), sizeof (snapshot0));
        std::memcpy (snapshot1, buffer.getReadPointer (1), sizeof (snapshot1));

        juce::MidiBuffer midi;
        proc.processBlock (buffer, midi);

        CHECK (std::memcmp (snapshot0, buffer.getReadPointer (0), sizeof (snapshot0)) == 0);
        CHECK (std::memcmp (snapshot1, buffer.getReadPointer (1), sizeof (snapshot1)) == 0);
        std::puts ("active-off stereo: bit-exact passthrough");
    }

    {
        AbaloneW5AudioProcessor proc;
        CHECK (setLayout (proc, juce::AudioChannelSet::mono(), juce::AudioChannelSet::stereo()));
        proc.prepareToPlay (kSampleRate, kBlock);
        setFlatParams (proc);
        *proc.getApvts().getRawParameterValue ("active") = 0.0f;

        juce::AudioBuffer<float> buffer (2, kBlock);
        fillSine (buffer, 0, 1000.0, 0.5f);
        fillDc (buffer, 1, 0.9f);

        static float snapshot0[kBlock];
        std::memcpy (snapshot0, buffer.getReadPointer (0), sizeof (snapshot0));

        juce::MidiBuffer midi;
        proc.processBlock (buffer, midi);

        const float* ch0 = buffer.getReadPointer (0);
        const float* ch1 = buffer.getReadPointer (1);
        CHECK (std::memcmp (snapshot0, ch0, sizeof (snapshot0)) == 0);
        const float dupErr = maxAbsDiff (ch0, ch1, kBlock);
        std::printf ("active-off mono 1-in/2-out: ch0 untouched, max|ch1-ch0| = %.9f (expect 0)\n", dupErr);
        CHECK (dupErr == 0.0f);
    }
}

// (f) ACTIVE toggle must not click: settle engaged on a continuous 220Hz
// sine, flip off mid-stream and bound every sample-to-sample jump (block
// boundary included), then flip back on and bound again. A 5ms equal-power
// crossfade keeps the toggle near the sine's natural slope (~0.014); a hard
// switch would jump by |wet-dry| (~0.2+).
void checkActiveToggleNoClick ()
{
    AbaloneW5AudioProcessor proc; // stereo default
    proc.prepareToPlay (kSampleRate, kBlock);
    setFlatParams (proc); // engaged, boost step 1 (+3dB), tone bypass

    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;
    // Toggle-phase discipline: the off-toggle must land near a sine PEAK so
    // the old-code hard switch actually jumps (phase-lucky zero-crossing
    // toggles pass even unfixed). 220Hz/48k advances 11/2400 cycles/sample;
    // peak needs total P = 51712 + P0 with P0 = 488 (52200 mod 2400 = 1800 =
    // quarter-cycle). The on-toggle then lands at 0.64 cycle (0.41 x 0.38
    // amplitude = 0.16 jump on old code) — still discriminating.
    int phase0 = 488;
    int phase1 = 488;
    for (int b = 0; b < 100; ++b) // settle ~1s engaged
    {
        fillSineCont (buffer, 0, 220.0, 0.5f, phase0);
        fillSineCont (buffer, 1, 220.0, 0.5f, phase1);
        proc.processBlock (buffer, midi);
    }
    fillSineCont (buffer, 0, 220.0, 0.5f, phase0);
    fillSineCont (buffer, 1, 220.0, 0.5f, phase1);
    proc.processBlock (buffer, midi);
    const float steadyJump = maxStepJump (buffer.getReadPointer (0), kBlock, -1);

    auto runToggleBlocks = [&] (float activeValue)
    {
        *proc.getApvts().getRawParameterValue ("active") = activeValue;
        float worst = 0.0f;
        float worstEdge = 0.0f;
        int worstEdgeBlock = -1;
        float prevLast = buffer.getReadPointer (0)[kBlock - 1];
        for (int b = 0; b < 4; ++b)
        {
            fillSineCont (buffer, 0, 220.0, 0.5f, phase0);
            fillSineCont (buffer, 1, 220.0, 0.5f, phase1);
            proc.processBlock (buffer, midi);
            const float* ch0 = buffer.getReadPointer (0);
            const float edge = std::fabs (ch0[0] - prevLast);
            if (edge > worstEdge)
            {
                worstEdge = edge;
                worstEdgeBlock = b;
            }
            worst = juce::jmax (worst, edge);
            worst = juce::jmax (worst, maxStepJump (ch0, kBlock, b));
            prevLast = ch0[kBlock - 1];
        }
        std::printf ("  edge-worst %.5f @blk %d\n", worstEdge, worstEdgeBlock);
        return worst;
    };

    const float offJump = runToggleBlocks (0.0f);
    std::printf ("active off-toggle: steady %.5f, toggle %.5f @blk %d idx %d (expect < 0.06)\n", steadyJump, offJump,
                 gWorstBlock, gWorstIdx);
    CHECK (offJump < 0.06f);

    const float onJump = runToggleBlocks (1.0f);
    std::printf ("active on-toggle: steady %.5f, toggle %.5f (expect < 0.06)\n", steadyJump, onJump);
    CHECK (onJump < 0.06f);
}

} // namespace

int main ()
{
    checkMonoInStereoOut();
    checkStereoIndependent();
    checkMonoInMonoOut();
    checkHostBypass();
    checkActiveOffPassthrough();
    checkActiveToggleNoClick();

    if (failures == 0)
    {
        std::puts ("ProcessorIOTest: all checks passed");
        return 0;
    }

    std::printf ("ProcessorIOTest: %d check(s) FAILED\n", failures);
    return 1;
}
