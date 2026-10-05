// Abalone W5 - U5-inspired clean bass DI.
// Copyright (C) 2026 Sakdinat2548.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <map>
#include <memory>
#include <vector>

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
// - Knob bodies (knob_*_no_pointer_redesigned.png) are NEVER rotated:
//   baked highlights would swing. Bodies are drawn static and
//   circular-clipped; value is shown by the extracted pointer
//   (pointer_*_redesigned.png, straightened to 12 o'clock about the art
//   axle) rotated about the measured pivot — true to hardware.

// Rotary look-and-feel: static photo knob body + extracted photo pointer.
// The slider bounds ARE the knob frame; `pivotX/Y` is the rotation axle as a
// fraction of the slider bounds (axle fit per knob art, not the frame
// center). Two needle modes: legacy linear (`needleStartDeg` +
// sliderPos * `needleSweepDeg`, clockwise-from-12) when `detentDeg` is empty,
// or an exact per-detent table (one clockwise-from-12 entry per integer
// slider value; entries past a 0-degree crossing are stored unwrapped, e.g.
// 389.6 for 29.6, so interpolation never swings backwards; fractional
// positions interpolate between entries). The redesigned UUV dial rings
// print their detent ticks on a 30-degree clock grid with a decorative top
// tick, while the NUMERALS sit mid-sector between tick rays — so no linear
// 10-/6-position needle can land on numerals (residuals up
// to 10 degrees); boost/tone carry per-detent numeral-center tables
// (see ui/new_ui/positions.csv), trim/OS stay linear (no printed scale).
struct PhotoDialLookAndFeel : public juce::LookAndFeel_V4
{
    juce::Image bodyImage;
    juce::Image pointerImage;
    float pivotX = 0.5f;
    float pivotY = 0.5f;
    float needleStartDeg = 225.0f;
    float needleSweepDeg = 270.0f;
    std::vector<float> detentDeg;

    float needleAngleFor (float sliderPos) const
    {
        if (detentDeg.size() >= 2)
        {
            const float last = static_cast<float> (detentDeg.size() - 1);
            const float p = juce::jlimit (0.0f, last, sliderPos * last);
            const size_t i = static_cast<size_t> (p);
            const size_t j = juce::jmin (i + 1, detentDeg.size() - 1);
            const float a = detentDeg[i];
            float b = detentDeg[j];
            if (b < a)
                b += 360.0f;
            return a + (b - a) * (p - static_cast<float> (i));
        }
        return needleStartDeg + sliderPos * needleSweepDeg;
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float /*rotaryStartAngle*/, float /*rotaryEndAngle*/, juce::Slider&) override
    {
        const float side = static_cast<float> (juce::jmin (width, height));
        const float fx = static_cast<float> (x);
        const float fy = static_cast<float> (y);

        if (bodyImage.isValid())
        {
            // Knob well: dark recess ring + 1px top-light arc under the
            // knob PNG, so it sits IN the panel instead of on it.
            const float cx = fx + static_cast<float> (width) * 0.5f;
            const float cy = fy + static_cast<float> (height) * 0.5f;
            const float wr = side * 0.52f;
            g.setColour (juce::Colour (0x99000000));
            g.fillEllipse (cx - wr, cy - wr, wr * 2.0f, wr * 2.0f);
            juce::Path hiArc;
            hiArc.addArc (cx - wr, cy - wr, wr * 2.0f, wr * 2.0f, juce::MathConstants<float>::pi * 1.15f,
                          juce::MathConstants<float>::pi * 1.85f, true);
            g.setColour (juce::Colour (0x59ffffff));
            g.strokePath (hiArc, juce::PathStrokeType (juce::jmax (1.0f, side * 0.006f)));
            // Circular clip keeps the photo frame's square corners (and any
            // surround plate/numeral fragments) off the faceplate. The knob
            // bevel sits at ~102/224 of the frame half-side; the clip at
            // 108/224 keeps the dark outline ring and drops the surround.
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
            const float angle = needleAngleFor (sliderPos) * juce::MathConstants<float>::pi / 180.0f;
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

private:
    void timerCallback () override;

    // OS factor label, painted in-code centered UNDER the OS mini-knob:
    // ONE live readout (current `osfactor` value only).
    void drawOsLabels (juce::Graphics& g, juce::Rectangle<int> knobBounds, float scale) const;

    // -2dBFS signal-present threshold (spec: LED is signal-present, not clip).
    static constexpr float kLedThreshold = 0.79432823f; // 10^(-2/20).

    // Editor is aspect-locked to the base texture (1969x799 faceplate)
    // and corner-drag resizable from 1x to 2x; every live element lays out
    // from CSV texture ratios in resized(), so the faceplate scales clean.
    static constexpr int kEditorWidth = 748;
    static constexpr int kEditorHeight = 304;
    static constexpr int kEditorMaxWidth = 1496;
    static constexpr int kEditorMaxHeight = 608;

    AbaloneW5AudioProcessor& processor;

    juce::Image faceImage;

    PhotoDialLookAndFeel boostDialLookAndFeel;
    PhotoDialLookAndFeel toneDialLookAndFeel;
    PhotoDialLookAndFeel trimDialLookAndFeel;

    juce::Slider boostSlider;
    juce::Slider toneSlider;   // manual: values 1-6, never engages (see note above).
    juce::Slider outputSlider; // attached; cut-only -30..0dB mini-knob on the
                               // black oval right of the THRU jack (see CSV trim_dial).
    juce::Slider osSlider;     // attached to the `osfactor` Choice (1x/2x/4x);
                               // 3-position mini-knob in the trim art family,
                               // between the SIGNAL LED and the UUV oval
                               // (see CSV os_dial). Knob drag + host automation
                               // both drive it via the attachment.
    juce::Label trimReadout;   // in-code dB readout on the silver strip below
                               // the oval (black plate ink). Single-click editable: type a
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

    // Masthead: the user-supplied ui/abalone.svg (BinaryData drawable),
    // drawn centered in the header slot; the in-code engraved Cinzel
    // fallback paints only if the SVG ever fails to parse. The masthead is
    // static black in every OS state — the Task-18 click-toggle + red
    // engaged state is gone (replaced by the `osfactor` mini-knob).
    std::unique_ptr<juce::Drawable> mastheadDrawable;
    juce::Font readoutFont;

    // Layout rects as texture ratios, parsed from
    // ui/new_ui/positions.csv (embedded as BinaryData) at construction.
    std::map<juce::String, juce::Rectangle<float>> layoutRatios;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> boostAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> osAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> highcutAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toneInAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> activeAttachment;

    bool ledOn = false;
    bool dimVisible = false;
    juce::String lastTrimText;
    juce::String lastOsText; // cached OS factor readout (repaint only on change).

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbaloneW5AudioProcessorEditor)
};
