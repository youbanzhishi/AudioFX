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
VCPitchShiftEditor::VCPitchShiftEditor(VCPitchShiftProcessor& p)
    : AudioProcessorEditor(&p)
    , processor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);

    // Semitones slider
    addAndMakeVisible(semitonesSlider);
    semitonesSlider.setSliderStyle(Slider::Rotary);
    semitonesSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    semitonesSlider.setColour(Slider::rotarySliderFillColourId, Colours::cyan);
    semitonesAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::semitones, semitonesSlider));

    addAndMakeVisible(semitonesLabel);
    semitonesLabel.setText("Semitones", dontSendNotification);
    semitonesLabel.setJustificationType(Justification::centred);

    // Cents slider
    addAndMakeVisible(centsSlider);
    centsSlider.setSliderStyle(Slider::Rotary);
    centsSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    centsSlider.setColour(Slider::rotarySliderFillColourId, Colours::orange);
    centsAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::cents, centsSlider));

    addAndMakeVisible(centsLabel);
    centsLabel.setText("Cents", dontSendNotification);
    centsLabel.setJustificationType(Justification::centred);

    // Formant button
    addAndMakeVisible(formantButton);
    formantButton.setButtonText("Formant Preserve");
    formantAttachment.reset(new AudioProcessorValueTreeState::ButtonAttachment(
        processor.getAPVTS(), ParameterIDs::formant, formantButton));
}

VCPitchShiftEditor::~VCPitchShiftEditor()
{
}

//==============================================================================
// Paint
//==============================================================================
void VCPitchShiftEditor::paint(Graphics& g)
{
    // Background
    g.fillAll(Colour(0xFF1E2530));

    // Title
    g.setColour(Colours::white);
    g.setFont(Font(20.0f, Font::bold));
    g.drawFittedText("VC-PitchShift", 15, 15, getWidth() - 30, 30,
                     Justification::left, 1);

    // Subtitle
    g.setColour(Colours::grey);
    g.setFont(Font(12.0f));
    g.drawFittedText("Phase Vocoder Pitch Shifting", 15, 45, getWidth() - 30, 20,
                     Justification::left, 1);
}

//==============================================================================
// Resize
//==============================================================================
void VCPitchShiftEditor::resized()
{
    int y = 80;
    int knobSize = 80;
    int spacing = 130;
    int startX = 50;

    semitonesSlider.setBounds(startX, y, knobSize, knobSize);
    semitonesLabel.setBounds(startX, y + knobSize + 2, knobSize, 18);

    centsSlider.setBounds(startX + spacing, y, knobSize, knobSize);
    centsLabel.setBounds(startX + spacing, y + knobSize + 2, knobSize, 18);

    formantButton.setBounds(startX + 20, y + knobSize + 40, 160, 24);
}
