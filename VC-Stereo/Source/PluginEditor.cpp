#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Constants
//==============================================================================
constexpr int PLUGIN_WIDTH = 500;
constexpr int PLUGIN_HEIGHT = 350;

//==============================================================================
// Construction / Destruction
//==============================================================================
VCStereoEditor::VCStereoEditor(VCStereoProcessor& p)
    : AudioProcessorEditor(&p)
    , processor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);

    // Width slider
    addAndMakeVisible(widthSlider);
    widthSlider.setSliderStyle(Slider::Rotary);
    widthSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    widthSlider.setColour(Slider::rotarySliderFillColourId, Colours::cyan);
    widthAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::width, widthSlider));

    addAndMakeVisible(widthLabel);
    widthLabel.setText("Width", dontSendNotification);
    widthLabel.setJustificationType(Justification::centred);

    // Pan slider
    addAndMakeVisible(panSlider);
    panSlider.setSliderStyle(Slider::Rotary);
    panSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    panSlider.setColour(Slider::rotarySliderFillColourId, Colours::orange);
    panAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::pan, panSlider));

    addAndMakeVisible(panLabel);
    panLabel.setText("Pan", dontSendNotification);
    panLabel.setJustificationType(Justification::centred);

    // Mono Bass button
    addAndMakeVisible(monoBassButton);
    monoBassButton.setButtonText("Mono Bass");
    monoBassAttachment.reset(new AudioProcessorValueTreeState::ButtonAttachment(
        processor.getAPVTS(), ParameterIDs::monoBass, monoBassButton));

    // Bass Freq slider
    addAndMakeVisible(bassFreqSlider);
    bassFreqSlider.setSliderStyle(Slider::Rotary);
    bassFreqSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    bassFreqSlider.setColour(Slider::rotarySliderFillColourId, Colours::yellow);
    bassFreqAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::bassFreq, bassFreqSlider));

    addAndMakeVisible(bassFreqLabel);
    bassFreqLabel.setText("Bass Freq", dontSendNotification);
    bassFreqLabel.setJustificationType(Justification::centred);
}

VCStereoEditor::~VCStereoEditor()
{
}

//==============================================================================
// Paint
//==============================================================================
void VCStereoEditor::paint(Graphics& g)
{
    // Background
    g.fillAll(Colour(0xFF1E2530));

    // Title
    g.setColour(Colours::white);
    g.setFont(Font(20.0f, Font::bold));
    g.drawFittedText("VC-Stereo", 15, 15, getWidth() - 30, 30,
                     Justification::left, 1);

    // Subtitle
    g.setColour(Colours::grey);
    g.setFont(Font(12.0f));
    g.drawFittedText("Stereo Width / MS Codec / Pan / Mono Bass", 15, 45, getWidth() - 30, 20,
                     Justification::left, 1);
}

//==============================================================================
// Resize
//==============================================================================
void VCStereoEditor::resized()
{
    int y = 80;
    int knobSize = 80;
    int spacing = 120;
    int startX = 30;

    widthSlider.setBounds(startX, y, knobSize, knobSize);
    widthLabel.setBounds(startX, y + knobSize + 2, knobSize, 18);

    panSlider.setBounds(startX + spacing, y, knobSize, knobSize);
    panLabel.setBounds(startX + spacing, y + knobSize + 2, knobSize, 18);

    bassFreqSlider.setBounds(startX + spacing * 2, y, knobSize, knobSize);
    bassFreqLabel.setBounds(startX + spacing * 2, y + knobSize + 2, knobSize, 18);

    monoBassButton.setBounds(startX + spacing * 2, y + knobSize + 30, 100, 24);
}
