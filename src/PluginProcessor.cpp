#include "PluginProcessor.h"
#include "PluginEditor.h"

AbaloneW5AudioProcessor::AbaloneW5AudioProcessor ()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

AbaloneW5AudioProcessor::~AbaloneW5AudioProcessor () = default;

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

void AbaloneW5AudioProcessor::prepareToPlay (double, int) {}
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
    juce::ignoreUnused (buffer);
    // Bypass-flat pass-through: no DSP yet (Task 7 wires the chain).
}

juce::AudioProcessorEditor* AbaloneW5AudioProcessor::createEditor ()
{
    return new AbaloneW5AudioProcessorEditor (*this);
}

bool AbaloneW5AudioProcessor::hasEditor () const { return true; }

void AbaloneW5AudioProcessor::getStateInformation (juce::MemoryBlock&) {}

void AbaloneW5AudioProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter () { return new AbaloneW5AudioProcessor(); }
