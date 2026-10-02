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

    void pushChainParams (int boostStep, int tone, bool highcut, float trimDb, int activeChannels);

    juce::AudioProcessorValueTreeState apvts;
    std::array<ProcessorChain, 2> chains;
    // Input-peak accumulator for the bypassed SIGNAL LED (reset-on-read).
    // Written on the audio thread, drained on the message thread.
    mutable std::atomic<float> bypassPeak_{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbaloneW5AudioProcessor)
};
