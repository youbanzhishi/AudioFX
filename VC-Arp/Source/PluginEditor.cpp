#include "PluginEditor.h"

VCArpEditor::VCArpEditor(VCArpProcessor& p)
    : AudioProcessorEditor(&p), mProcessor(p)
{
    setSize(400, 300);
}

VCArpEditor::~VCArpEditor()
{
}

void VCArpEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawFittedText("VC-Arp\n(Arpeggiator)", getLocalBounds(),
                     juce::Justification::centred, 2);
}

void VCArpEditor::resized()
{
}
