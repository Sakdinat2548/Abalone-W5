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
        "output", "Output", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
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
    const int boostStep = static_cast<int> (apvts.getRawParameterValue ("boost")->load()) + 1;
    const int tone = static_cast<int> (apvts.getRawParameterValue ("tone")->load());
    const bool highcut = apvts.getRawParameterValue ("highcut")->load() > 0.5f;
    const float trimDb = apvts.getRawParameterValue ("output")->load();

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const int activeChannels = juce::jmin (numChannels, static_cast<int> (chains.size()));

    for (int ch = 0; ch < activeChannels; ++ch)
    {
        ProcessorChain& chain = chains[static_cast<size_t> (ch)];
        chain.setBoostStep (boostStep);
        chain.setTone (tone);
        chain.setHighcut (highcut);
        chain.setTrimDb (trimDb);

        float* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = chain.processSample (data[i]);
    }

    for (int ch = activeChannels; ch < numChannels; ++ch)
        buffer.clear (ch, 0, numSamples);
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
