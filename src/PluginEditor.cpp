#include "PluginEditor.h"
#include "PluginProcessor.h"

AbaloneW5AudioProcessorEditor::AbaloneW5AudioProcessorEditor (AbaloneW5AudioProcessor& p) : AudioProcessorEditor (&p)
{
    setSize (400, 300);
}

AbaloneW5AudioProcessorEditor::~AbaloneW5AudioProcessorEditor () = default;

void AbaloneW5AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Abalone W5", getLocalBounds(), juce::Justification::centred, 1);
}

void AbaloneW5AudioProcessorEditor::resized () {}
