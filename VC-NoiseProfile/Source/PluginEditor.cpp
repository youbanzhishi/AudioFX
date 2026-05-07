#include "PluginEditor.h"

using namespace juce;

constexpr int PLUGIN_WIDTH = 500;
constexpr int PLUGIN_HEIGHT = 400;

VCNoiseProfileEditor::VCNoiseProfileEditor(VCNoiseProfileProcessor& p)
    : AudioProcessorEditor(&p)
    , processor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);
}

VCNoiseProfileEditor::~VCNoiseProfileEditor()
{
}

void VCNoiseProfileEditor::paint(Graphics& g)
{
    g.fillAll(Colour(0xFF1E2530));

    // Title
    g.setColour(Colours::white);
    g.setFont(Font(20.0f, Font::bold));
    g.drawFittedText("VC-NoiseProfile", 15, 15, getWidth() - 30, 30,
                     Justification::left, 1);

    // Subtitle
    g.setColour(Colours::grey);
    g.setFont(Font(12.0f));
    g.drawFittedText("Noise Profile Analysis + Adaptive Spectral Subtraction + Noise Gate",
                     15, 45, getWidth() - 30, 20,
                     Justification::left, 1);

    // Placeholder
    g.setColour(Colours::darkgrey);
    g.setFont(Font(14.0f));
    g.drawFittedText("Noise Profile Controls\n(Add UI controls here)",
                     15, 100, getWidth() - 30, getHeight() - 100,
                     Justification::centred, 2);
}

void VCNoiseProfileEditor::resized()
{
}
