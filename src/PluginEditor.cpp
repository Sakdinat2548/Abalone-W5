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

// Measured width of the tracked-out header string (shared by the paint
// routines and the click hit-test so the rect always matches the art).
float headerTextWidth (const juce::Font& font, const juce::String& text, float trackingPx)
{
    float total = 0.0f;
    for (int i = 0; i < text.length(); ++i)
        total += juce::GlyphArrangement::getStringWidth (font, text.substring (i, i + 1));
    total += trackingPx * static_cast<float> (juce::jmax (0, text.length() - 1));
    return total;
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
    const float total = headerTextWidth (font, text, trackingPx);

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
    // Starting a trim drag while the readout is being edited cancels the
    // edit (the hide below restores the live value via onEditorHide).
    outputSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outputSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    outputSlider.setRange (-30.0, 0.0, 0.1);
    outputSlider.setLookAndFeel (&trimDialLookAndFeel);
    outputSlider.onDragStart = [this]
    {
        if (trimReadout.isBeingEdited())
            trimReadout.hideEditor (true);
    };
    addAndMakeVisible (outputSlider);

    // OS factor mini-knob in the trim art family (tone body + trim
    // pointer via the shared trim L&F), attached to the `osfactor` Choice.
    // Range 0..2 step 1 matches the Choice indices (1x/2x/4x); the detents
    // make it a 3-position knob. Knob drag + host automation both drive it.
    osSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    osSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    osSlider.setRange (0.0, 2.0, 1.0);
    osSlider.setLookAndFeel (&trimDialLookAndFeel);
    addAndMakeVisible (osSlider);

    // In-code dB readout below the trim knob, following the output param
    // (set in resized(); text refreshed in timerCallback). Pale on the black
    // oval; Cinzel Black at a subordinate size. Single-click editable:
    // Enter commits the typed number to the output param (clamped -30..0,
    // so -40 lands at -30; non-numeric input is ignored and the live value
    // returns), Esc cancels and restores the live value, and focus-loss
    // also exits editing (discards, never traps keyboard focus).
    trimReadout.setEditable (true, false, true);
    trimReadout.setJustificationType (juce::Justification::centred);
    trimReadout.setColour (juce::Label::textColourId, juce::Colour (0xffe9ebee));
    trimReadout.setColour (juce::Label::backgroundWhenEditingColourId, juce::Colour (0xff0a0b0c));
    trimReadout.setColour (juce::Label::textWhenEditingColourId, juce::Colour (0xffe9ebee));
    trimReadout.setColour (juce::Label::outlineWhenEditingColourId, juce::Colour (0xff3a3d40));
    trimReadout.setColour (juce::CaretComponent::caretColourId, juce::Colour (0xffe9ebee));
    trimReadout.setColour (juce::TextEditor::highlightColourId, juce::Colour (0xff4a5a6a));
    trimReadout.setKeyboardType (juce::TextInputTarget::decimalKeyboard);
    trimReadout.setFont (readoutFont);
    trimReadout.onEditorShow = [this]
    {
        // Editing shows the bare number (no " dB" suffix) so a typed value
        // replaces it cleanly; the commit below re-parses with or without it.
        if (auto* ed = trimReadout.getCurrentTextEditor())
        {
            juce::String t = trimReadout.getText().trim();
            if (t.endsWithIgnoreCase ("dB"))
                t = t.dropLastCharacters (2).trim();
            ed->setText (t);
            ed->setHighlightedRegion (juce::Range<int> (0, t.length()));
        }
    };
    trimReadout.onEditorHide = [this]
    {
        // Single commit/cancel point: JUCE moves the typed text into the
        // label BEFORE this fires on the Enter/focus paths, while Esc (and
        // the trim-drag cancel above) leave the pre-edit text — so a changed
        // numeric text commits and anything else falls through to the live
        // restore. The commit wins over automation/preset drift mid-edit.
        auto* outParam = processor.getApvts().getParameter ("output");
        if (outParam == nullptr)
            return;
        if (trimReadout.getText() != lastTrimText)
        {
            juce::String typed = trimReadout.getText().trim();
            if (typed.endsWithIgnoreCase ("dB"))
                typed = typed.dropLastCharacters (2).trim();
            bool hasDigit = false;
            for (const auto ch : typed)
                if (juce::CharacterFunctions::isDigit (ch))
                {
                    hasDigit = true;
                    break;
                }
            if (hasDigit)
            {
                const float clamped = juce::jlimit (-30.0f, 0.0f, typed.getFloatValue());
                outParam->setValueNotifyingHost (outParam->convertTo0to1 (clamped));
            }
        }
        lastTrimText = outParam->getCurrentValueAsText() + " dB";
        trimReadout.setText (lastTrimText, juce::dontSendNotification);
    };
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

    // Blue POWER LED: mains lamp, ALWAYS lit while the plugin is open,
    // independent of ACTIVE. Painted UNDER the dim veil with everything
    // else, so the veil dims it naturally while bypassed (the on-image is
    // set once and never swapped dark).
    powerLedImage.setImage (ledOnImage);
    addAndMakeVisible (powerLedImage);

    // Lights-off veil LAST so it paints over every control including POWER.
    // Non-interactive so knob drags pass straight through to the controls
    // beneath (the readout stays clickable while dimmed).
    dimOverlay.setInterceptsMouseClicks (false, false);
    dimOverlay.setVisible (false);
    addAndMakeVisible (dimOverlay);

    auto& apvts = processor.getApvts();
    boostAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "boost", boostSlider);
    outputAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "output", outputSlider);
    osAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, "osfactor", osSlider);
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

    // Aspect-locked corner-drag resizable, 1x to 2x. All live elements lay
    // out from CSV texture ratios in resized(), so everything scales; the
    // fonts are rebuilt at the window scale there too.
    setResizable (true, true);
    setResizeLimits (kEditorWidth, kEditorHeight, kEditorMaxWidth, kEditorMaxHeight);
    getConstrainer()->setFixedAspectRatio (static_cast<double> (kEditorWidth) / static_cast<double> (kEditorHeight));

    // Masthead SVG (BinaryData drawable, parsed once here on the message
    // thread); the engraved in-code fallback in paint() covers a parse
    // failure so the header is never blank.
    if (auto svg =
            juce::XmlDocument::parse (juce::String::fromUTF8 (BinaryData::abalone_svg, BinaryData::abalone_svgSize)))
        mastheadDrawable = juce::Drawable::createFromSVG (*svg);

    // Initial veil state from the `active` param (the timer keeps it live;
    // this covers the first paint). POWER is a mains lamp — always on.
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
    osSlider.setLookAndFeel (nullptr);
}

void AbaloneW5AudioProcessorEditor::paint (juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();
    const float scale = static_cast<float> (w) / static_cast<float> (kEditorWidth);

    // Faceplate photo fills the editor (aspect-locked, so stretch == 1:1).
    if (faceImage.isValid())
        g.drawImageWithin (faceImage, 0, 0, w, h, juce::RectanglePlacement::stretchToFit);
    else
        g.fillAll (juce::Colour (0xff1a1c20));

    // ABALONE masthead, static black in every OS state: the SVG asset
    // centered in the header slot (texture y=98.5 cap-center, ~50px cap to
    // match the hardware badge; the slot box is padded, drawWithin centres
    // the art). The engraved in-code fallback covers an SVG parse failure.
    const float cyMid = 98.5f * static_cast<float> (h) / 867.0f;
    if (mastheadDrawable != nullptr)
    {
        const float slotW = 620.0f * static_cast<float> (w) / 2136.0f;
        const float slotH = 56.0f * static_cast<float> (h) / 867.0f;
        mastheadDrawable->drawWithin (
            g,
            juce::Rectangle<float> (static_cast<float> (w) * 0.5f - slotW * 0.5f, cyMid - slotH * 0.5f, slotW, slotH),
            juce::RectanglePlacement::centred, 1.0f);
    }
    else
    {
        drawEngravedCentred (g, headerFont, "ABALONE", static_cast<float> (w) * 0.5f, cyMid, 8.0f * scale);
    }

    drawOsLabels (g, scaledRect (layoutRatios, "os_dial", w, h), scale);
}

// OS factor readout, immediately right of the OS mini-knob: ONE live label
// showing the current `osfactor` value only ("1x"/"2x"/"4x"), vertically
// centered on the knob in the dark plate caption colour. It reads the same
// raw Choice index the DSP path reads (clamped identically), so the label
// can never disagree with the audio factor. Glyph size matches the baked
// to-THRU caption beside it: THRU caps span texture rows 703-720 = 18px
// cap-height (measured), i.e. ~6.3 editor px at 1x; a 9px default-sans face
// (cap ratio ~0.7) lands at ~6.3px cap — same cap-height, not eyeballed.
void AbaloneW5AudioProcessorEditor::drawOsLabels (juce::Graphics& g, juce::Rectangle<int> knob, float scale) const
{
    const int osIndex = static_cast<int> (std::round (processor.getApvts().getRawParameterValue ("osfactor")->load()));
    const char* text = (osIndex <= 0) ? "1x" : (osIndex == 1) ? "2x" : "4x";
    g.setFont (juce::Font (juce::FontOptions (9.0f * scale)));
    g.setColour (juce::Colour (0xff2e3234));
    const int x = knob.getRight() + juce::roundToInt (4.0f * scale);
    const int lw = juce::roundToInt (20.0f * scale);
    g.drawText (text, x, knob.getY(), lw, knob.getHeight(), juce::Justification::centredLeft, false);
}

void AbaloneW5AudioProcessorEditor::resized ()
{
    const int w = getWidth();
    const int h = getHeight();
    const float scale = static_cast<float> (w) / static_cast<float> (kEditorWidth);

    // Typefaces follow the window scale (rebuilt here on the message thread,
    // never on audio).
    headerFont = makePlateFont (32.0f * scale);
    readoutFont = makePlateFont (9.0f * scale);
    trimReadout.setFont (readoutFont);

    boostSlider.setBounds (scaledRect (layoutRatios, "boost_dial", w, h));
    toneSlider.setBounds (scaledRect (layoutRatios, "tone_dial", w, h));
    const auto trimBounds = scaledRect (layoutRatios, "trim_dial", w, h);
    outputSlider.setBounds (trimBounds);
    // Readout sits directly below the trim knob, wider than the knob so the
    // "-30.0 dB" string fits; offsets scale with the window.
    trimReadout.setBounds (trimBounds.getX() - juce::roundToInt (24.0f * scale),
                           trimBounds.getBottom() + juce::roundToInt (2.0f * scale),
                           trimBounds.getWidth() + juce::roundToInt (48.0f * scale), juce::roundToInt (14.0f * scale));
    osSlider.setBounds (scaledRect (layoutRatios, "os_dial", w, h));
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
    // Skipped while the readout is being edited (Label::setText would kill
    // the edit): a commit overwrites any automation/preset drift made
    // mid-edit, Esc restores the live value.
    if (!trimReadout.isBeingEdited())
    {
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

    // OS factor readout follows `osfactor` (knob drag, automation, preset
    // load all arrive via the param): repaint only on change, like TRIM.
    // Reads the same raw Choice index the DSP path reads (same clamp), so
    // the label can never disagree with the audio factor.
    const int osIndex = static_cast<int> (std::round (processor.getApvts().getRawParameterValue ("osfactor")->load()));
    const juce::String osText = (osIndex <= 0) ? "1x" : (osIndex == 1) ? "2x" : "4x";
    if (osText != lastOsText)
    {
        lastOsText = osText;
        repaint();
    }
}
