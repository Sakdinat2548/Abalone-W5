#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

class AbaloneW5AudioProcessor;

// Toggle look-and-feel driven by the user's button_on/off.png pair (loaded
// once in the editor constructor, never on the audio thread).
struct PngToggleLookAndFeel : public juce::LookAndFeel_V4
{
    juce::Image onImage;
    juce::Image offImage;

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool, bool) override
    {
        const juce::Image& img = button.getToggleState() ? onImage : offImage;
        if (img.isValid())
            g.drawImageWithin (img, 0, 0, button.getWidth(), button.getHeight(), juce::RectanglePlacement::centred);
    }
};

class AbaloneW5AudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AbaloneW5AudioProcessorEditor (AbaloneW5AudioProcessor&);
    ~AbaloneW5AudioProcessorEditor () override;

    void paint (juce::Graphics&) override;
    void resized () override;

private:
    void timerCallback () override;

    // -2dBFS signal-present threshold (spec: LED is signal-present, not clip).
    static constexpr float kLedThreshold = 0.79432823f; // 10^(-2/20).

    AbaloneW5AudioProcessor& processor;

    juce::Slider boostSlider;
    juce::Slider toneSlider;
    juce::Slider outputSlider;
    juce::ToggleButton highcutButton;
    juce::ToggleButton toneEngageButton;
    juce::Label boostLabel;
    juce::Label toneLabel;
    juce::Label outputLabel;
    juce::Label highcutLabel;
    juce::Label toneEngageLabel;
    juce::Label ledLabel;
    juce::ImageComponent ledImage;

    PngToggleLookAndFeel toggleLookAndFeel;
    juce::Image ledOnImage;
    juce::Image ledOffImage;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> boostAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> toneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> highcutAttachment;

    int lastToneIndex = 3; // restored when the tone-engage toggle is re-armed.
    bool ledOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbaloneW5AudioProcessorEditor)
};
