#include "PluginEditor.h"
#include "PluginProcessor.h"

AbaloneU55AudioProcessorEditor::AbaloneU55AudioProcessorEditor (AbaloneU55AudioProcessor& p) : AudioProcessorEditor (&p)
{
    setSize (400, 300);
}

AbaloneU55AudioProcessorEditor::~AbaloneU55AudioProcessorEditor () = default;

void AbaloneU55AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Abalone-U55", getLocalBounds(), juce::Justification::centred, 1);
}

void AbaloneU55AudioProcessorEditor::resized () {}
