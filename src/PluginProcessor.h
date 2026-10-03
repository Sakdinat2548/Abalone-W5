#pragma once

#include <array>

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/ProcessorChain.h"

class AbaloneW5AudioProcessor : public juce::AudioProcessor
{
public:
    AbaloneW5AudioProcessor ();
    ~AbaloneW5AudioProcessor () override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources () override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Host bypass: TRUE passthrough — the buffer is left untouched (zero DSP,
    // chain states frozen), exactly like ACTIVE off. The input peak is still
    // tracked so the SIGNAL LED follows the input while bypassed.
    // Mono-in exception: with a single main-bus input and >= 2 buffer
    // channels, ch0 is copied to the extra channels (same dual-mono rule as
    // processBlock) since they hold no input data.
    // JUCE 8.0.15 has no AudioProcessor::isBypassed(); the host calls this
    // INSTEAD of processBlock, which is the equivalent hook.
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor () override;
    bool hasEditor () const override;

    const juce::String getName () const override;

    bool acceptsMidi () const override;
    bool producesMidi () const override;
    bool isMidiEffect () const override;
    double getTailLengthSeconds () const override;

    int getNumPrograms () override;
    int getCurrentProgram () override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getApvts () { return apvts; }

    // Max LED peak since the last call (resets on read). Normal operation:
    // the pre-trim (post-HighCut, pre-trim-gain) chain peak — the hardware
    // has no trim so its LED can't see one. Bypassed (ACTIVE off or host
    // bypass): the input peak, since passthrough has no output stage to
    // read. The editor LED timer polls this on the message thread; the
    // atomics are lock-free.
    float getSignalPeak ();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout ();

    // Tracks max |input| over the kept channels into bypassPeak_ (read side
    // of the chains is never touched: bypass runs zero DSP, states frozen).
    void trackBypassPeak (const juce::AudioBuffer<float>& buffer, int activeChannels, int numSamples);

    void pushChainParams (int boostStep, int tone, bool highcut, float trimDb, int osFactor, int activeChannels);

    juce::AudioProcessorValueTreeState apvts;
    std::array<ProcessorChain, 2> chains;
    // Last latency reported via setLatencySamples (0 at 1x, the chain's
    // exact hot-path FIR delay at 2x/4x). Updated only on change: the
    // factor switch flips it in processBlock, the rate path in prepareToPlay.
    int lastReportedLatency_ = 0;
    // Input-peak accumulator for the bypassed SIGNAL LED (reset-on-read).
    // Written on the audio thread, drained on the message thread.
    mutable std::atomic<float> bypassPeak_{0.0f};
    // Last rate from OUR prepareToPlay override (hosts also feed JUCE's base
    // rate via setPlayConfigDetails, but headless tests only call prepare —
    // so DSP math (bypass fade length) reads this, never getSampleRate()).
    double preparedSampleRate_ = 48000.0;
    // ACTIVE crossfade state: bypassMix_ is the dry weight (1 = full dry
    // passthrough, 0 = fully engaged). Steady states take the existing fast
    // paths untouched; only mid-transition blocks run the 5ms equal-power
    // fade below, so idle CPU is unchanged. firstAudioBlock_ snaps the mix
    // to the loaded state (no fade-in on plugin/preset load — fades only
    // cover live toggles, which is the click being fixed).
    float bypassMix_ = 0.0f;
    bool firstAudioBlock_ = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbaloneW5AudioProcessor)
};
