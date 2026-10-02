#pragma once

#include <map>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

class AbaloneW5AudioProcessor;

// Tone bypass mapping (hardware-true, preset-safe):
// - The APVTS `tone` param (Choice Bypass/1-6, default Tone 3) is UNCHANGED;
//   presets/automation store denormalised param values, so recall of the
//   existing four ids is unaffected.
// - NEW additive Bool `toneIn` (default true) is driven by the red TONE
//   button; the chain receives `toneIn ? tone : 0`. Old states lack the
//   child, so APVTS falls back to the default: engaged.
// - The TONE KNOB is a manual (unattached) slider covering values 1-6 only.
//   Turning it while bypassed writes `tone` but never engages and never
//   changes the sound (the chain still receives 0) — like the hardware
//   knob sitting where you left it. The knob always reflects the `tone`
//   param, following automation/presets even while bypassed.
// - timerCallback re-syncs the knob (when not dragged) from the `tone`
//   param ONLY; the TONE button is a `toneIn` attachment (never synced
//   from the tone value).
// - NEW additive Bool `active` (default true, red ACTIVE button) is the
//   power switch: ACTIVE-to-THRU is a TRUE bypass (see processBlock) — zero
//   DSP, chain states frozen, buffer untouched; a brief relay-style settle
//   transient is possible on re-engage. Old states load as active. SPEAKER
//   is hardware-only: it renders as a permanent OFF image and is
//   non-interactive. ACTIVE off veils the panel (DimOverlay); the POWER
//   LED is a mains lamp — always lit while the plugin is open, painted
//   UNDER the veil with everything else so it dims naturally while
//   bypassed; the SIGNAL LED follows the input peak while bypassed.
// - Photo knob bodies (knob_*_no_pointer.png) are NEVER rotated: baked
//   off-axis highlights + edge dial-numeral fragments would swing. Bodies
//   are drawn static and circular-clipped; value is shown by the extracted
//   photo pointer (pointer_*.png: tight offline crop of the baked pointer
//   from knob_*.png, straightened to 12 o'clock, pivot = rim-fit axle)
//   rotated about the measured pivot — true to hardware and immune to skew.

// Rotary look-and-feel: static photo knob body + extracted photo pointer.
// The slider bounds ARE the knob frame; `needleStartDeg`/`needleSweepDeg`
// are clockwise-from-12, fitted against tick RAYS (see
// component_positions.csv). `pivotX/Y` is the rotation axle as a fraction
// of the slider bounds (rim-circle fit per knob art, not the frame center).
struct PhotoDialLookAndFeel : public juce::LookAndFeel_V4
{
    juce::Image bodyImage;
    juce::Image pointerImage;
    float pivotX = 0.5f;
    float pivotY = 0.5f;
    float needleStartDeg = 225.0f;
    float needleSweepDeg = 270.0f;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float /*rotaryStartAngle*/, float /*rotaryEndAngle*/, juce::Slider&) override
    {
        const float side = static_cast<float> (juce::jmin (width, height));
        const float fx = static_cast<float> (x);
        const float fy = static_cast<float> (y);

        if (bodyImage.isValid())
        {
            // Circular clip keeps the photo frame's square corners (and any
            // surround plate/numeral fragments) off the faceplate. The knob
            // bevel sits at ~102/224 of the frame half-side; the clip at
            // 108/224 keeps the dark outline ring and drops the surround.
            const float cx = fx + static_cast<float> (width) * 0.5f;
            const float cy = fy + static_cast<float> (height) * 0.5f;
            juce::Path clip;
            clip.addEllipse (cx - side * 0.4821f, cy - side * 0.4821f, side * 0.9642f, side * 0.9642f);
            g.saveState();
            g.reduceClipRegion (clip);
            g.drawImage (bodyImage,
                         juce::Rectangle<float> (fx, fy, static_cast<float> (width), static_cast<float> (height)));
            g.restoreState();
        }

        if (pointerImage.isValid())
        {
            // Extracted photo pointer, rotated about the measured axle. The
            // art points at 12 o'clock at rotation 0, so the rotation angle
            // IS the needle angle (clockwise-from-12, y-down screen space).
            const float angle = (needleStartDeg + sliderPos * needleSweepDeg) * juce::MathConstants<float>::pi / 180.0f;
            g.saveState();
            g.addTransform (juce::AffineTransform::rotation (angle, fx + pivotX * static_cast<float> (width),
                                                             fy + pivotY * static_cast<float> (height)));
            g.drawImage (pointerImage,
                         juce::Rectangle<float> (fx, fy, static_cast<float> (width), static_cast<float> (height)));
            g.restoreState();
        }
    }
};

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

// Lights-off overlay: ACTIVE off darkens the whole panel with a translucent
// fill. Painted LAST so it veils every control including the POWER LED
// (the mains lamp dims naturally with the panel, like hardware). Ordered
// above every other control; non-interactive so drags pass through;
// visibility flips instantly on re-engage from the existing 30Hz timer (no
// new threads, no fade animation). POWER's image is set once and never
// driven dark.
struct DimOverlay : public juce::Component
{
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0x99000000)); }
};

class AbaloneW5AudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AbaloneW5AudioProcessorEditor (AbaloneW5AudioProcessor&);
    ~AbaloneW5AudioProcessorEditor () override;

    void paint (juce::Graphics&) override;
    void resized () override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback () override;

    // ABALONE wordmark hit rect (paint-space): the centered header text
    // plus padding. The header is a click target toggling the additive
    // `oversample` Bool (2x on the ColorStage only, default off); cursor
    // turns to a pointing hand over it. Engaged paints lit-red
    // (button_on.png echo: red gradient + glow, never flat); the ACTIVE-off
    // dim veil covers it like every other control except POWER.
    juce::Rectangle<float> headerBounds () const;

    // -2dBFS signal-present threshold (spec: LED is signal-present, not clip).
    static constexpr float kLedThreshold = 0.79432823f; // 10^(-2/20).

    // Editor size locks to the base-texture aspect (2136x867 faceplate-only);
    // the faceplate is drawn 1:1 with no stretching.
    static constexpr int kEditorWidth = 748;
    static constexpr int kEditorHeight = 304;

    AbaloneW5AudioProcessor& processor;

    juce::Image faceImage;

    PhotoDialLookAndFeel boostDialLookAndFeel;
    PhotoDialLookAndFeel toneDialLookAndFeel;
    PhotoDialLookAndFeel trimDialLookAndFeel;

    juce::Slider boostSlider;
    juce::Slider toneSlider;   // manual: values 1-6, never engages (see note above).
    juce::Slider outputSlider; // attached; cut-only -30..0dB mini-knob on the
                               // black oval right of the THRU jack (see CSV trim_dial).
    juce::Label trimReadout;   // in-code dB readout below the trim knob (pale
                               // on the black oval). Single-click editable: type a
                               // number, Enter commits (clamped -30..0), Esc cancels.
                               // Follows the param while idle (see timerCallback).
    juce::ToggleButton highcutButton;
    juce::ToggleButton toneEngageButton; // attached to `toneIn`.
    juce::ToggleButton activeButton;     // attached to `active` (power switch).
    juce::ImageComponent speakerImage;   // permanent OFF, non-interactive (hardware-only tap).
    juce::ImageComponent signalLedImage;
    juce::ImageComponent powerLedImage;
    DimOverlay dimOverlay; // lights-off veil, visible only while ACTIVE is off.

    PngToggleLookAndFeel toggleLookAndFeel;
    juce::Image ledOnImage;
    juce::Image ledOffImage;

    // Trajan-class header face: Cinzel Black 900 (OFL, embedded as
    // BinaryData) for the in-code ABALONE wordmark + TRIM dB readout.
    // Cinzel Black is the user's final call (fix round 4): legally-available
    // OFL, supersedes the Eurostile-class round. Stroke-matched against the
    // hardware AVALON badge in docs/refs/u5_front.png at matched cap-height
    // (AVALON cap 50px texture, stems 14-15px; Cinzel Black at the same cap
    // renders stems 15-16px — no faux-bold anywhere). Single static 900
    // weight. Single typeface built once at construction; system-font
    // fallback if the embed ever fails to parse.
    juce::Font headerFont;
    juce::Font readoutFont;

    // Layout rects as texture ratios, parsed from
    // ui/component_positions.csv (embedded as BinaryData) at construction.
    std::map<juce::String, juce::Rectangle<float>> layoutRatios;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> boostAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> highcutAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toneInAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> activeAttachment;

    bool ledOn = false;
    bool powerOn = true;
    bool dimVisible = false;
    bool lastOsEngaged = false; // mirrors the `oversample` param (header red state).
    juce::String lastTrimText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbaloneW5AudioProcessorEditor)
};
