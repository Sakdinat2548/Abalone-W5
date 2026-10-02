#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>

#include <BinaryData.h>

namespace
{

juce::Image imageFromBinary (const void* data, int size) { return juce::ImageCache::getFromMemory (data, size); }

// Parses the embedded ui/component_positions.csv into name -> ratio rect.
// Runs once on the message thread at construction; no audio-thread use.
std::map<juce::String, juce::Rectangle<float>> parseLayoutCsv (const char* data, int size)
{
    std::map<juce::String, juce::Rectangle<float>> out;
    juce::StringArray lines = juce::StringArray::fromLines (juce::String::fromUTF8 (data, size));
    for (const auto& line : lines)
    {
        juce::String trimmed = line.trim();
        if (trimmed.isEmpty() || trimmed.startsWithChar ('#') || trimmed.startsWith ("name,"))
            continue;
        // name,cx,cy,w,h,note — the note column never contains commas.
        juce::StringArray cols = juce::StringArray::fromTokens (trimmed, ",", "");
        if (cols.size() < 5)
            continue;
        out[cols[0].trim()] = juce::Rectangle<float> (cols[1].getFloatValue(), cols[2].getFloatValue(),
                                                      cols[3].getFloatValue(), cols[4].getFloatValue());
    }
    return out;
}

juce::Rectangle<int> scaledRect (const std::map<juce::String, juce::Rectangle<float>>& layout, const juce::String& name,
                                 int w, int h)
{
    const auto it = layout.find (name);
    jassert (it != layout.end()); // every live element must be in the CSV.
    if (it == layout.end())
        return {};
    const auto& r = it->second;
    return juce::Rectangle<int> (juce::roundToInt ((r.getX() - r.getWidth() * 0.5f) * static_cast<float> (w)),
                                 juce::roundToInt ((r.getY() - r.getHeight() * 0.5f) * static_cast<float> (h)),
                                 juce::roundToInt (r.getWidth() * static_cast<float> (w)),
                                 juce::roundToInt (r.getHeight() * static_cast<float> (h)));
}

} // namespace

AbaloneW5AudioProcessorEditor::AbaloneW5AudioProcessorEditor (AbaloneW5AudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // PNG skins + layout CSV are decoded/parsed once here on the message
    // thread, never on audio.
    faceImage = imageFromBinary (BinaryData::u5_front_clean_png, BinaryData::u5_front_clean_pngSize);
    boostDialLookAndFeel.knobImage =
        imageFromBinary (BinaryData::knob_boost_no_pointer_png, BinaryData::knob_boost_no_pointer_pngSize);
    toneDialLookAndFeel.knobImage =
        imageFromBinary (BinaryData::knob_tone_no_pointer_png, BinaryData::knob_tone_no_pointer_pngSize);
    trimDialLookAndFeel.knobImage = boostDialLookAndFeel.knobImage;
    toggleLookAndFeel.onImage = imageFromBinary (BinaryData::button_on_png, BinaryData::button_on_pngSize);
    toggleLookAndFeel.offImage = imageFromBinary (BinaryData::button_off_png, BinaryData::button_off_pngSize);
    ledOnImage = imageFromBinary (BinaryData::led_on_png, BinaryData::led_on_pngSize);
    ledOffImage = imageFromBinary (BinaryData::led_off_png, BinaryData::led_off_pngSize);

    layoutRatios = parseLayoutCsv (BinaryData::component_positions_csv, BinaryData::component_positions_csvSize);

    // Needle sweeps measured clockwise-from-12 against the baked dial ticks
    // (least-squares fit through the baked numeral centroids, which sit on
    // the tick rays; residuals +/-10deg from photo perspective).
    boostDialLookAndFeel.needleStartDeg = 219.0f;
    boostDialLookAndFeel.needleSweepDeg = 271.0f;
    toneDialLookAndFeel.needleStartDeg = 267.0f;
    toneDialLookAndFeel.needleSweepDeg = 186.0f;
    trimDialLookAndFeel.needleStartDeg = 225.0f;
    trimDialLookAndFeel.needleSweepDeg = 270.0f;

    boostSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    boostSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    boostSlider.setRange (0.0, 9.0, 1.0);
    boostSlider.textFromValueFunction = [] (double v) { return juce::String (static_cast<int> (v) + 1); };
    boostSlider.valueFromTextFunction = [] (const juce::String& t)
    { return static_cast<double> (juce::jlimit (0, 9, t.getIntValue() - 1)); };
    boostSlider.setLookAndFeel (&boostDialLookAndFeel);
    addAndMakeVisible (boostSlider);

    // Manual tone knob: 1-6 only, NOT attached (see header note). Dragging it
    // writes the param, which re-engages the tone when bypassed.
    toneSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    toneSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    toneSlider.setRange (1.0, 6.0, 1.0);
    toneSlider.setLookAndFeel (&toneDialLookAndFeel);
    toneSlider.onValueChange = [this]
    {
        const int value = static_cast<int> (toneSlider.getValue());
        lastToneIndex = value;
        if (auto* param = processor.getApvts().getParameter ("tone"))
            param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (value)));
    };
    addAndMakeVisible (toneSlider);

    outputSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outputSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    outputSlider.setRange (-12.0, 12.0, 0.1);
    outputSlider.setLookAndFeel (&trimDialLookAndFeel);
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

    signalLedImage.setImage (ledOffImage);
    addAndMakeVisible (signalLedImage);

    // Blue POWER LED: always on, like hardware.
    powerLedImage.setImage (ledOnImage);
    addAndMakeVisible (powerLedImage);

    auto& apvts = processor.getApvts();
    boostAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "boost", boostSlider);
    outputAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "output", outputSlider);
    highcutAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "highcut", highcutButton);

    // The tone knob keeps its position while bypassed: initialise from the
    // param when engaged, else from the default last-tone slot. The engage
    // button is synced here too so the first paint (before the timer fires)
    // already shows the param state.
    if (auto* param = apvts.getParameter ("tone"))
    {
        const int toneIndex = static_cast<int> (param->getValue() * 6.0f + 0.5f);
        if (toneIndex != 0)
        {
            lastToneIndex = toneIndex;
            toneSlider.setValue (static_cast<double> (toneIndex), juce::dontSendNotification);
        }
        else
        {
            toneSlider.setValue (static_cast<double> (lastToneIndex), juce::dontSendNotification);
        }
        toneEngageButton.setToggleState (toneIndex != 0, juce::dontSendNotification);
    }

    setSize (kEditorWidth, kEditorHeight);
    startTimerHz (30);
}

AbaloneW5AudioProcessorEditor::~AbaloneW5AudioProcessorEditor ()
{
    stopTimer();
    highcutButton.setLookAndFeel (nullptr);
    toneEngageButton.setLookAndFeel (nullptr);
    boostSlider.setLookAndFeel (nullptr);
    toneSlider.setLookAndFeel (nullptr);
    outputSlider.setLookAndFeel (nullptr);
}

void AbaloneW5AudioProcessorEditor::paint (juce::Graphics& g)
{
    // Faceplate photo fills the editor 1:1 (sizes are aspect-locked).
    if (faceImage.isValid())
        g.drawImageWithin (faceImage, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::stretchToFit);
    else
        g.fillAll (juce::Colour (0xff1a1c20));
}

void AbaloneW5AudioProcessorEditor::resized ()
{
    const int w = getWidth();
    const int h = getHeight();
    boostSlider.setBounds (scaledRect (layoutRatios, "boost_dial", w, h));
    toneSlider.setBounds (scaledRect (layoutRatios, "tone_dial", w, h));
    outputSlider.setBounds (scaledRect (layoutRatios, "trim_dial", w, h));
    highcutButton.setBounds (scaledRect (layoutRatios, "highcut_button", w, h));
    toneEngageButton.setBounds (scaledRect (layoutRatios, "tone_button", w, h));
    signalLedImage.setBounds (scaledRect (layoutRatios, "signal_led", w, h));
    powerLedImage.setBounds (scaledRect (layoutRatios, "power_led", w, h));
}

void AbaloneW5AudioProcessorEditor::timerCallback ()
{
    const float peak = processor.getSignalPeak();
    const bool shouldBeOn = peak >= kLedThreshold;
    if (shouldBeOn != ledOn)
    {
        ledOn = shouldBeOn;
        signalLedImage.setImage (ledOn ? ledOnImage : ledOffImage);
    }

    if (auto* param = processor.getApvts().getParameter ("tone"))
    {
        const int toneIndex = static_cast<int> (param->getValue() * 6.0f + 0.5f);
        const bool engaged = toneIndex != 0;
        if (engaged != toneEngageButton.getToggleState())
            toneEngageButton.setToggleState (engaged, juce::dontSendNotification);
        if (engaged)
        {
            lastToneIndex = toneIndex;
            // Follow preset/automation recall, but never fight a live drag.
            if (!toneSlider.isMouseButtonDown() && static_cast<int> (toneSlider.getValue()) != toneIndex)
                toneSlider.setValue (static_cast<double> (toneIndex), juce::dontSendNotification);
        }
    }
}
