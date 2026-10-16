// Abalone W5 - U5-inspired clean DI.
// Copyright (C) 2026 Sakdinat2548.
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <array>

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/ProcessorChain.h"

class AbaloneW5AudioProcessor : public juce::AudioProcessor,
                                private juce::AudioProcessorValueTreeState::Listener,
                                private juce::AsyncUpdater
{
public:
    AbaloneW5AudioProcessor ();
    ~AbaloneW5AudioProcessor () override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources () override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Host bypass: TRUE passthrough — the input delayed by the reported
    // latency (0 at 1x, i.e. untouched; 40/60 under OS, matching the engaged
    // FIR path), exactly like ACTIVE off. The input peak is still
    // tracked so the SIGNAL LED follows the input while bypassed.
    // Mono-in exception: with a single main-bus input and >= 2 buffer
    // channels, ch0 is copied to the extra channels (same dual-mono rule as
    // processBlock) since they hold no input data. Channel layout is read
    // from the main-bus layout, never buffer sniffing.
    // JUCE 8.0.15 has no AudioProcessor::isBypassed(); the host calls this
    // INSTEAD of processBlock, which is the equivalent hook.
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor () override;
    bool hasEditor () const override;

    const juce::String getName () const override;

    bool acceptsMidi () const override;
    bool producesMidi () const override;
    bool isMidiEffect () const override;
    double getTailLengthSeconds () const override;

    int getNumPrograms () override;
    int getCurrentProgram () override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // VST3 bypass parameter (user-ruled DAW awareness): the red ACTIVE button
    // drives the host bypass button via the one-way mirror (see
    // parameterChanged); host-bypass presses silence the DSP without
    // echoing into ACTIVE (auval-fidelity rule).
    juce::AudioProcessorParameter* getBypassParameter () const override { return bypassParam_; }

    juce::AudioProcessorValueTreeState& getApvts () { return apvts; }

    // Max LED peak since the last call (resets on read). Normal operation:
    // the pre-trim (post-HighCut, pre-trim-gain) chain peak — the hardware
    // has no trim so its LED can't see one. Bypassed (ACTIVE off or host
    // bypass): dark by user ruling — the LED follows the effect, not the
    // thru (the lights-off veil already dims the panel). The editor LED
    // timer polls this on the message thread; the
    // atomics are lock-free.
    float getSignalPeak ();

    // Test taps for the bypass/OS regression gates (see ProcessorIOTest):
    // dry-tap blend state after a block. Post-B2-fix a bypassed landing
    // with a changed live tap arms the blend (old tap stays live) instead
    // of adopting the new tap under audible audio.
    bool isDryBlendingForTest () const { return dryBlending_; }
    int dryLatencyForTest () const { return dryLatency_; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout ();

    // ACTIVE->bypass one-way mirror (diverge-only write so the pair can never
    // chase itself; the reverse echo is gone — auval writes every parameter
    // independently and an eager echo clobbers bulk sets). Runs on EITHER
    // thread: host automation arrives on the audio thread (verified in JUCE
    // 8.0.15: VST3 process() applies input parameter changes there), UI and
    // headless on the message thread (or none). Suppressed
    // while setStateInformation bulk-loads (loadingState_): mid-load mirrors
    // fight the incoming values — alignment happens once, explicitly, after
    // the load instead (missing-`bypass`-child blobs only; current-format
    // blobs replay bit-exact).
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    bool loadingState_ = false;

    // Deferred mirror: audio-thread arrivals park the latest value and run
    // on the message thread (gesture-bracketed host writes must not run on
    // audio — host locks mean dropouts). An explicit bypass write always
    // cancels a pending mirror, so auval bulk order (active-then-bypass,
    // bypass is laid out last) replays verbatim exactly as before.
    void handleAsyncUpdate () override;
    void mirrorActive (float activeValue);
    std::atomic<float> mirrorValue_{0.0f};
    std::atomic<bool> mirrorPending_{false};

    // Tracks max |input| over the kept channels into bypassPeak_ (read side
    // of the chains is never touched: bypass runs zero DSP, states frozen).
    void trackBypassPeak (const juce::AudioBuffer<float>& buffer, int activeChannels, int numSamples);

    void pushChainParams (int boostStep, int tone, bool highcut, float trimDb, int osFactor, int activeChannels);

    juce::AudioProcessorValueTreeState apvts;
    // Non-owning: APVTS owns the `bypass` param (created in the layout).
    juce::RangedAudioParameter* bypassParam_ = nullptr;
    std::array<ProcessorChain, 2> chains;
    // Last latency reported via setLatencySamples (0 at 1x, the chain's
    // exact hot-path FIR delay at 2x/4x). Updated only on change: the
    // factor switch flips it in processBlock, the rate path in prepareToPlay.
    int lastReportedLatency_ = 0;
    // Input-peak accumulator for the bypassed SIGNAL LED (reset-on-read).
    // Written on the audio thread, drained on the message thread.
    mutable std::atomic<float> bypassPeak_{0.0f};
    // Last rate from OUR prepareToPlay override (hosts also feed JUCE's base
    // rate via setPlayConfigDetails, but headless tests only call prepare —
    // so DSP math (bypass fade length) reads this, never getSampleRate()).
    double preparedSampleRate_ = 48000.0;
    // ACTIVE crossfade state: bypassMix_ is the dry weight (1 = full dry
    // passthrough, 0 = fully engaged). Steady states take the existing fast
    // paths untouched; only mid-transition blocks run the 5ms equal-power
    // fade below, so idle CPU is unchanged. firstAudioBlock_ snaps the mix
    // to the loaded state (no fade-in on plugin/preset load — fades only
    // cover live toggles, which is the click being fixed).
    float bypassMix_ = 0.0f;
    bool firstAudioBlock_ = true;
    // Latch set by processBlockBypassed, consumed by processBlock.
    // Default false = normal processBlock path. Atomic: audio-to-message
    // handoff — also read by getSignalPeak on the message-thread LED timer.
    std::atomic<bool> wasBypassedCall_{false};

    // Dry-path alignment delay: the engaged path delays by the hot FIR group
    // delay (0/40/60 at 1x/2x/4x, reported to the host), so the dry path
    // carries the same delay — otherwise toggling ACTIVE shifts timing and
    // PDC holds only by luck, and the crossfade combs. Per-channel circular
    // lines sized to the max (4x) latency; at 1x the tap is 0 and the lines
    // pass through untouched (bit-exact fast path preserved). Fed in every
    // path (engaged included) so the history is always live on toggle.
    // Tap switches ride the bypass fade: when the wet path's delay changes
    // mid-fade (OS flip landing in the same block as the toggle), the dry
    // side blends tap(dryLatency_) -> tap(live) equal-power over the fade —
    // taps are live histories and the weights move continuously, so even
    // mid-fade retoggles stay smooth. dryLatency_ only ever adopts a new tap
    // where dry is silent (engaged steady path, or fade landing), never
    // mid-fade.
    static constexpr int kMaxDryLatency = 60;
    std::array<std::array<float, 60>, 2> dryDelay_ = {};
    std::array<int, 2> dryPos_ = {0, 0};
    int dryLatency_ = 0;
    int dryFrom_ = 0;
    int dryBlendPos_ = 0;
    int dryBlendLen_ = 240; // 5 ms @48k; recomputed per rate in prepareToPlay.
    bool dryBlending_ = false;

    float peekDry (int ch, int latency) const
    {
        // Internal invariant: taps only ever come from our own
        // getLatencySamples() (0/40/60), so this can never fire unless a
        // future factor outgrows the lines — fail loud in debug, not silent
        // mistiming in release.
        jassert (latency >= 0 && latency <= kMaxDryLatency);
        const int p = dryPos_[static_cast<size_t> (ch)] - latency;
        const int n = kMaxDryLatency;
        return dryDelay_[static_cast<size_t> (ch)][static_cast<size_t> ((p % n + n) % n)];
    }

    void pushDry (int ch, float x)
    {
        auto& line = dryDelay_[static_cast<size_t> (ch)];
        line[static_cast<size_t> (dryPos_[static_cast<size_t> (ch)])] = x;
        dryPos_[static_cast<size_t> (ch)] = (dryPos_[static_cast<size_t> (ch)] + 1) % kMaxDryLatency;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbaloneW5AudioProcessor)
};
