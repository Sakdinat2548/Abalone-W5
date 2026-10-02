#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class AbaloneU55AudioProcessor;

class AbaloneU55AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AbaloneU55AudioProcessorEditor (AbaloneU55AudioProcessor&);
    ~AbaloneU55AudioProcessorEditor () override;

    void paint (juce::Graphics&) override;
    void resized () override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbaloneU55AudioProcessorEditor)
};
