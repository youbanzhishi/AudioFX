#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Constants
//==============================================================================
constexpr int PLUGIN_WIDTH = 480;
constexpr int PLUGIN_HEIGHT = 400;

//==============================================================================
// Construction / Destruction
//==============================================================================
VCReverbEditor::VCReverbEditor(VCReverbProcessor& p)
    : AudioProcessorEditor(&p)
    , processor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);
    
    // Title
    addAndMakeVisible(titleLabel);
    titleLabel.setText("VC-Reverb", dontSendNotification);
    titleLabel.setFont(Font(24.0f, Font::bold));
    titleLabel.setColour(Label::textColourId, Colours::white);
    
    // Subtitle
    addAndMakeVisible(subtitleLabel);
    subtitleLabel.setText("Schroeder Algorithmic Reverb", dontSendNotification);
    subtitleLabel.setFont(Font(12.0f));
    subtitleLabel.setColour(Label::textColourId, Colours::grey);
    
    // Room Size
    addAndMakeVisible(roomLabel);
    roomLabel.setText("Room Size", dontSendNotification);
    roomLabel.setFont(Font(11.0f));
    roomLabel.setColour(Label::textColourId, Colours::lightgrey);
    
    addAndMakeVisible(roomSlider);
    roomSlider.setSliderStyle(Slider::Rotary);
    roomSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 20);
    roomSlider.setColour(Slider::rotarySliderFillColourId, Colour(0xFF6B9FD4));
    roomAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::room, roomSlider));
    
    // Decay
    addAndMakeVisible(decayLabel);
    decayLabel.setText("Decay", dontSendNotification);
    decayLabel.setFont(Font(11.0f));
    decayLabel.setColour(Label::textColourId, Colours::lightgrey);
    
    addAndMakeVisible(decaySlider);
    decaySlider.setSliderStyle(Slider::Rotary);
    decaySlider.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 20);
    decaySlider.setColour(Slider::rotarySliderFillColourId, Colour(0xFF7BC96F));
    decayAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::decay, decaySlider));
    
    // Damping
    addAndMakeVisible(dampingLabel);
    dampingLabel.setText("Damping", dontSendNotification);
    dampingLabel.setFont(Font(11.0f));
    dampingLabel.setColour(Label::textColourId, Colours::lightgrey);
    
    addAndMakeVisible(dampingSlider);
    dampingSlider.setSliderStyle(Slider::Rotary);
    dampingSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 20);
    dampingSlider.setColour(Slider::rotarySliderFillColourId, Colour(0xFFE28C42));
    dampingAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::damping, dampingSlider));
    
    // Pre-Delay
    addAndMakeVisible(preDelayLabel);
    preDelayLabel.setText("Pre-Delay", dontSendNotification);
    preDelayLabel.setFont(Font(11.0f));
    preDelayLabel.setColour(Label::textColourId, Colours::lightgrey);
    
    addAndMakeVisible(preDelaySlider);
    preDelaySlider.setSliderStyle(Slider::Rotary);
    preDelaySlider.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 20);
    preDelaySlider.setColour(Slider::rotarySliderFillColourId, Colour(0xFFC77DFF));
    preDelayAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::preDelay, preDelaySlider));
    
    // Mix
    addAndMakeVisible(mixLabel);
    mixLabel.setText("Mix", dontSendNotification);
    mixLabel.setFont(Font(11.0f));
    mixLabel.setColour(Label::textColourId, Colours::lightgrey);
    
    addAndMakeVisible(mixSlider);
    mixSlider.setSliderStyle(Slider::Rotary);
    mixSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 50, 20);
    mixSlider.setColour(Slider::rotarySliderFillColourId, Colour(0xFF5CB3FF));
    mixAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::mix, mixSlider));
    
    // Bypass
    addAndMakeVisible(bypassButton);
    bypassButton.setButtonText("Bypass");
    bypassButton.setColour(TextButton::buttonColourId, Colour(0xFF404040));
    bypassButton.setColour(TextButton::textColourOnId, Colours::white);
    bypassAttachment.reset(new AudioProcessorValueTreeState::ButtonAttachment(
        processor.getAPVTS(), ParameterIDs::bypass, bypassButton));
}

VCReverbEditor::~VCReverbEditor()
{
}

//==============================================================================
// Paint
//==============================================================================
void VCReverbEditor::paint(Graphics& g)
{
    // Background
    g.fillAll(Colour(0xFF1E2530));
    
    // Border
    g.setColour(Colour(0xFF2A3540));
    g.drawRect(getLocalBounds().reduced(1), 1);
}

namespace {
    void layoutSlider(juce::Component& slider, juce::Component& label, 
                       int x, int y, int size) {
        label.setBounds(x, y + size - 25, size, 18);
        slider.setBounds(x, y, size, size);
    }
}

//==============================================================================
// Resize
//==============================================================================
void VCReverbEditor::resized()
{
    int margin = 20;
    int knobSize = 70;
    int row1Y = 60;
    int row2Y = 170;
    int row3Y = 280;
    
    // Title
    titleLabel.setBounds(margin, 15, 200, 30);
    subtitleLabel.setBounds(margin, 40, 250, 18);
    
    // Row 1: Room Size, Decay, Damping
    int row1Start = margin;
    int spacing = 90;
    layoutSlider(roomSlider, roomLabel, row1Start, row1Y, knobSize);
    layoutSlider(decaySlider, decayLabel, row1Start + spacing, row1Y, knobSize);
    layoutSlider(dampingSlider, dampingLabel, row1Start + spacing * 2, row1Y, knobSize);
    
    // Row 2: Pre-Delay, Mix
    int row2Start = margin + 45;
    layoutSlider(preDelaySlider, preDelayLabel, row2Start, row2Y, knobSize);
    layoutSlider(mixSlider, mixLabel, row2Start + spacing * 2, row2Y, knobSize);
    
    // Bypass button
    bypassButton.setBounds(getWidth() - 100, 15, 80, 30);
}
