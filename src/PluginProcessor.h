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

    // Host bypass: bit-transparent passthrough that keeps the chains'
    // processing state advancing (the input is run through the chains and
    // the result discarded), so re-engaging clicks no more than the natural
    // signal return. JUCE 8.0.15 has no AudioProcessor::isBypassed(); the
    // host calls this INSTEAD of processBlock, which is the equivalent hook.
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

    // Max chain peak since the last call (resets on read). The editor LED
    // timer polls this on the message thread; the atomics are lock-free.
    float getSignalPeak ();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout ();

    // Runs the input through the chains and discards the result, keeping
    // filter state advancing while the buffer (still the input) is untouched.
    void advanceChains (juce::AudioBuffer<float>& buffer, int activeChannels, int numSamples);

    void pushChainParams (int boostStep, int tone, bool highcut, float trimDb, int activeChannels);

    juce::AudioProcessorValueTreeState apvts;
    std::array<ProcessorChain, 2> chains;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbaloneW5AudioProcessor)
};
