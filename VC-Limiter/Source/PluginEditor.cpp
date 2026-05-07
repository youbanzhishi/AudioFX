#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Constants
//==============================================================================
constexpr int PLUGIN_WIDTH = 400;
constexpr int PLUGIN_HEIGHT = 380;

//==============================================================================
// Construction / Destruction
//==============================================================================
VC_LimiterEditor::VC_LimiterEditor(VC_LimiterProcessor& p)
    : AudioProcessorEditor(&p)
    , processor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);
}

VC_LimiterEditor::~VC_LimiterEditor()
{
}

//==============================================================================
// Paint
//==============================================================================
void VC_LimiterEditor::paint(Graphics& g)
{
    // Background
    g.fillAll(Colour(0xFF1E2530));

    // Title
    g.setColour(Colours::white);
    g.setFont(Font(22.0f, Font::bold));
    g.drawFittedText("VC-Limiter", 15, 15, getWidth() - 30, 35,
                     Justification::left, 1);

    // Subtitle
    g.setColour(Colours::grey);
    g.setFont(Font(12.0f));
    g.drawFittedText("Peak Limiter with Ceiling Control", 15, 50, getWidth() - 30, 20,
                     Justification::left, 1);

    // Decorative line
    g.setColour(Colour(0xFF3A4555));
    g.drawLine(15, 75, getWidth() - 15, 75, 1.0f);
}

//==============================================================================
// Resize
//==============================================================================
void VC_LimiterEditor::resized()
{
    // Reserved for future UI controls
}
