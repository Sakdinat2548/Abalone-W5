// Abalone W5 - U5-inspired clean bass DI.
// Copyright (C) 2026 Sakdinat2548.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>

#include <BinaryData.h>

namespace
{

juce::Image imageFromBinary (const void* data, int size) { return juce::ImageCache::getFromMemory (data, size); }

// Parses the embedded ui/new_ui/positions.csv into name -> ratio rect.
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

// Float twin of scaledRect: the TRUE slot rect with no int rounding. Slot-art
// controls (FloatArtButton/Led) draw their 1:1 PNGs at this rect (minus the
// component origin), so art registers sub-pixel-exact on the stretched plate.
juce::Rectangle<float> scaledRectF (const std::map<juce::String, juce::Rectangle<float>>& layout,
                                    const juce::String& name, int w, int h)
{
    const auto it = layout.find (name);
    jassert (it != layout.end());
    if (it == layout.end())
        return {};
    const auto& r = it->second;
    const float fw = static_cast<float> (w), fh = static_cast<float> (h);
    return juce::Rectangle<float> ((r.getX() - r.getWidth() * 0.5f) * fw, (r.getY() - r.getHeight() * 0.5f) * fh,
                                   r.getWidth() * fw, r.getHeight() * fh);
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
    // Bold system sans at plate-caption presence: baked captions are bold
    // sans ~7px on screen; UI readouts render one step larger for legibility
    // in the same family and ink (0xff1b1b1c), like INPUT/THRU read at a
    // glance. Scale-aware (rebuilt in resized()).
    // Readout cap-height matches the baked plate captions (~18px at
    // 1969px texture width): 9.5px Bold system sans at 1x scale.
    : AudioProcessorEditor (&p), processor (p), readoutFont (juce::Font (juce::FontOptions (9.5f).withStyle ("Bold")))
{
    // PNG skins + layout CSV are decoded/parsed once here on the message
    // thread, never on audio.
    faceImage = imageFromBinary (BinaryData::V110Bakedbackground_png, BinaryData::V110Bakedbackground_pngSize);
    // v110: knob bodies are BAKED (never drawn by code) — only the needles
    // rotate, pivot at needle-art bottom-center (12 o'clock art).
    boostDialLookAndFeel.needleImage =
        imageFromBinary (BinaryData::V110big_pointer_png, BinaryData::V110big_pointer_pngSize);
    toneDialLookAndFeel.needleImage = boostDialLookAndFeel.needleImage;
    trimDialLookAndFeel.needleImage =
        imageFromBinary (BinaryData::V110small_pointer_png, BinaryData::V110small_pointer_pngSize);
    boostDialLookAndFeel.dialSidePx = 420.0f;
    boostDialLookAndFeel.needleWPx = 18.0f;
    boostDialLookAndFeel.needleHPx = 61.0f;
    boostDialLookAndFeel.rimPx = 113.0f;
    toneDialLookAndFeel.dialSidePx = 420.0f;
    toneDialLookAndFeel.needleWPx = 18.0f;
    toneDialLookAndFeel.needleHPx = 61.0f;
    toneDialLookAndFeel.rimPx = 113.0f;
    trimDialLookAndFeel.dialSidePx = 110.0f;
    trimDialLookAndFeel.needleWPx = 6.0f;
    trimDialLookAndFeel.needleHPx = 20.0f;
    trimDialLookAndFeel.rimPx = 33.0f;
    // Slot art is 1:1 with the plate (buttons 103x48, LED globes 47x48), so
    // every slot control shares the same pair; float dest rects in resized()
    // land them sub-pixel-exact with no scaling (see FloatArtButton).
    highcutButton.onImage = imageFromBinary (BinaryData::V110button_on_png, BinaryData::V110button_on_pngSize);
    highcutButton.offImage = imageFromBinary (BinaryData::V110button_off_png, BinaryData::V110button_off_pngSize);
    toneEngageButton.onImage = highcutButton.onImage;
    toneEngageButton.offImage = highcutButton.offImage;
    activeButton.onImage = highcutButton.onImage;
    activeButton.offImage = highcutButton.offImage;
    signalLedImage.onImage = imageFromBinary (BinaryData::V110led_on_png, BinaryData::V110led_on_pngSize);
    signalLedImage.offImage = imageFromBinary (BinaryData::V110led_off_png, BinaryData::V110led_off_pngSize);

    layoutRatios = parseLayoutCsv (BinaryData::positions_csv, BinaryData::positions_csvSize);

    // Needle geometry: each detent aims at its NUMERAL's middle, measured
    // as the dark-mass centroid per dial sector on the redesigned texture
    // (boost numerals sit mid-sector between tick rays: centroids 224.9 ..
    // 134.8; tone likewise 285.1 .. 74.9 — entries past top stored
    // unwrapped, see PhotoDialLookAndFeel). An earlier tick-ray fit aimed
    // ~15 degrees off everywhere; numerals are what the eye reads, so the
    // tables below carry numeral centers, verified within 0.4deg of exact
    // sector midpoints. TRIM has no printed scale at its oval spot, so it
    // keeps the conventional 7-to-5-o'clock sweep; OS keeps the shared
    // trim sweep.
    boostDialLookAndFeel.detentDeg = {224.9f, 254.9f, 285.0f, 314.5f, 345.0f, 374.0f, 404.6f, 434.6f, 464.4f, 494.8f};
    toneDialLookAndFeel.detentDeg = {285.1f, 314.9f, 345.5f, 374.3f, 404.9f, 434.9f};
    trimDialLookAndFeel.needleStartDeg = 225.0f;
    trimDialLookAndFeel.needleSweepDeg = 270.0f;

    // Rotation axles: needles are straight-up 12 o'clock art; rotation
    // angle IS the needle angle (clockwise-from-12, y-down screen space).
    // Boost/tone numeral tables below carry numeral centers; trim/OS stay
    // linear (no printed scale).

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
    // Black plate ink: the readout lives on silver below the oval (see
    // resized()), matching the baked captions' family/weight, sized up for
    // legibility. Editing chrome stays dark-box/light-text (readable on
    // silver, unchanged behavior).
    trimReadout.setColour (juce::Label::textColourId, juce::Colour (0xff1b1b1c));
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

    highcutButton.setClickingTogglesState (true);
    addAndMakeVisible (highcutButton);

    // Both red buttons are plain attachments: TONE drives `toneIn`, ACTIVE
    // drives `active`. Sync (incl. first paint) is the attachments' job;
    // the timer never touches them.
    toneEngageButton.setClickingTogglesState (true);
    addAndMakeVisible (toneEngageButton);

    activeButton.setClickingTogglesState (true);
    addAndMakeVisible (activeButton);

    signalLedImage.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (signalLedImage);

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
    // this covers the first paint).
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

    // Version stamp, bottom-left corner (small, dim — never on a control).
    // From juce_add_plugin(VERSION) / test-target definitions, both fed by
    // CMake project() VERSION — single source, bump there on release.
    g.setFont (juce::Font (juce::FontOptions (11.0f * scale)));
    g.setColour (juce::Colour (0xff8a8f96));
    g.drawText ("v" JucePlugin_VersionString, juce::roundToInt (8.0f * scale), h - juce::roundToInt (20.0f * scale),
                juce::roundToInt (120.0f * scale), juce::roundToInt (14.0f * scale), juce::Justification::left, false);

    // ABALONE masthead: the SVG asset centered in the header slot (texture
    // y=98.5 cap-center, ~50px cap to match the hardware badge; the slot box
    // is padded, drawWithin centres the art). No fallback: the embedded SVG
    // cannot realistically fail to parse, so a missing drawable paints
    // nothing rather than carrying a 34KB font for a dead path.
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

    drawOsLabels (g, scaledRect (layoutRatios, "os_dial", w, h), scale);
}

// OS factor readout, centered UNDER the OS mini-knob on the silver strip
// below the oval (black plate ink, like the trim readout — never on the
// black oval). It reads the same raw Choice index the DSP path
// reads (clamped identically), so the label can never disagree with the
// audio factor. Bold system sans at readout size, matching the baked
// caption family the way the trim readout does.
void AbaloneW5AudioProcessorEditor::drawOsLabels (juce::Graphics& g, juce::Rectangle<int> knob, float scale) const
{
    const int osIndex = static_cast<int> (std::round (processor.getApvts().getRawParameterValue ("osfactor")->load()));
    const char* text = (osIndex <= 0) ? "1x" : (osIndex == 1) ? "2x" : "4x";
    g.setFont (juce::Font (juce::FontOptions (9.5f * scale).withStyle ("Bold")));
    g.setColour (juce::Colour (0xff1b1b1c));
    const int lw = juce::roundToInt (90.0f * scale);
    const int lh = juce::roundToInt (16.0f * scale);
    const int panelH = getHeight();
    const int tx = knob.getCentreX() - lw / 2;
    const int ty = panelH - juce::roundToInt (34.0f * scale);
    g.setColour (juce::Colour (0xff1b1b1c));
    g.drawText (text, tx, ty, lw, lh, juce::Justification::centred, false);
}

void AbaloneW5AudioProcessorEditor::resized ()
{
    const int w = getWidth();
    const int h = getHeight();
    const float scale = static_cast<float> (w) / static_cast<float> (kEditorWidth);

    // Typeface follows the window scale (rebuilt here on the message thread,
    // never on audio).
    readoutFont = juce::Font (juce::FontOptions (9.5f * scale).withStyle ("Bold"));
    trimReadout.setFont (readoutFont);

    boostSlider.setBounds (scaledRect (layoutRatios, "boost_dial", w, h));
    toneSlider.setBounds (scaledRect (layoutRatios, "tone_dial", w, h));
    const auto trimBounds = scaledRect (layoutRatios, "trim_dial", w, h);
    outputSlider.setBounds (trimBounds);
    // Readout lives on the silver strip below the oval (black ink needs a
    // light ground — never on the black oval). Rect is centered on the trim
    // knob's x, tucked between the oval bottom edge and the panel edge;
    // offsets scale with the window.
    trimReadout.setBounds (trimBounds.getCentreX() - juce::roundToInt (60.0f * scale),
                           h - juce::roundToInt (34.0f * scale), juce::roundToInt (120.0f * scale),
                           juce::roundToInt (16.0f * scale));
    osSlider.setBounds (scaledRect (layoutRatios, "os_dial", w, h));
    // Slot-art controls: bounds fit the ART (not the slot) so spillover like
    // the LED glow is never clipped by the component frame; the art itself
    // draws at its float dest, 1:1 and unscaled (see FloatArtButton). Slots
    // nudge +1 texture-px right / +1 down: pixel-aligned art read a hair
    // up-left of the slots. Single constants — adjust on visual check.
    const float nudgeX = 1.0f * static_cast<float> (w) / 1969.0f;
    const float nudgeY = 1.0f * static_cast<float> (h) / 799.0f;
    const auto placeButton = [this, w, h, nudgeX, nudgeY] (FloatArtButton& b, const juce::String& name)
    {
        const auto dest = scaledRectF (layoutRatios, name, w, h).translated (nudgeX, nudgeY);
        b.setBounds (dest.getSmallestIntegerContainer().expanded (1));
        b.dest = dest - b.getPosition().toFloat();
    };
    placeButton (highcutButton, "highcut_button");
    placeButton (toneEngageButton, "tone_button");
    placeButton (activeButton, "active_button");
    {
        const float sx = static_cast<float> (w) / 1969.0f;
        const float sy = static_cast<float> (h) / 799.0f;
        const auto centre = scaledRectF (layoutRatios, "signal_led", w, h).translated (nudgeX, nudgeY).getCentre();
        // OFF 48px core draws 1:1 on the slot; ON 79px art lands its 48px
        // core (file coords (42,43), spill reaches top-left) on the same
        // point. Asset geometry only — no crop, no resize.
        const auto& off = signalLedImage.offImage;
        const auto absOff =
            juce::Rectangle<float> (centre.x - off.getWidth() * sx * 0.5f, centre.y - off.getHeight() * sy * 0.5f,
                                    off.getWidth() * sx, off.getHeight() * sy);
        const auto& on = signalLedImage.onImage;
        const auto absOn = juce::Rectangle<float> (centre.x - 42.0f * sx, centre.y - 43.0f * sy, on.getWidth() * sx,
                                                   on.getHeight() * sy);
        // Bounds contain the larger (ON) dest +1px: the glow must paint past
        // the slot without the frame clipping it into a hard corner.
        signalLedImage.setBounds (absOff.getUnion (absOn).getSmallestIntegerContainer().expanded (1));
        const auto origin = signalLedImage.getPosition().toFloat();
        signalLedImage.offDest = absOff - origin;
        signalLedImage.onDest = absOn - origin;
    }
    dimOverlay.setBounds (0, 0, w, h);
}

void AbaloneW5AudioProcessorEditor::timerCallback ()
{
    const float peak = processor.getSignalPeak();
    signalLedImage.setLit (peak >= kLedThreshold);

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
