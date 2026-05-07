#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Constants
//==============================================================================
constexpr int PLUGIN_WIDTH = 400;
constexpr int PLUGIN_HEIGHT = 320;

//==============================================================================
// Construction / Destruction
//==============================================================================
VCSaturatorEditor::VCSaturatorEditor(VCSaturatorProcessor& p)
    : AudioProcessorEditor(&p)
    , processor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);
}

VCSaturatorEditor::~VCSaturatorEditor()
{
}

//==============================================================================
// Paint
//==============================================================================
void VCSaturatorEditor::paint(Graphics& g)
{
    // Background
    g.fillAll(Colour(0xFF1E2530));

    // Title
    g.setColour(Colours::white);
    g.setFont(Font(20.0f, Font::bold));
    g.drawFittedText("VC-Saturator", 15, 15, getWidth() - 30, 30,
                     Justification::left, 1);

    // Subtitle
    g.setColour(Colours::grey);
    g.setFont(Font(12.0f));
    g.drawFittedText("Saturation / Distortion", 15, 45, getWidth() - 30, 20,
                     Justification::left, 1);

    // Placeholder text
    g.setColour(Colours::darkgrey);
    g.setFont(Font(14.0f));
    g.drawFittedText("Add your UI controls here",
                     15, 100, getWidth() - 30, getHeight() - 100,
                     Justification::centred, 1);
}

//==============================================================================
// Resize
//==============================================================================
void VCSaturatorEditor::resized()
{
    // Placeholder - add knob positions here
}
