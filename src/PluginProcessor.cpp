// Abalone W5 - U5-inspired clean DI.
// Copyright (C) 2026 Sakdinat2548.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{

// Dual-mono copy: channel 0 to every remaining buffer channel. Plain loop
// (memcpy-class, no heap, no locks) for the 1-in/N-out case; the caller owns
// the bus-layout check. ch0 itself is never touched.
void duplicateMonoToOutputs (juce::AudioBuffer<float>& buffer, int numSamples)
{
    const int numChannels = buffer.getNumChannels();
    if (numChannels < 2)
        return;

    const float* src = buffer.getReadPointer (0);
    for (int ch = 1; ch < numChannels; ++ch)
        buffer.copyFrom (ch, 0, src, numSamples);
}

} // namespace

AbaloneW5AudioProcessor::AbaloneW5AudioProcessor ()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    bypassParam_ = apvts.getParameter ("bypass");
    apvts.addParameterListener ("bypass", this);
    apvts.addParameterListener ("active", this);
}

AbaloneW5AudioProcessor::~AbaloneW5AudioProcessor ()
{
    apvts.removeParameterListener ("bypass", this);
    apvts.removeParameterListener ("active", this);
}

juce::AudioProcessorValueTreeState::ParameterLayout AbaloneW5AudioProcessor::createParameterLayout ()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "boost", "Boost", juce::StringArray ({"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"}), 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "tone", "Tone", juce::StringArray ({"Bypass", "Tone 1", "Tone 2", "Tone 3", "Tone 4", "Tone 5", "Tone 6"}), 3));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("highcut", "High Cut", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "output", "Output", juce::NormalisableRange<float> (-32.0f, 6.0f, 0.1f), -3.0f));
    // Additive hardware-fidelity params (both default true, so states saved
    // before they existed load as engaged/active — see setNewState default).
    // `toneIn` is driven by the red TONE button; the chain receives
    // `toneIn ? tone : 0`, so the tone knob (1-6 only) never writes bypass.
    // `active` is driven by the red ACTIVE button: ACTIVE-to-THRU is an
    // internal bypass (see processBlock). `bypass` (below) is the host
    // bypass parameter served by getBypassParameter, synced ONE WAY from
    // active (user-ruled: the DAW bypass button follows ACTIVE; the reverse
    // echo is gone — see parameterChanged). DSP obeys either control, so
    // preset recall is bit-identical; host bypass arrives as a bypass-param
    // change and rides the same TRUE-bypass fade. processBlockBypassed stays
    // for hosts/formats that invoke it (VST3 with our own bypass param
    // always runs processBlock instead).
    params.push_back (std::make_unique<juce::AudioParameterBool> ("toneIn", "Tone In", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("active", "Active", true));
    // Additive quality param (default 1x, so states saved before it
    // existed load as 1x — the byte-identical default path). REPLACES the
    // Task-18 Bool `oversample` (v1 unreleased: no in-the-wild presets to
    // break; the id change is recorded here). Driven by the OS mini-knob in
    // the editor (SliderAttachment: knob drag + host automation both drive
    // it). 2x/4x wrap the ColorStage only (see ProcessorChain.h: 2x is the
    // shipped 81-tap FIR, 4x cascades it 2x->2x); the exact FIR delay
    // (0/40/60 samples) is reported via setLatencySamples on switch +
    // prepare.
    params.push_back (std::make_unique<juce::AudioParameterChoice> ("osfactor", "OS Factor",
                                                                    juce::StringArray ({"1x", "2x", "4x"}), 0));
    // Host bypass parameter (user-ruled DAW awareness — see getBypassParameter
    // + parameterChanged). Appended LAST so legacy param indices (and host
    // automation) of every existing id are untouched. Default false =
    // engaged, so pre-bypass states recall exactly as before; the
    // setStateInformation converge aligns it to a stored ACTIVE-off only
    // when the blob predates the param (no `bypass` child).
    params.push_back (std::make_unique<juce::AudioParameterBool> ("bypass", "Bypass", false));
    return {params.begin(), params.end()};
}

const juce::String AbaloneW5AudioProcessor::getName () const { return JucePlugin_Name; }

bool AbaloneW5AudioProcessor::acceptsMidi () const { return false; }
bool AbaloneW5AudioProcessor::producesMidi () const { return false; }
bool AbaloneW5AudioProcessor::isMidiEffect () const { return false; }
double AbaloneW5AudioProcessor::getTailLengthSeconds () const
{
    // Measured 1.0 s (not the reviewer's 0.3 s estimate, and not from the
    // tone sections either): a hot stopped 55 Hz note rings the 5 Hz/2 Hz
    // DC-blockers + low-heavy tones above -60 dBFS for 0.6 s realistic
    // (0.96 s pathological full-scale-DC case), so 1.0 s covers both.
    return 1.0;
}

int AbaloneW5AudioProcessor::getNumPrograms () { return 1; }
int AbaloneW5AudioProcessor::getCurrentProgram () { return 0; }
void AbaloneW5AudioProcessor::setCurrentProgram (int) {}
const juce::String AbaloneW5AudioProcessor::getProgramName (int) { return {}; }
void AbaloneW5AudioProcessor::changeProgramName (int, const juce::String&) {}

void AbaloneW5AudioProcessor::prepareToPlay (double sampleRate, int)
{
    // Our own prepared rate (JUCE's base rate is host-fed via
    // setPlayConfigDetails and reads 0 headless — never use getSampleRate()
    // for DSP math; see the bypass-fade note in processBlock).
    preparedSampleRate_ = (sampleRate > 0.0) ? sampleRate : 48000.0;
    // Restored oversampled sessions must report the hot-path delay from the
    // start: push the param into the chains BEFORE reading
    // getLatencySamples, or a session saved hot reports 0 until the first
    // processBlock push flips it (DAW compensates late -> early audio runs
    // uncompensated). setOsFactor on an already-matching target is a no-op,
    // so repeated prepares never re-arm the entry blend; setSampleRate
    // preserves the target (ChainTest prepare-ordering gates pin both
    // orders, all factors, both rates).
    const int osIndex = static_cast<int> (std::round (apvts.getRawParameterValue ("osfactor")->load()));
    const int osFactor = (osIndex <= 0) ? 1 : (osIndex == 1) ? 2 : 4;
    for (auto& chain : chains)
    {
        chain.setSampleRate (sampleRate);
        chain.setOsFactor (osFactor);
    }
    // Re-report after every rate change (the 1x/2x/4x latency is
    // rate-independent, but the host still needs a fresh value on
    // re-prepare; the switch path in processBlock covers factor changes).
    lastReportedLatency_ = chains[0].getLatencySamples();
    setLatencySamples (lastReportedLatency_);
    // Dry-alignment history is meaningless across rates/prepares. The tap
    // adopts the pushed factor directly (hosts compensate from block one).
    for (auto& line : dryDelay_)
        line.fill (0.0f);
    dryPos_ = {0, 0};
    dryLatency_ = chains[0].getLatencySamples();
    dryBlending_ = false;
    dryBlendLen_ = juce::jmax (1, static_cast<int> (0.005 * preparedSampleRate_));
}

void AbaloneW5AudioProcessor::releaseResources () {}

bool AbaloneW5AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (!juce::AudioProcessor::isBusesLayoutSupported (layouts))
        return false;

    return true;
}

void AbaloneW5AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Params are copied once per block; the per-sample path takes no locks.
    // `toneIn ? tone : 0` is the hardware-true TONE button: the knob only
    // ever holds 1-6, the button decides whether it reaches the chain.
    const int boostStep = static_cast<int> (apvts.getRawParameterValue ("boost")->load()) + 1;
    const int toneParam = static_cast<int> (apvts.getRawParameterValue ("tone")->load());
    const bool toneIn = apvts.getRawParameterValue ("toneIn")->load() > 0.5f;
    const int tone = toneIn ? toneParam : 0;
    const bool highcut = apvts.getRawParameterValue ("highcut")->load() > 0.5f;
    const float trimDb = apvts.getRawParameterValue ("output")->load();
    // One-way mirror (see parameterChanged): host bypass writes only
    // `bypass` and never echoes into `active`, so bulk sets (AU state
    // restore, auval's per-param randomization) round-trip bit-exact. The
    // DSP obeys EITHER control, so host bypass still silences audio; only
    // the ACTIVE button display no longer follows host-bypass presses.
    const bool hostBypass = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    const bool active = apvts.getRawParameterValue ("active")->load() > 0.5f && !hostBypass;
    const int osIndex = static_cast<int> (std::round (apvts.getRawParameterValue ("osfactor")->load()));
    const int osFactor = (osIndex <= 0) ? 1 : (osIndex == 1) ? 2 : 4;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const int activeChannels = juce::jmin (numChannels, static_cast<int> (chains.size()));

    // Dual-mono rule: the buses admit mono-in/stereo-out (e.g. a mono ASIO
    // input feeding the stereo Standalone output). The extra buffer channels
    // then hold no input data, so per-channel processing would play garbage
    // on the right. Detect via the BUS layout (never buffer content): with a
    // single main-bus input and >= 2 buffer channels, ch0 runs through
    // chain[0] and the result is copied to every remaining channel. With
    // inputs >= outputs each channel keeps its own chain, exactly as before.
    // The copy is a plain loop: no heap, no locks on the audio path.
    const bool monoDuplicate = getMainBusNumInputChannels() == 1 && numChannels >= 2;

    // ACTIVE-to-THRU: TRUE bypass (relay-style). Zero DSP: the chains are not
    // fed at all and their states freeze. The buffer already holds the input
    // (in-place processing), so the passthrough is the input delayed by the
    // reported latency (0 at 1x — untouched; 40/60 under OS, matching the
    // engaged FIR path, so toggles never shift timing and PDC holds) — only
    // unmapped extra channels are cleared, as in the engaged path.
    // Re-engage settle: filters resume from the frozen state, so a brief
    // transient is possible on re-engage (authentic relay behavior), settling
    // within milliseconds as the DC-blocker re-converges.
    // Toggle clicks are covered below by a 5ms equal-power crossfade: steady
    // states (mix pinned at 0 or 1) take the fast paths with zero extra
    // cost; only the transition blocks mix dry/wet per sample.
    if (firstAudioBlock_)
    {
        bypassMix_ = active ? 0.0f : 1.0f;
        firstAudioBlock_ = false;
    }
    const float bypassTarget = active ? 0.0f : 1.0f;
    if (bypassMix_ == bypassTarget)
    {
        if (bypassTarget >= 1.0f)
        {
            trackBypassPeak (buffer, monoDuplicate ? 1 : activeChannels, numSamples);
            // Latency-aligned dry on the adopted tap (frozen while bypassed —
            // chains are not re-pushed here, so the tap matches the last
            // reported value by construction).
            const int nCh = monoDuplicate ? 1 : activeChannels;
            if (dryLatency_ > 0)
            {
                for (int ch = 0; ch < nCh; ++ch)
                {
                    float* data = buffer.getWritePointer (ch);
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const float x = data[i];
                        data[i] = peekDry (ch, dryLatency_);
                        pushDry (ch, x);
                    }
                }
            }
            else
            {
                // 1x: passthrough stays bit-exact, but the lines must still
                // track the input — an OS flip landing mid-bypass only
                // reaches the chains at the next re-engage fade, whose dry
                // side would otherwise replay pre-bypass history.
                for (int ch = 0; ch < nCh; ++ch)
                {
                    const float* data = buffer.getReadPointer (ch);
                    for (int i = 0; i < numSamples; ++i)
                        pushDry (ch, data[i]);
                }
            }
            if (monoDuplicate)
                duplicateMonoToOutputs (buffer, numSamples);
            else
                for (int ch = activeChannels; ch < numChannels; ++ch)
                    buffer.clear (ch, 0, numSamples);
            return;
        }
    }
    else
    {
        // Mid-transition: one shared trajectory for all channels (mix
        // advances once per sample, identical per channel, so no per-channel
        // state can drift). Chains stay fed throughout the fade (settling
        // while covered), then freeze once fully bypassed. Retoggle mid-fade
        // reverses smoothly: no restart jump, the trajectory just turns.
        pushChainParams (boostStep, tone, highcut, trimDb, osFactor, monoDuplicate ? 1 : activeChannels);
        const int nCh = monoDuplicate ? 1 : activeChannels;
        // Dry-tap blend: the wet path's delay can change inside this same
        // block (OS flip landing with the toggle). The dry side then blends
        // tap(dryLatency_) -> tap(live) equal-power across the fade instead
        // of stepping taps at the boundary (a 0->40 tap switch on live audio
        // jumps ~0.4 on a 220 Hz sine). Arms once per fade; the taps stay
        // live throughout, so the weights — not the taps — carry the motion.
        const int fadeTarget = chains[0].getLatencySamples();
        if (!dryBlending_ && fadeTarget != dryLatency_)
        {
            dryFrom_ = dryLatency_;
            dryBlendPos_ = 0;
            dryBlending_ = true;
        }
        float* chPtr[2] = {nullptr, nullptr};
        for (int ch = 0; ch < nCh; ++ch)
            chPtr[ch] = buffer.getWritePointer (ch);
        // NOTE: uses preparedSampleRate_, NOT getSampleRate(): our
        // prepareToPlay override never feeds JUCE's base rate (only hosts do
        // via setPlayConfigDetails), so getSampleRate() is unreliable here
        // (0 in headless tests -> fadeStep 1.0 -> instant jump, i.e. the bug
        // this fade exists to fix).
        const float fadeStep = 1.0f / juce::jmax (1, static_cast<int> (0.005 * preparedSampleRate_));
        constexpr float halfPi = 1.57079632679489661923f;
        for (int i = 0; i < numSamples; ++i)
        {
            if (bypassMix_ < bypassTarget)
                bypassMix_ = juce::jmin (bypassTarget, bypassMix_ + fadeStep);
            else if (bypassMix_ > bypassTarget)
                bypassMix_ = juce::jmax (bypassTarget, bypassMix_ - fadeStep);
            const float dryW = std::sin (halfPi * bypassMix_);
            const float wetW = std::cos (halfPi * bypassMix_);
            // Dry-blend clock: one tap-step per dry sample while armed.
            float tapNewW = 0.0f;
            float tapOldW = 1.0f;
            if (dryBlending_)
            {
                ++dryBlendPos_;
                float t = static_cast<float> (dryBlendPos_) / static_cast<float> (dryBlendLen_);
                if (t > 1.0f)
                    t = 1.0f;
                tapNewW = std::sin (halfPi * t);
                tapOldW = std::cos (halfPi * t);
            }
            for (int ch = 0; ch < nCh; ++ch)
            {
                const float dry = chPtr[ch][i];
                const float tapNow = (dryLatency_ > 0) ? peekDry (ch, dryLatency_) : dry;
                float dryAligned = tapNow;
                if (dryBlending_)
                {
                    const float tapFrom = (dryFrom_ > 0) ? peekDry (ch, dryFrom_) : dry;
                    const float tapTo = (fadeTarget > 0) ? peekDry (ch, fadeTarget) : dry;
                    dryAligned = tapFrom * tapOldW + tapTo * tapNewW;
                }
                const float wet = chains[static_cast<size_t> (ch)].processSample (dry);
                pushDry (ch, dry);
                chPtr[ch][i] = dryAligned * dryW + wet * wetW;
            }
        }
        // Landed: adopt the live tap (the blend already sits at weight 1, so
        // the adoption itself is a no-op numerically — the tap only ever
        // switches hands where dry is silent or fully blended).
        if (bypassMix_ == bypassTarget)
        {
            dryLatency_ = chains[0].getLatencySamples();
            dryBlending_ = false;
        }
        if (monoDuplicate)
            duplicateMonoToOutputs (buffer, numSamples);
        else
            for (int ch = activeChannels; ch < numChannels; ++ch)
                buffer.clear (ch, 0, numSamples);
        // Same DAW-compensation reporting as the engaged path below: an
        // osfactor flip landing in the same block as the bypass toggle must
        // not report a block late.
        const int transLatency = chains[0].getLatencySamples();
        if (transLatency != lastReportedLatency_)
        {
            lastReportedLatency_ = transLatency;
            setLatencySamples (transLatency);
        }
        return;
    }

    pushChainParams (boostStep, tone, highcut, trimDb, osFactor, monoDuplicate ? 1 : activeChannels);

    // DAW compensation: the hot FIR path adds the chain's exact group delay
    // (0 at 1x, 40 at 2x, 60 at 4x). Reported on change only; the factor
    // switch flips it here, the rate path in prepareToPlay. Both chains
    // share the same target, so chain[0] is the source of truth.
    const int latency = chains[0].getLatencySamples();
    if (latency != lastReportedLatency_)
    {
        lastReportedLatency_ = latency;
        setLatencySamples (latency);
    }

    if (monoDuplicate)
    {
        ProcessorChain& chain = chains[0];

        float* data = buffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = data[i];
            data[i] = chain.processSample (x);
            pushDry (0, x); // keep the alignment history live for the next toggle.
        }

        dryLatency_ = chains[0].getLatencySamples(); // dry unread here: adopt free.
        duplicateMonoToOutputs (buffer, numSamples);
        return;
    }

    for (int ch = 0; ch < activeChannels; ++ch)
    {
        ProcessorChain& chain = chains[static_cast<size_t> (ch)];

        float* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = data[i];
            data[i] = chain.processSample (x);
            pushDry (ch, x);
        }
    }
    dryLatency_ = chains[0].getLatencySamples(); // dry unread here: adopt free.

    for (int ch = activeChannels; ch < numChannels; ++ch)
        buffer.clear (ch, 0, numSamples);
}

void AbaloneW5AudioProcessor::pushChainParams (int boostStep, int tone, bool highcut, float trimDb, int osFactor,
                                               int activeChannels)
{
    for (int ch = 0; ch < activeChannels; ++ch)
    {
        ProcessorChain& chain = chains[static_cast<size_t> (ch)];
        chain.setBoostStep (boostStep);
        chain.setTone (tone);
        chain.setHighcut (highcut);
        chain.setTrimDb (trimDb);
        chain.setOsFactor (osFactor);
    }
}

void AbaloneW5AudioProcessor::trackBypassPeak (const juce::AudioBuffer<float>& buffer, int activeChannels,
                                               int numSamples)
{
    float m = 0.0f;
    for (int ch = 0; ch < activeChannels; ++ch)
    {
        const float* data = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float a = std::fabs (data[i]);
            if (a > m)
                m = a;
        }
    }
    // Benign race (monotonic max within the interval, reset-on-read on the
    // message thread): a lost update only dims one 30Hz LED poll; the atomic
    // keeps the realtime thread lock-free (no mutexes).
    const float cur = bypassPeak_.load (std::memory_order_relaxed);
    if (m > cur)
        bypassPeak_.store (m, std::memory_order_relaxed);
}

void AbaloneW5AudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Host bypass: same TRUE passthrough as ACTIVE off — zero DSP, states
    // frozen, input delayed by the reported latency (see processBlock).
    // Only the input peak is tracked for the LED.
    // Mono-in exception: the extra channels hold no input data, so ch0 is
    // copied out (same dual-mono rule as the other paths) — still no DSP.
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const bool monoDuplicate = getMainBusNumInputChannels() == 1 && numChannels >= 2;

    trackBypassPeak (buffer, monoDuplicate ? 1 : juce::jmin (numChannels, static_cast<int> (chains.size())),
                     numSamples);

    const int nCh = monoDuplicate ? 1 : juce::jmin (numChannels, static_cast<int> (chains.size()));
    if (dryLatency_ > 0)
    {
        for (int ch = 0; ch < nCh; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                const float x = data[i];
                data[i] = peekDry (ch, dryLatency_);
                pushDry (ch, x);
            }
        }
    }
    else
    {
        // 1x: untouched passthrough, but the alignment history still tracks
        // (same OS-flip-mid-bypass hole as processBlock above).
        for (int ch = 0; ch < nCh; ++ch)
        {
            const float* data = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                pushDry (ch, data[i]);
        }
    }

    if (monoDuplicate)
        duplicateMonoToOutputs (buffer, numSamples);
    else
        // Unmapped extra channels carry no input data (same rule as every
        // other path): clear, never leak stale buffer content to the host.
        for (int ch = nCh; ch < numChannels; ++ch)
            buffer.clear (ch, 0, numSamples);
}

float AbaloneW5AudioProcessor::getSignalPeak ()
{
    // Bypassed intervals contribute the input peak; engaged intervals the
    // pre-trim chain peak. The legacy post-trim tracker is drained (not
    // used) so no stale maximum survives across reads.
    float peak = bypassPeak_.exchange (0.0f, std::memory_order_relaxed);
    for (auto& chain : chains)
    {
        static_cast<void> (chain.getLastPeak());
        const float p = chain.getLastPreTrimPeak();
        if (p > peak)
            peak = p;
    }
    return peak;
}

juce::AudioProcessorEditor* AbaloneW5AudioProcessor::createEditor ()
{
    return new AbaloneW5AudioProcessorEditor (*this);
}

bool AbaloneW5AudioProcessor::hasEditor () const { return true; }

void AbaloneW5AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    std::unique_ptr<juce::XmlElement> xml (apvts.copyState().createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void AbaloneW5AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    loadingState_ = true;
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    loadingState_ = false;
    // Pre-bypass-era blobs have no `bypass` child (default engaged): align it
    // once from a stored ACTIVE-off so old bypassed sessions recall bypassed
    // in the DAW. Presence is read from the BLOB, never apvts.state:
    // replaceState re-links every adapter and re-appends missing children
    // with defaults (JUCE's updateParameterConnectionsToChildTrees), so the
    // tree always looks complete afterwards. Current-format blobs (child
    // present) are left bit-exact — even an inconsistent pair replays
    // verbatim, which is what AU state round-trip validation (auval)
    // requires of every parameter. Never touch `active` here: it is the
    // source of truth, `bypass` the follower.
    if (bypassParam_ != nullptr && xml != nullptr)
    {
        bool hasBypassChild = false;
        for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
            if (child->getStringAttribute ("id") == "bypass")
            {
                hasBypassChild = true;
                break;
            }
        if (!hasBypassChild)
        {
            const bool active = apvts.getRawParameterValue ("active")->load() > 0.5f;
            bypassParam_->setValueNotifyingHost (active ? 0.0f : 1.0f);
        }
    }
}

void AbaloneW5AudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (loadingState_)
        return;
    // One-way mirror, ACTIVE -> bypass only (diverge-only write so the pair
    // can never chase itself). Gesture-bracketed: some hosts ignore
    // unbracketed performEdit on kIsBypass, so the DAW bypass button would
    // never follow the ACTIVE button without the gesture pair.
    // The reverse echo (bypass -> active) is deliberately GONE: auval (and
    // Apple's AU per-parameter state replay) writes every parameter
    // independently, so an eager echo clobbers bulk sets — proven by CI
    // (Active 0.112305 restored as 1.0). Host bypass still silences the DSP
    // (processBlock obeys either control); only the ACTIVE button display
    // no longer follows host-bypass presses.
    if (parameterID != "active")
        return;
    const bool on = newValue > 0.5f;
    if (bypassParam_ != nullptr)
        if ((bypassParam_->getValue() > 0.5f) == on) // bypass must read !active
        {
            bypassParam_->beginChangeGesture();
            bypassParam_->setValueNotifyingHost (on ? 0.0f : 1.0f);
            bypassParam_->endChangeGesture();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter () { return new AbaloneW5AudioProcessor(); }
