#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <BinaryData.h>

namespace
{

juce::Image imageFromBinary (const void* data, int size) { return juce::ImageCache::getFromMemory (data, size); }

} // namespace

AbaloneW5AudioProcessorEditor::AbaloneW5AudioProcessorEditor (AbaloneW5AudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // PNG skins are decoded once here on the message thread, never on audio.
    // knob_boost/knob_tone.png are single 224x224 frames (not vertical
    // filmstrips), so the rotaries below stay stock; buttons and LED are
    // single on/off frames, which suit toggles and the LED directly.
    toggleLookAndFeel.onImage = imageFromBinary (BinaryData::button_on_png, BinaryData::button_on_pngSize);
    toggleLookAndFeel.offImage = imageFromBinary (BinaryData::button_off_png, BinaryData::button_off_pngSize);
    ledOnImage = imageFromBinary (BinaryData::led_on_png, BinaryData::led_on_pngSize);
    ledOffImage = imageFromBinary (BinaryData::led_off_png, BinaryData::led_off_pngSize);

    boostSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    boostSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 20);
    boostSlider.setRange (0.0, 9.0, 1.0);
    boostSlider.textFromValueFunction = [] (double v) { return juce::String (static_cast<int> (v) + 1); };
    boostSlider.valueFromTextFunction = [] (const juce::String& t)
    { return static_cast<double> (juce::jlimit (0, 9, t.getIntValue() - 1)); };
    addAndMakeVisible (boostSlider);

    toneSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    toneSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 20);
    toneSlider.setRange (0.0, 6.0, 1.0);
    toneSlider.textFromValueFunction = [] (double v)
    { return v < 0.5 ? juce::String ("Bypass") : "Tone " + juce::String (static_cast<int> (v)); };
    toneSlider.valueFromTextFunction = [] (const juce::String& t)
    { return static_cast<double> (juce::jlimit (0, 6, t.getIntValue())); };
    addAndMakeVisible (toneSlider);

    outputSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outputSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 20);
    outputSlider.setRange (-12.0, 12.0, 0.1);
    outputSlider.setTextValueSuffix (" dB");
    addAndMakeVisible (outputSlider);

    highcutButton.setLookAndFeel (&toggleLookAndFeel);
    highcutButton.setClickingTogglesState (true);
    addAndMakeVisible (highcutButton);

    // Manual (unattached) toggle: armed = tone index != Bypass. Clicking it
    // parks the tone at Bypass or restores the last non-bypass tone.
    toneEngageButton.setLookAndFeel (&toggleLookAndFeel);
    toneEngageButton.setClickingTogglesState (true);
    toneEngageButton.onClick = [this]
    {
        auto* param = processor.getApvts().getParameter ("tone");
        if (param == nullptr)
            return;
        if (toneEngageButton.getToggleState())
        {
            param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (lastToneIndex)));
        }
        else
        {
            const int current = static_cast<int> (param->getValue() * 6.0f + 0.5f);
            if (current != 0)
                lastToneIndex = current;
            param->setValueNotifyingHost (param->convertTo0to1 (0.0f));
        }
    };
    addAndMakeVisible (toneEngageButton);

    boostLabel.setText ("Boost", juce::dontSendNotification);
    boostLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (boostLabel);

    toneLabel.setText ("Tone", juce::dontSendNotification);
    toneLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (toneLabel);

    outputLabel.setText ("Output", juce::dontSendNotification);
    outputLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (outputLabel);

    highcutLabel.setText ("High Cut", juce::dontSendNotification);
    highcutLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (highcutLabel);

    toneEngageLabel.setText ("Tone In", juce::dontSendNotification);
    toneEngageLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (toneEngageLabel);

    ledLabel.setText ("Signal", juce::dontSendNotification);
    ledLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (ledLabel);

    ledImage.setImage (ledOffImage);
    addAndMakeVisible (ledImage);

    auto& apvts = processor.getApvts();
    boostAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "boost", boostSlider);
    toneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "tone", toneSlider);
    outputAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "output", outputSlider);
    highcutAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "highcut", highcutButton);

    setSize (400, 300);
    startTimerHz (30);
}

AbaloneW5AudioProcessorEditor::~AbaloneW5AudioProcessorEditor ()
{
    stopTimer();
    highcutButton.setLookAndFeel (nullptr);
    toneEngageButton.setLookAndFeel (nullptr);
}

void AbaloneW5AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1c20));
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawFittedText ("Abalone W5", 0, 6, getWidth(), 24, juce::Justification::centred, 1);
}

void AbaloneW5AudioProcessorEditor::resized ()
{
    boostLabel.setBounds (15, 34, 115, 18);
    boostSlider.setBounds (15, 52, 115, 125);
    toneLabel.setBounds (145, 34, 115, 18);
    toneSlider.setBounds (145, 52, 115, 125);
    outputLabel.setBounds (275, 34, 110, 18);
    outputSlider.setBounds (275, 52, 110, 125);

    highcutButton.setBounds (20, 205, 103, 48);
    highcutLabel.setBounds (20, 253, 103, 18);
    toneEngageButton.setBounds (148, 205, 103, 48);
    toneEngageLabel.setBounds (148, 253, 103, 18);
    ledImage.setBounds (310, 205, 48, 48);
    ledLabel.setBounds (288, 253, 92, 18);
}

void AbaloneW5AudioProcessorEditor::timerCallback ()
{
    const float peak = processor.getSignalPeak();
    const bool shouldBeOn = peak >= kLedThreshold;
    if (shouldBeOn != ledOn)
    {
        ledOn = shouldBeOn;
        ledImage.setImage (ledOn ? ledOnImage : ledOffImage);
    }

    if (auto* param = processor.getApvts().getParameter ("tone"))
    {
        const int toneIndex = static_cast<int> (param->getValue() * 6.0f + 0.5f);
        const bool engaged = toneIndex != 0;
        if (engaged != toneEngageButton.getToggleState())
            toneEngageButton.setToggleState (engaged, juce::dontSendNotification);
        if (engaged)
            lastToneIndex = toneIndex;
    }
}
