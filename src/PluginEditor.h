#pragma once

#include <map>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

class AbaloneW5AudioProcessor;

// Tone bypass mapping (hardware-true, preset-safe):
// - The APVTS `tone` param (Choice Bypass/1-6, default Tone 3) is UNCHANGED;
//   presets/automation store param values, so recall is byte-identical.
// - The TONE KNOB is a manual (unattached) slider covering values 1-6 only.
//   Dragging it writes the param and re-engages the tone if bypassed.
// - The red TONE BUTTON toggles Bypass <-> last non-bypass tone: pressing it
//   while engaged parks the param at Bypass (the knob keeps its position, like
//   hardware); pressing it while bypassed restores lastToneIndex.
// - timerCallback re-syncs the button (and the knob, when engaged and not
//   being dragged) from the param, so preset recall + automation always show.
// - Boost 1-10 keeps its attached Choice mapping; output trim is attached.
// - Photo knob bodies (knob_*_no_pointer.png) are NEVER rotated: baked
//   off-axis highlights + edge dial-numeral fragments would swing. Bodies are
//   drawn static and circular-clipped; value is shown by a drawn needle only.

// Rotary look-and-feel: static photo knob body + drawn needle. The slider
// bounds ARE the knob frame; `needleStartDeg`/`needleSweepDeg` are measured
// clockwise-from-12 to match the baked dial ticks (see component_positions.csv).
struct PhotoDialLookAndFeel : public juce::LookAndFeel_V4
{
    juce::Image knobImage;
    float needleStartDeg = 225.0f;
    float needleSweepDeg = 270.0f;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float /*rotaryStartAngle*/, float /*rotaryEndAngle*/, juce::Slider&) override
    {
        const float side = static_cast<float> (juce::jmin (width, height));
        const float cx = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
        const float cy = static_cast<float> (y) + static_cast<float> (height) * 0.5f;

        if (knobImage.isValid())
        {
            // Circular clip keeps the photo frame's square corners (and any
            // surround plate/numeral fragments) off the faceplate. The knob
            // bevel sits at ~102/224 of the frame half-side; the clip at
            // 108/224 keeps the dark outline ring and drops the surround.
            juce::Path clip;
            clip.addEllipse (cx - side * 0.4821f, cy - side * 0.4821f, side * 0.9642f, side * 0.9642f);
            g.saveState();
            g.reduceClipRegion (clip);
            g.drawImage (knobImage, juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                            static_cast<float> (width), static_cast<float> (height)));
            g.restoreState();
        }

        // Drawn needle: dark keyline + light core, hardware-pointer style.
        // The photo body never rotates; only this needle moves with value.
        const float angle = (needleStartDeg + sliderPos * needleSweepDeg) * juce::MathConstants<float>::pi / 180.0f;
        const float knobR = side * (102.0f / 224.0f);
        const float len = knobR * 0.78f;
        const juce::Point<float> tip (cx + std::sin (angle) * len, cy - std::cos (angle) * len);
        const juce::Point<float> tail (cx - std::sin (angle) * len * 0.18f, cy + std::cos (angle) * len * 0.18f);
        const float w = juce::jmax (2.0f, side * 0.020f);
        g.setColour (juce::Colour (0xff232527));
        g.drawLine (juce::Line<float> (tail, tip), w + 2.0f);
        g.setColour (juce::Colour (0xfff4f6f8));
        g.drawLine (juce::Line<float> (tail, tip), w);
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

    // Editor size locks to the base-texture aspect (2136x970); the faceplate
    // is drawn 1:1 with no stretching.
    static constexpr int kEditorWidth = 748;
    static constexpr int kEditorHeight = 340;

    AbaloneW5AudioProcessor& processor;

    juce::Image faceImage;

    PhotoDialLookAndFeel boostDialLookAndFeel;
    PhotoDialLookAndFeel toneDialLookAndFeel;
    PhotoDialLookAndFeel trimDialLookAndFeel;

    juce::Slider boostSlider;
    juce::Slider toneSlider; // manual: values 1-6, re-engages on drag (see note above).
    juce::Slider outputSlider;
    juce::ToggleButton highcutButton;
    juce::ToggleButton toneEngageButton;
    juce::ImageComponent signalLedImage;
    juce::ImageComponent powerLedImage;

    PngToggleLookAndFeel toggleLookAndFeel;
    juce::Image ledOnImage;
    juce::Image ledOffImage;

    // Layout rects as texture ratios, parsed from
    // ui/component_positions.csv (embedded as BinaryData) at construction.
    std::map<juce::String, juce::Rectangle<float>> layoutRatios;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> boostAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> highcutAttachment;

    int lastToneIndex = 3; // restored when the tone-engage toggle is re-armed.
    bool ledOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbaloneW5AudioProcessorEditor)
};
