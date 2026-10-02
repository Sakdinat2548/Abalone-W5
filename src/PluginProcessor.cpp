#include "PluginProcessor.h"
#include "PluginEditor.h"

AbaloneW5AudioProcessor::AbaloneW5AudioProcessor ()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

AbaloneW5AudioProcessor::~AbaloneW5AudioProcessor () = default;

juce::AudioProcessorValueTreeState::ParameterLayout AbaloneW5AudioProcessor::createParameterLayout ()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "boost", "Boost", juce::StringArray ({"1", "2", "3", "4", "5", "6", "7", "8", "9", "10"}), 2));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "tone", "Tone", juce::StringArray ({"Bypass", "Tone 1", "Tone 2", "Tone 3", "Tone 4", "Tone 5", "Tone 6"}), 3));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("highcut", "High Cut", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "output", "Output", juce::NormalisableRange<float> (-30.0f, 0.0f, 0.1f), 0.0f));
    // Additive hardware-fidelity params (both default true, so states saved
    // before they existed load as engaged/active — see setNewState default).
    // `toneIn` is driven by the red TONE button; the chain receives
    // `toneIn ? tone : 0`, so the tone knob (1-6 only) never writes bypass.
    // `active` is driven by the red ACTIVE button: ACTIVE-to-THRU is an
    // internal bypass (see processBlock). Deliberately NOT exposed via
    // getBypassParameter: host bypass must not dirty plugin state by
    // flipping a preset param; host bypass is served by processBlockBypassed.
    params.push_back (std::make_unique<juce::AudioParameterBool> ("toneIn", "Tone In", true));
    params.push_back (std::make_unique<juce::AudioParameterBool> ("active", "Active", true));
    return {params.begin(), params.end()};
}

const juce::String AbaloneW5AudioProcessor::getName () const { return JucePlugin_Name; }

bool AbaloneW5AudioProcessor::acceptsMidi () const { return false; }
bool AbaloneW5AudioProcessor::producesMidi () const { return false; }
bool AbaloneW5AudioProcessor::isMidiEffect () const { return false; }
double AbaloneW5AudioProcessor::getTailLengthSeconds () const { return 0.0; }

int AbaloneW5AudioProcessor::getNumPrograms () { return 1; }
int AbaloneW5AudioProcessor::getCurrentProgram () { return 0; }
void AbaloneW5AudioProcessor::setCurrentProgram (int) {}
const juce::String AbaloneW5AudioProcessor::getProgramName (int) { return {}; }
void AbaloneW5AudioProcessor::changeProgramName (int, const juce::String&) {}

void AbaloneW5AudioProcessor::prepareToPlay (double sampleRate, int)
{
    for (auto& chain : chains)
        chain.setSampleRate (sampleRate);
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
    const bool active = apvts.getRawParameterValue ("active")->load() > 0.5f;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const int activeChannels = juce::jmin (numChannels, static_cast<int> (chains.size()));

    pushChainParams (boostStep, tone, highcut, trimDb, activeChannels);

    // ACTIVE-to-THRU: internal bypass. The buffer already holds the input,
    // so transparency is leaving it untouched; the chains still consume the
    // input (result discarded, params above already pushed) to keep
    // DC-blocker/filter state advancing, so re-engaging clicks no more
    // than the natural signal return.
    if (!active)
    {
        advanceChains (buffer, activeChannels, numSamples);
        return;
    }

    for (int ch = 0; ch < activeChannels; ++ch)
    {
        ProcessorChain& chain = chains[static_cast<size_t> (ch)];

        float* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = chain.processSample (data[i]);
    }

    for (int ch = activeChannels; ch < numChannels; ++ch)
        buffer.clear (ch, 0, numSamples);
}

void AbaloneW5AudioProcessor::pushChainParams (int boostStep, int tone, bool highcut, float trimDb, int activeChannels)
{
    for (int ch = 0; ch < activeChannels; ++ch)
    {
        ProcessorChain& chain = chains[static_cast<size_t> (ch)];
        chain.setBoostStep (boostStep);
        chain.setTone (tone);
        chain.setHighcut (highcut);
        chain.setTrimDb (trimDb);
    }
}

void AbaloneW5AudioProcessor::advanceChains (juce::AudioBuffer<float>& buffer, int activeChannels, int numSamples)
{
    for (int ch = 0; ch < activeChannels; ++ch)
    {
        ProcessorChain& chain = chains[static_cast<size_t> (ch)];
        const float* data = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            static_cast<void> (chain.processSample (data[i]));
    }
}

void AbaloneW5AudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int activeChannels = juce::jmin (buffer.getNumChannels(), static_cast<int> (chains.size()));
    const int toneParam = static_cast<int> (apvts.getRawParameterValue ("tone")->load());
    pushChainParams (static_cast<int> (apvts.getRawParameterValue ("boost")->load()) + 1,
                     apvts.getRawParameterValue ("toneIn")->load() > 0.5f ? toneParam : 0,
                     apvts.getRawParameterValue ("highcut")->load() > 0.5f,
                     apvts.getRawParameterValue ("output")->load(), activeChannels);
    advanceChains (buffer, activeChannels, buffer.getNumSamples());
}

float AbaloneW5AudioProcessor::getSignalPeak ()
{
    float peak = 0.0f;
    for (auto& chain : chains)
    {
        const float p = chain.getLastPeak();
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
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter () { return new AbaloneW5AudioProcessor(); }
