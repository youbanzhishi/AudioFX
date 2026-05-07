#include "PluginEditor.h"

VCSynthEditor::VCSynthEditor(VCSynthProcessor& p)
    : AudioProcessorEditor(&p), mProcessor(p)
{
    setSize(400, 300);
}

VCSynthEditor::~VCSynthEditor()
{
}

void VCSynthEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawFittedText("VC-Synth\n(Subtractive Synthesizer)", getLocalBounds(),
                     juce::Justification::centred, 2);
}

void VCSynthEditor::resized()
{
}
