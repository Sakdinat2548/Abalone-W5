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
//   transient is possible on re-engage. Old states load as active. The
//   v110 plate bakes SPEAKER + POWER in the ON look — code owns no part for
//   them. ACTIVE off veils the panel (DimOverlay); the SIGNAL LED follows
//   the input peak while bypassed.
// - Knob bodies live in the baked v110 plate (never drawn/rotated by code):
//   value is shown by the needle PNGs (V110big/small_pointer, straight up at
//   12 o'clock) drawn at natural art size, tip at the face rim, rotating
//   about the measured axle — true to hardware.

// Rotary look-and-feel: baked knob body + natural-size needle.
// The slider bounds center IS the rotation axle (tick-arc / face fit per
// dial, see ui/v110ui/positions.csv). Two needle modes: legacy linear
// (`needleStartDeg` + sliderPos * `needleSweepDeg`, clockwise-from-12) when
// `detentDeg` is empty, or an exact per-detent table (one clockwise-from-12
// entry per integer slider value; entries past a 0-degree crossing are
// stored unwrapped, e.g. 389.6 for 29.6, so interpolation never swings
// backwards; fractional positions interpolate between entries). The v110
// dial rings print ticks/numerals on an exact 30-degree clock grid, so
// boost/tone carry 30-degree detent tables (visually verified against the
// baked numerals); trim/OS stay linear (no printed scale).
struct PhotoDialLookAndFeel : public juce::LookAndFeel_V4
{
    // v110: knob bodies live in the baked plate — code draws ONLY the
    // needle (straight-up 12 o'clock art) at NATURAL art size (never
    // stretched: needleWPx/HPx == PNG px). It sits outer-rim like a tire
    // tread — tip at rimPx from the axle, butt floating over the face —
    // rotating about the axle (dial center).
    juce::Image needleImage;
    float dialSidePx = 420.0f;
    float needleWPx = 18.0f;
    float needleHPx = 61.0f;
    float rimPx = 113.0f;
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

        if (needleImage.isValid())
        {
            // Needle only (bodies live in the baked plate), drawn UNSTRETCHED
            // at natural art size: tip at rimPx from the axle, butt floating
            // over the face (tire-tread, never spanning the radius). Art
            // points at 12 o'clock at rotation 0; rotation angle IS the
            // needle angle (clockwise-from-12, y-down screen space).
            const float k = side / dialSidePx;
            const float pw = needleWPx * k;
            const float ph = needleHPx * k;
            const float rim = rimPx * k;
            const float cx = fx + static_cast<float> (width) * 0.5f;
            const float cy = fy + static_cast<float> (height) * 0.5f;
            const float angle = needleAngleFor (sliderPos) * juce::MathConstants<float>::pi / 180.0f;
            g.saveState();
            g.addTransform (juce::AffineTransform::rotation (angle, cx, cy));
            g.drawImage (needleImage, juce::Rectangle<float> (cx - pw * 0.5f, cy - rim, pw, ph));
            g.restoreState();
        }
    }
};

// Slot-art control: draws its PNG at a float dest rect (set in resized())
// instead of integer component bounds, so 1:1-exported art registers
// exactly with the stretched baked plate — no scaling, no cover-up overlap.
// The art is slot-sized by design, so dest size == true slot size; only the
// position is sub-pixel. Plain ToggleButton otherwise (attachments stay).
struct FloatArtButton : public juce::ToggleButton
{
    juce::Image onImage, offImage;
    juce::Rectangle<float> dest;

    void paint (juce::Graphics& g) override
    {
        const juce::Image& img = getToggleState() ? onImage : offImage;
        if (img.isValid() && !dest.isEmpty())
            g.drawImage (img, dest);
    }
};

// Same float-dest idea for the SIGNAL LED (non-interactive). OFF art is
// the bare 48px core (= slot size); ON art is 79px (48px core + light
// spill, core center at (42,43) of the file). Each dest lands its CORE on
// the slot center — art is never cropped, resized, or recentered.
struct FloatArtLed : public juce::Component
{
    juce::Image onImage, offImage;
    juce::Rectangle<float> offDest, onDest;
    bool lit = false;

    void setLit (bool shouldBeOn)
    {
        if (shouldBeOn != lit)
        {
            lit = shouldBeOn;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const juce::Image& img = lit ? onImage : offImage;
        const auto& d = lit ? onDest : offDest;
        if (img.isValid() && !d.isEmpty())
            g.drawImage (img, d);
    }
};

// Lights-off overlay: ACTIVE off darkens the whole panel with a translucent
// fill. Painted LAST so it veils every control (baked lamps dim naturally
// with the panel, like hardware). Ordered above every other control;
// non-interactive so drags pass through; visibility flips instantly on
// re-engage from the existing 30Hz timer (no new threads, no fade animation).
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
    juce::Slider outputSlider; // attached; -32..+6dB mini-knob on the
                               // black oval right of the THRU jack (see CSV trim_dial).
    juce::Slider osSlider;     // attached to the `osfactor` Choice (1x/2x/4x);
                               // 3-position mini-knob in the trim art family,
                               // between the SIGNAL LED and the UUV oval
                               // (see CSV os_dial). Knob drag + host automation
                               // both drive it via the attachment.
    juce::Label trimReadout;   // in-code dB readout on the silver strip below
                               // the oval (black plate ink). Single-click editable: type a
                               // number, Enter commits (clamped -32..+6), Esc cancels.
                               // Follows the param while idle (see timerCallback).
    FloatArtButton highcutButton;
    FloatArtButton toneEngageButton; // attached to `toneIn`.
    FloatArtButton activeButton;     // attached to `active` (power switch).
    FloatArtLed signalLedImage;
    DimOverlay dimOverlay; // lights-off veil, visible only while ACTIVE is off.

    // Masthead: the user-supplied ui/abalone.svg (BinaryData drawable),
    // drawn centered in the header slot; the in-code engraved Cinzel
    // fallback paints only if the SVG ever fails to parse. The masthead is
    // static black in every OS state — the Task-18 click-toggle + red
    // engaged state is gone (replaced by the `osfactor` mini-knob).
    std::unique_ptr<juce::Drawable> mastheadDrawable;
    juce::Font readoutFont;

    // Layout rects as texture ratios, parsed from
    // ui/v110ui/positions.csv (embedded as BinaryData, v110 geometry) at construction.
    std::map<juce::String, juce::Rectangle<float>> layoutRatios;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> boostAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> osAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> highcutAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toneInAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> activeAttachment;

    bool dimVisible = false;
    juce::String lastTrimText;
    juce::String lastOsText; // cached OS factor readout (repaint only on change).

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbaloneW5AudioProcessorEditor)
};
