#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class AbaloneW5AudioProcessor;

class AbaloneW5AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AbaloneW5AudioProcessorEditor (AbaloneW5AudioProcessor&);
    ~AbaloneW5AudioProcessorEditor () override;

    void paint (juce::Graphics&) override;
    void resized () override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbaloneW5AudioProcessorEditor)
};
