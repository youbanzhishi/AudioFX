#include "PluginEditor.h"

VCDrumEditor::VCDrumEditor(VCDrumProcessor& p)
    : AudioProcessorEditor(&p), mProcessor(p)
{
    setSize(400, 300);
}

VCDrumEditor::~VCDrumEditor()
{
}

void VCDrumEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawFittedText("VC-Drum\n(Drum Synthesizer)", getLocalBounds(),
                     juce::Justification::centred, 2);
}

void VCDrumEditor::resized()
{
}
