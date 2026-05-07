#include "PluginEditor.h"

using namespace juce;

constexpr int PLUGIN_WIDTH = 500;
constexpr int PLUGIN_HEIGHT = 400;

VCHarmonizerEditor::VCHarmonizerEditor(VCHarmonizerProcessor& p)
    : AudioProcessorEditor(&p)
    , processor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);
}

VCHarmonizerEditor::~VCHarmonizerEditor()
{
}

void VCHarmonizerEditor::paint(Graphics& g)
{
    g.fillAll(Colour(0xFF1E2530));

    // Title
    g.setColour(Colours::white);
    g.setFont(Font(20.0f, Font::bold));
    g.drawFittedText("VC-Harmonizer", 15, 15, getWidth() - 30, 30,
                     Justification::left, 1);

    // Subtitle
    g.setColour(Colours::grey);
    g.setFont(Font(12.0f));
    g.drawFittedText("Intelligent Harmony Generator", 15, 45, getWidth() - 30, 20,
                     Justification::left, 1);

    // Placeholder
    g.setColour(Colours::darkgrey);
    g.setFont(Font(14.0f));
    g.drawFittedText("Harmony Voice Controls\n(Add UI controls here)",
                     15, 100, getWidth() - 30, getHeight() - 100,
                     Justification::centred, 2);
}

void VCHarmonizerEditor::resized()
{
}
