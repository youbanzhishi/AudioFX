#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Constants
//==============================================================================
constexpr int PLUGIN_WIDTH = 500;
constexpr int PLUGIN_HEIGHT = 420;

//==============================================================================
// Construction / Destruction
//==============================================================================
VCDynamicEQEditor::VCDynamicEQEditor(VCDynamicEQProcessor& p)
    : AudioProcessorEditor(&p)
    , processor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);

    // Title
    addAndMakeVisible(titleLabel);
    titleLabel.setText("VC-DynamicEQ", juce::dontSendNotification);
    titleLabel.setFont(Font(22.0f, Font::bold));
    titleLabel.setColour(Label::textColourId, Colours::white);
    titleLabel.setBounds(15, 12, 300, 28);

    // Subtitle
    addAndMakeVisible(subtitleLabel);
    subtitleLabel.setText("Dynamic Equalizer", juce::dontSendNotification);
    subtitleLabel.setFont(Font(13.0f));
    subtitleLabel.setColour(Label::textColourId, Colours::grey);
    subtitleLabel.setBounds(15, 40, 200, 18);

    // Bypass button
    addAndMakeVisible(bypassButton);
    bypassButton.setButtonText("Bypass");
    bypassButton.setColour(ToggleButton::textColourId, Colours::white);
    bypassAttachment.reset(new AudioProcessorValueTreeState::ButtonAttachment(
        processor.getAPVTS(), ParameterIDs::bypass, bypassButton));

    // Frequency slider
    addAndMakeVisible(freqSlider);
    freqSlider.setSliderStyle(Slider::Rotary);
    freqSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    freqSlider.setColour(Slider::rotarySliderFillColourId, Colour(0xFF6366F1));
    freqAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::frequency, freqSlider));

    addAndMakeVisible(freqLabel);
    freqLabel.setText("Frequency", juce::dontSendNotification);
    freqLabel.setFont(Font(11.0f));
    freqLabel.setColour(Label::textColourId, Colours::grey);
    freqLabel.setJustificationType(Justification::centred);

    // Gain slider
    addAndMakeVisible(gainSlider);
    gainSlider.setSliderStyle(Slider::Rotary);
    gainSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    gainSlider.setColour(Slider::rotarySliderFillColourId, Colour(0xFF10B981));
    gainAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::gain, gainSlider));

    addAndMakeVisible(gainLabel);
    gainLabel.setText("Gain", juce::dontSendNotification);
    gainLabel.setFont(Font(11.0f));
    gainLabel.setColour(Label::textColourId, Colours::grey);
    gainLabel.setJustificationType(Justification::centred);

    // Q slider
    addAndMakeVisible(qSlider);
    qSlider.setSliderStyle(Slider::Rotary);
    qSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    qSlider.setColour(Slider::rotarySliderFillColourId, Colour(0xFFF59E0B));
    qAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::q, qSlider));

    addAndMakeVisible(qLabel);
    qLabel.setText("Q", juce::dontSendNotification);
    qLabel.setFont(Font(11.0f));
    qLabel.setColour(Label::textColourId, Colours::grey);
    qLabel.setJustificationType(Justification::centred);

    // Threshold slider
    addAndMakeVisible(thresholdSlider);
    thresholdSlider.setSliderStyle(Slider::Rotary);
    thresholdSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    thresholdSlider.setColour(Slider::rotarySliderFillColourId, Colour(0xFFEF4444));
    thresholdAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::threshold, thresholdSlider));

    addAndMakeVisible(thresholdLabel);
    thresholdLabel.setText("Threshold", juce::dontSendNotification);
    thresholdLabel.setFont(Font(11.0f));
    thresholdLabel.setColour(Label::textColourId, Colours::grey);
    thresholdLabel.setJustificationType(Justification::centred);

    // Range slider
    addAndMakeVisible(rangeSlider);
    rangeSlider.setSliderStyle(Slider::Rotary);
    rangeSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    rangeSlider.setColour(Slider::rotarySliderFillColourId, Colour(0xFFEC4899));
    rangeAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::range, rangeSlider));

    addAndMakeVisible(rangeLabel);
    rangeLabel.setText("Range", juce::dontSendNotification);
    rangeLabel.setFont(Font(11.0f));
    rangeLabel.setColour(Label::textColourId, Colours::grey);
    rangeLabel.setJustificationType(Justification::centred);

    // Attack slider
    addAndMakeVisible(attackSlider);
    attackSlider.setSliderStyle(Slider::Rotary);
    attackSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    attackSlider.setColour(Slider::rotarySliderFillColourId, Colour(0xFF06B6D4));
    attackAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::attack, attackSlider));

    addAndMakeVisible(attackLabel);
    attackLabel.setText("Attack", juce::dontSendNotification);
    attackLabel.setFont(Font(11.0f));
    attackLabel.setColour(Label::textColourId, Colours::grey);
    attackLabel.setJustificationType(Justification::centred);

    // Release slider
    addAndMakeVisible(releaseSlider);
    releaseSlider.setSliderStyle(Slider::Rotary);
    releaseSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    releaseSlider.setColour(Slider::rotarySliderFillColourId, Colour(0xFF8B5CF6));
    releaseAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::release, releaseSlider));

    addAndMakeVisible(releaseLabel);
    releaseLabel.setText("Release", juce::dontSendNotification);
    releaseLabel.setFont(Font(11.0f));
    releaseLabel.setColour(Label::textColourId, Colours::grey);
    releaseLabel.setJustificationType(Justification::centred);

    // Mix slider
    addAndMakeVisible(mixSlider);
    mixSlider.setSliderStyle(Slider::Rotary);
    mixSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    mixSlider.setColour(Slider::rotarySliderFillColourId, Colour(0xFFFFFFFF));
    mixAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
        processor.getAPVTS(), ParameterIDs::mix, mixSlider));

    addAndMakeVisible(mixLabel);
    mixLabel.setText("Mix", juce::dontSendNotification);
    mixLabel.setFont(Font(11.0f));
    mixLabel.setColour(Label::textColourId, Colours::grey);
    mixLabel.setJustificationType(Justification::centred);
}

VCDynamicEQEditor::~VCDynamicEQEditor()
{
}

//==============================================================================
// Paint
//==============================================================================
void VCDynamicEQEditor::paint(Graphics& g)
{
    // Background
    g.fillAll(Colour(0xFF1E2530));

    // Header bar
    g.setColour(Colour(0xFF2D3748));
    g.fillRect(0, 0, getWidth(), 70);

    // Bypass button position
    bypassButton.setBounds(getWidth() - 90, 20, 70, 30);
}

//==============================================================================
// Resize
//==============================================================================
void VCDynamicEQEditor::resized()
{
    int knobSize = 60;
    int startY = 85;
    int row1Y = startY;
    int row2Y = startY + 95;
    int col1X = 35;
    int col2X = 130;
    int col3X = 225;
    int col4X = 320;
    int col5X = 415;

    // Row 1: Frequency, Gain, Q, Threshold, Range
    freqSlider.setBounds(col1X, row1Y, knobSize, knobSize);
    freqLabel.setBounds(col1X, row1Y + knobSize + 2, knobSize, 16);

    gainSlider.setBounds(col2X, row1Y, knobSize, knobSize);
    gainLabel.setBounds(col2X, row1Y + knobSize + 2, knobSize, 16);

    qSlider.setBounds(col3X, row1Y, knobSize, knobSize);
    qLabel.setBounds(col3X, row1Y + knobSize + 2, knobSize, 16);

    thresholdSlider.setBounds(col4X, row1Y, knobSize, knobSize);
    thresholdLabel.setBounds(col4X, row1Y + knobSize + 2, knobSize, 16);

    rangeSlider.setBounds(col5X, row1Y, knobSize, knobSize);
    rangeLabel.setBounds(col5X, row1Y + knobSize + 2, knobSize, 16);

    // Row 2: Attack, Release, Mix
    attackSlider.setBounds(col1X, row2Y, knobSize, knobSize);
    attackLabel.setBounds(col1X, row2Y + knobSize + 2, knobSize, 16);

    releaseSlider.setBounds(col2X, row2Y, knobSize, knobSize);
    releaseLabel.setBounds(col2X, row2Y + knobSize + 2, knobSize, 16);

    mixSlider.setBounds(col3X, row2Y, knobSize, knobSize);
    mixLabel.setBounds(col3X, row2Y + knobSize + 2, knobSize, 16);
}
