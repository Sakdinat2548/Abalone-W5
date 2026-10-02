#include "PluginProcessor.h"
#include "PluginEditor.h"

AbaloneU55AudioProcessor::AbaloneU55AudioProcessor ()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

AbaloneU55AudioProcessor::~AbaloneU55AudioProcessor () = default;

const juce::String AbaloneU55AudioProcessor::getName () const { return JucePlugin_Name; }

bool AbaloneU55AudioProcessor::acceptsMidi () const { return false; }
bool AbaloneU55AudioProcessor::producesMidi () const { return false; }
bool AbaloneU55AudioProcessor::isMidiEffect () const { return false; }
double AbaloneU55AudioProcessor::getTailLengthSeconds () const { return 0.0; }

int AbaloneU55AudioProcessor::getNumPrograms () { return 1; }
int AbaloneU55AudioProcessor::getCurrentProgram () { return 0; }
void AbaloneU55AudioProcessor::setCurrentProgram (int) {}
const juce::String AbaloneU55AudioProcessor::getProgramName (int) { return {}; }
void AbaloneU55AudioProcessor::changeProgramName (int, const juce::String&) {}

void AbaloneU55AudioProcessor::prepareToPlay (double, int) {}
void AbaloneU55AudioProcessor::releaseResources () {}

bool AbaloneU55AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (!juce::AudioProcessor::isBusesLayoutSupported (layouts))
        return false;

    return true;
}

void AbaloneU55AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (buffer);
    // Bypass-flat pass-through: no DSP yet (Task 7 wires the chain).
}

juce::AudioProcessorEditor* AbaloneU55AudioProcessor::createEditor ()
{
    return new AbaloneU55AudioProcessorEditor (*this);
}

bool AbaloneU55AudioProcessor::hasEditor () const { return true; }

void AbaloneU55AudioProcessor::getStateInformation (juce::MemoryBlock&) {}

void AbaloneU55AudioProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter () { return new AbaloneU55AudioProcessor(); }
