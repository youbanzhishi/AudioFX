#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Constants
//==============================================================================
constexpr int PLUGIN_WIDTH = 400;
constexpr int PLUGIN_HEIGHT = 300;

//==============================================================================
// Construction / Destruction
//==============================================================================
VC-DrumEditor::VC-DrumEditor(VC-DrumProcessor& p)
    : AudioProcessorEditor(&p)
    , processor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);

    //============================================================================
    // TODO: Create and add controls
    // Example:
    // addAndMakeVisible(gainSlider);
    // gainSlider.setSliderStyle(Slider::Rotary);
    // gainSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    // gainSlider.setColour(Slider::rotarySliderFillColourId, Colours::cyan);
    // gainAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
    //     processor.getAPVTS(), ParameterIDs::gain, gainSlider));
    //============================================================================
}

VC-DrumEditor::~VC-DrumEditor()
{
}

//==============================================================================
// Paint
//==============================================================================
void VC-DrumEditor::paint(Graphics& g)
{
    // Background
    g.fillAll(Colour(0xFF1E2530));

    // Title
    g.setColour(Colours::white);
    g.setFont(Font(20.0f, Font::bold));
    g.drawFittedText("VC-Drum", 15, 15, getWidth() - 30, 30,
                     Justification::left, 1);

    // Subtitle
    g.setColour(Colours::grey);
    g.setFont(Font(12.0f));
    g.drawFittedText("VST3 Audio Plugin", 15, 45, getWidth() - 30, 20,
                     Justification::left, 1);

    // Placeholder text
    g.setColour(Colours::darkgrey);
    g.setFont(Font(14.0f));
    g.drawFittedText("Add your UI controls here",
                     15, 100, getWidth() - 30, getHeight() - 100,
                     Justification::centred, 1);

    //============================================================================
    // TODO: Draw your plugin's UI elements
    //============================================================================
}

//==============================================================================
// Resize
//==============================================================================
void VC-DrumEditor::resized()
{
    //============================================================================
    // TODO: Layout control positions
    // Example:
    // int y = 80;
    // int knobSize = 70;
    // gainSlider.setBounds(20, y, knobSize, knobSize);
    // gainLabel.setBounds(20, y + knobSize + 2, knobSize, 18);
    //============================================================================
}
