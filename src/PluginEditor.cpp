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

// Engraved-plate lettering (two-pass: pale groove highlight below, dark face
// on top) for the in-code ABALONE wordmark. No vendored font: the JUCE
// default sans at wide tracking reads as an engraved badge on the brushed
// plate, and keeps BinaryData to plate/knob art only.
void drawEngravedCentred (juce::Graphics& g, const juce::String& text, float cx, float cyMid, float fontSize,
                          float trackingPx)
{
    const juce::Font font = juce::Font{juce::FontOptions (fontSize)}.boldened();
    float total = 0.0f;
    for (int i = 0; i < text.length(); ++i)
        total += juce::GlyphArrangement::getStringWidth (font, text.substring (i, i + 1));
    total += trackingPx * static_cast<float> (juce::jmax (0, text.length() - 1));

    float x = cx - total * 0.5f;
    g.setFont (font);
    for (int i = 0; i < text.length(); ++i)
    {
        const juce::String ch = text.substring (i, i + 1);
        const float w = juce::GlyphArrangement::getStringWidth (font, ch);
        const juce::Rectangle<float> r (x, cyMid - fontSize * 0.5f, w, fontSize);
        g.setColour (juce::Colour (0x8cf4f6f8));
        g.drawText (ch, r.translated (0.0f, 1.0f), juce::Justification::centred, false);
        g.setColour (juce::Colour (0xff2e3234));
        g.drawText (ch, r, juce::Justification::centred, false);
        x += w + trackingPx;
    }
}

} // namespace

AbaloneW5AudioProcessorEditor::AbaloneW5AudioProcessorEditor (AbaloneW5AudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // PNG skins + layout CSV are decoded/parsed once here on the message
    // thread, never on audio.
    faceImage = imageFromBinary (BinaryData::u5_front_clean_png, BinaryData::u5_front_clean_pngSize);
    boostDialLookAndFeel.bodyImage =
        imageFromBinary (BinaryData::knob_boost_no_pointer_png, BinaryData::knob_boost_no_pointer_pngSize);
    toneDialLookAndFeel.bodyImage =
        imageFromBinary (BinaryData::knob_tone_no_pointer_png, BinaryData::knob_tone_no_pointer_pngSize);
    // TRIM is the one deliberate addition: a mini-knob in the same chrome
    // family (tone art scaled down) with the tone pointer.
    trimDialLookAndFeel.bodyImage = toneDialLookAndFeel.bodyImage;
    boostDialLookAndFeel.pointerImage =
        imageFromBinary (BinaryData::pointer_boost_png, BinaryData::pointer_boost_pngSize);
    toneDialLookAndFeel.pointerImage = imageFromBinary (BinaryData::pointer_tone_png, BinaryData::pointer_tone_pngSize);
    trimDialLookAndFeel.pointerImage = imageFromBinary (BinaryData::pointer_trim_png, BinaryData::pointer_trim_pngSize);
    toggleLookAndFeel.onImage = imageFromBinary (BinaryData::button_on_png, BinaryData::button_on_pngSize);
    toggleLookAndFeel.offImage = imageFromBinary (BinaryData::button_off_png, BinaryData::button_off_pngSize);
    ledOnImage = imageFromBinary (BinaryData::led_on_png, BinaryData::led_on_pngSize);
    ledOffImage = imageFromBinary (BinaryData::led_off_png, BinaryData::led_off_pngSize);

    layoutRatios = parseLayoutCsv (BinaryData::component_positions_csv, BinaryData::component_positions_csvSize);

    // Needle sweeps, clockwise-from-12, fitted against the baked tick RAYS
    // (end ticks exact; photo perspective leaves mid-scale residuals, worst
    // ~10deg near the top where the print skews most — linear is all an
    // attached/linear slider can do). Boost 1->209.0deg, 10->150.5deg;
    // tone 1->268.3deg, 6->91.0deg. TRIM has no printed scale at its new
    // spot, so it keeps the conventional 7-to-5-o'clock sweep.
    boostDialLookAndFeel.needleStartDeg = 209.0f;
    boostDialLookAndFeel.needleSweepDeg = 301.5f;
    toneDialLookAndFeel.needleStartDeg = 268.3f;
    toneDialLookAndFeel.needleSweepDeg = 182.7f;
    trimDialLookAndFeel.needleStartDeg = 225.0f;
    trimDialLookAndFeel.needleSweepDeg = 270.0f;

    // Rotation axles, fractions of the 224px knob frame (rim-circle fits of
    // the photo art — not the frame centers — so the pointer pivots about
    // the knob's own axle).
    boostDialLookAndFeel.pivotX = 107.9f / 224.0f;
    boostDialLookAndFeel.pivotY = 111.6f / 224.0f;
    toneDialLookAndFeel.pivotX = 113.2f / 224.0f;
    toneDialLookAndFeel.pivotY = 113.0f / 224.0f;
    trimDialLookAndFeel.pivotX = toneDialLookAndFeel.pivotX;
    trimDialLookAndFeel.pivotY = toneDialLookAndFeel.pivotY;

    boostSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    boostSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    boostSlider.setRange (0.0, 9.0, 1.0);
    boostSlider.textFromValueFunction = [] (double v) { return juce::String (static_cast<int> (v) + 1); };
    boostSlider.valueFromTextFunction = [] (const juce::String& t)
    { return static_cast<double> (juce::jlimit (0, 9, t.getIntValue() - 1)); };
    boostSlider.setLookAndFeel (&boostDialLookAndFeel);
    addAndMakeVisible (boostSlider);

    // Manual tone knob, 1-6 only, NOT attached (see header note). Turning it
    // while bypassed writes `tone` but never engages and never changes the
    // sound; the knob always shows the `tone` param, even while bypassed.
    toneSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    toneSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    toneSlider.setRange (1.0, 6.0, 1.0);
    toneSlider.setLookAndFeel (&toneDialLookAndFeel);
    toneSlider.onValueChange = [this]
    {
        const int value = static_cast<int> (toneSlider.getValue());
        if (auto* param = processor.getApvts().getParameter ("tone"))
            param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (value)));
    };
    addAndMakeVisible (toneSlider);

    // Cut-only output trim, attached (range must match the param exactly).
    outputSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outputSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    outputSlider.setRange (-30.0, 0.0, 0.1);
    outputSlider.setLookAndFeel (&trimDialLookAndFeel);
    addAndMakeVisible (outputSlider);

    highcutButton.setLookAndFeel (&toggleLookAndFeel);
    highcutButton.setClickingTogglesState (true);
    addAndMakeVisible (highcutButton);

    // Both red buttons are plain attachments: TONE drives `toneIn`, ACTIVE
    // drives `active`. Sync (incl. first paint) is the attachments' job;
    // the timer never touches them.
    toneEngageButton.setLookAndFeel (&toggleLookAndFeel);
    toneEngageButton.setClickingTogglesState (true);
    addAndMakeVisible (toneEngageButton);

    activeButton.setLookAndFeel (&toggleLookAndFeel);
    activeButton.setClickingTogglesState (true);
    addAndMakeVisible (activeButton);

    // SPEAKER is a hardware-only tap: permanent OFF image, non-interactive.
    speakerImage.setImage (toggleLookAndFeel.offImage);
    speakerImage.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (speakerImage);

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
    toneInAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "toneIn", toneEngageButton);
    activeAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, "active", activeButton);

    // The tone knob keeps its position while bypassed: it always shows the
    // `tone` param (1-6). A stored Bypass (old preset/automation writing 0
    // directly) has no knob position, so the knob parks at the default
    // Tone 3 — the sound still follows `toneIn ? tone : 0` either way.
    if (auto* param = apvts.getParameter ("tone"))
    {
        const int toneIndex = static_cast<int> (param->getValue() * 6.0f + 0.5f);
        toneSlider.setValue (static_cast<double> (toneIndex != 0 ? toneIndex : 3), juce::dontSendNotification);
    }

    setSize (kEditorWidth, kEditorHeight);
    startTimerHz (30);
}

AbaloneW5AudioProcessorEditor::~AbaloneW5AudioProcessorEditor ()
{
    stopTimer();
    highcutButton.setLookAndFeel (nullptr);
    toneEngageButton.setLookAndFeel (nullptr);
    activeButton.setLookAndFeel (nullptr);
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

    // ABALONE wordmark, engraved style, where AVALON sat (top-center band;
    // shifted left of the old center: TRIM took the band's right end).
    // Texture (950,189) -> screen; ~64px texture cap-height. The editor is
    // aspect-locked to the texture, so texture ratios map 1:1.
    const float sx = static_cast<float> (getWidth()) / 2136.0f;
    const float sy = static_cast<float> (getHeight()) / 970.0f;
    drawEngravedCentred (g, "ABALONE", 950.0f * sx, 189.0f * sy, 31.0f, 4.0f);
}

void AbaloneW5AudioProcessorEditor::resized ()
{
    const int w = getWidth();
    const int h = getHeight();
    boostSlider.setBounds (scaledRect (layoutRatios, "boost_dial", w, h));
    toneSlider.setBounds (scaledRect (layoutRatios, "tone_dial", w, h));
    outputSlider.setBounds (scaledRect (layoutRatios, "trim_dial", w, h));
    highcutButton.setBounds (scaledRect (layoutRatios, "highcut_button", w, h));
    speakerImage.setBounds (scaledRect (layoutRatios, "speaker_button", w, h));
    toneEngageButton.setBounds (scaledRect (layoutRatios, "tone_button", w, h));
    activeButton.setBounds (scaledRect (layoutRatios, "active_button", w, h));
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

    // The knob always reflects the `tone` param — engaged or bypassed (like
    // the hardware knob sitting where you left it) — but never fights a
    // live drag. A stored Bypass has no knob position: leave the knob alone.
    if (auto* param = processor.getApvts().getParameter ("tone"))
    {
        const int toneIndex = static_cast<int> (param->getValue() * 6.0f + 0.5f);
        if (toneIndex != 0 && !toneSlider.isMouseButtonDown() && static_cast<int> (toneSlider.getValue()) != toneIndex)
            toneSlider.setValue (static_cast<double> (toneIndex), juce::dontSendNotification);
    }
}
