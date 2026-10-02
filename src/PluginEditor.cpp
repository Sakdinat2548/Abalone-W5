#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>

#include <BinaryData.h>

namespace
{

juce::Image imageFromBinary (const void* data, int size) { return juce::ImageCache::getFromMemory (data, size); }

// Trajan-class plate face: single Cinzel Black 900 typeface (OFL, embedded
// as BinaryData — stroke-matched to the hardware badge, see the header
// note). Falls back to the default system font (never blank) if the embed
// ever fails to parse. Built once per font at editor construction. The
// loader keys off the BinaryData symbol only, so a future face swap touches
// just this line + CMake SOURCES.
juce::Font makePlateFont (float height)
{
    if (auto face =
            juce::Typeface::createSystemTypefaceFor (BinaryData::CinzelBlack_ttf, BinaryData::CinzelBlack_ttfSize))
        return juce::Font (juce::FontOptions (face).withHeight (height));
    return juce::Font (juce::FontOptions (height));
}

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
// on top) for the in-code ABALONE wordmark. The face is Cinzel Black 900
// (OFL Trajan-class serif, vendored via BinaryData — stroke-matched to the
// hardware AVALON badge at matched cap-height); the caller passes the
// editor's true horizontal center so the wordmark is optically centered
// across the full panel via the measured text width (no hardcoded x that
// can drift with letterforms).
void drawEngravedCentred (juce::Graphics& g, const juce::Font& font, const juce::String& text, float cx, float cyMid,
                          float trackingPx)
{
    const float fontSize = font.getHeight();
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
    : AudioProcessorEditor (&p), processor (p), headerFont (makePlateFont (32.0f)), readoutFont (makePlateFont (9.0f))
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
    // (re-measured fix round 3: radial min-dark scan, 0.1deg steps, r118-165;
    // end ticks exact, detents land exactly on them with zero overtravel).
    // Boost 1->209.0deg, 10->150.5deg (plateau centers 208.9/150.6, within
    // 0.1deg — sub-pixel at tick radius); tone 1->268.3deg, 6->91.3deg.
    // Photo perspective leaves mid-scale residuals a linear slider cannot
    // follow. TRIM has no printed scale at its oval spot, so it keeps the
    // conventional 7-to-5-o'clock sweep.
    boostDialLookAndFeel.needleStartDeg = 209.0f;
    boostDialLookAndFeel.needleSweepDeg = 301.5f;
    toneDialLookAndFeel.needleStartDeg = 268.3f;
    toneDialLookAndFeel.needleSweepDeg = 183.0f;
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

    // In-code dB readout below the trim knob, following the output param
    // (set in resized(); text refreshed in timerCallback). Pale on the black
    // oval; Cinzel Black at a subordinate size, non-interactive.
    trimReadout.setInterceptsMouseClicks (false, false);
    trimReadout.setJustificationType (juce::Justification::centred);
    trimReadout.setColour (juce::Label::textColourId, juce::Colour (0xffe9ebee));
    trimReadout.setFont (readoutFont);
    addAndMakeVisible (trimReadout);

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

    // Lights-off veil: ordered below the POWER LED (which stays bright as
    // the mains lamp) but above every other control. Non-interactive so
    // knob drags pass straight through to the controls beneath.
    dimOverlay.setInterceptsMouseClicks (false, false);
    dimOverlay.setVisible (false);
    addAndMakeVisible (dimOverlay);

    // Blue POWER LED: mains lamp, ALWAYS lit while the plugin is open,
    // independent of ACTIVE. Added after the veil so it paints above it
    // and stays bright while everything else dims.
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

    // Initial veil state from the `active` param (the timer keeps it live;
    // this covers the first paint). POWER is a mains lamp — always on.
    powerOn = true;
    powerLedImage.setImage (ledOnImage);
    if (auto* activeParam = apvts.getParameter ("active"))
    {
        dimVisible = activeParam->getValue() <= 0.5f;
        dimOverlay.setVisible (dimVisible);
    }

    // Initial readout text (the timer refreshes it; this covers first paint).
    if (auto* outParam = apvts.getParameter ("output"))
    {
        lastTrimText = outParam->getCurrentValueAsText() + " dB";
        trimReadout.setText (lastTrimText, juce::dontSendNotification);
    }

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

    // ABALONE wordmark, engraved style, optically centered across the full
    // panel: cx is the editor's true horizontal center and the draw routine
    // centers via the measured Cinzel Black text width. Texture y=98.5 is the
    // measured AVALON cap-center on the cropped faceplate (caps 74-123). The
    // editor is aspect-locked to the texture, so texture ratios map 1:1.
    // JUCE maps font height across the full ascent+descent cell, so the
    // measured cap at 32px is ~35px @2x = ~49px texture, matching the
    // hardware badge cap-height (50px) with matched stroke weight.
    const float sy = static_cast<float> (getHeight()) / 867.0f;
    drawEngravedCentred (g, headerFont, "ABALONE", static_cast<float> (getWidth()) * 0.5f, 98.5f * sy, 8.0f);
}

void AbaloneW5AudioProcessorEditor::resized ()
{
    const int w = getWidth();
    const int h = getHeight();
    boostSlider.setBounds (scaledRect (layoutRatios, "boost_dial", w, h));
    toneSlider.setBounds (scaledRect (layoutRatios, "tone_dial", w, h));
    const auto trimBounds = scaledRect (layoutRatios, "trim_dial", w, h);
    outputSlider.setBounds (trimBounds);
    // Readout sits directly below the trim knob, wider than the knob so the
    // "-30.0 dB" string fits; fixed-size editor so px constants are stable.
    trimReadout.setBounds (trimBounds.getX() - 24, trimBounds.getBottom() + 2, trimBounds.getWidth() + 48, 14);
    highcutButton.setBounds (scaledRect (layoutRatios, "highcut_button", w, h));
    speakerImage.setBounds (scaledRect (layoutRatios, "speaker_button", w, h));
    toneEngageButton.setBounds (scaledRect (layoutRatios, "tone_button", w, h));
    activeButton.setBounds (scaledRect (layoutRatios, "active_button", w, h));
    signalLedImage.setBounds (scaledRect (layoutRatios, "signal_led", w, h));
    powerLedImage.setBounds (scaledRect (layoutRatios, "power_led", w, h));
    dimOverlay.setBounds (0, 0, w, h);
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

    // Lights-off veil follows ACTIVE (lifts instantly on re-engage),
    // driven here on the existing 30Hz timer — no new threads, no fading.
    // POWER is a mains lamp and is never driven dark.
    if (auto* activeParam = processor.getApvts().getParameter ("active"))
    {
        const bool shouldDim = activeParam->getValue() <= 0.5f;
        if (shouldDim != dimVisible)
        {
            dimVisible = shouldDim;
            dimOverlay.setVisible (dimVisible);
        }
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

    // TRIM dB readout follows the output param (repaint only on change).
    if (auto* outParam = processor.getApvts().getParameter ("output"))
    {
        const juce::String text = outParam->getCurrentValueAsText() + " dB";
        if (text != lastTrimText)
        {
            lastTrimText = text;
            trimReadout.setText (text, juce::dontSendNotification);
        }
    }
}
