#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Constants
//==============================================================================
constexpr int PLUGIN_WIDTH = 400;
constexpr int PLUGIN_HEIGHT = 420;

// Color palette — dark blue-grey theme
namespace Colors
{
    constexpr uint32 bg          = 0xFF1A1E2E;
    constexpr uint32 border      = 0xFF2A3040;
    constexpr uint32 titleText   = 0xFFD0D8E8;
    constexpr uint32 labelText   = 0xFF8898A8;
    constexpr uint32 knobFill1   = 0xFF6B9FD4; // blue - threshold
    constexpr uint32 knobFill2   = 0xFF7BC96F; // green - reduction
    constexpr uint32 knobFill3   = 0xFFE28C42; // orange - freq
    constexpr uint32 knobFill4   = 0xFFC77DFF; // purple - mode
}

//==============================================================================
// Construction / Destruction
//==============================================================================
VCSurgicalDeEsserEditor::VCSurgicalDeEsserEditor(VCSurgicalDeEsserProcessor& p)
    : AudioProcessorEditor(&p)
    , processor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);

    // Title
    addAndMakeVisible(titleLabel);
    titleLabel.setText("VC-SurgicalDeEsser", dontSendNotification);
    titleLabel.setFont(Font(22.0f, Font::bold));
    titleLabel.setColour(Label::textColourId, Colour(Colors::titleText));

    // Subtitle
    addAndMakeVisible(subtitleLabel);
    subtitleLabel.setText("Surgical De-Esser", dontSendNotification);
    subtitleLabel.setFont(Font(11.0f));
    subtitleLabel.setColour(Label::textColourId, Colour(Colors::labelText));

    // Helper lambda for creating knobs
    auto makeKnob = [this](Label& label, Slider& slider, const String& name,
                           const String& unit, uint32 color,
                           std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>& attachment,
                           const String& paramID) {
        addAndMakeVisible(label);
        label.setText(name, dontSendNotification);
        label.setFont(Font(10.0f));
        label.setColour(Label::textColourId, Colour(Colors::labelText));
        label.setJustificationType(Justification::centred);

        addAndMakeVisible(slider);
        slider.setSliderStyle(Slider::Rotary);
        slider.setTextBoxStyle(Slider::TextBoxBelow, false, 52, 16);
        slider.setColour(Slider::rotarySliderFillColourId, Colour(color));
        slider.setColour(Slider::textBoxTextColourId, Colour(Colors::labelText));
        slider.setColour(Slider::textBoxBackgroundColourId, Colour(Colors::border));
        slider.setColour(Slider::textBoxHighlightColourId, Colour(color));

        attachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
            processor.getAPVTS(), paramID, slider));
    };

    // Threshold knob
    makeKnob(thresholdLabel, thresholdSlider, "Threshold", "dBFS", 
             Colors::knobFill1, thresholdAttachment, ParameterIDs::threshold);

    // Reduction knob
    makeKnob(reductionLabel, reductionSlider, "Reduction", "dB",
             Colors::knobFill2, reductionAttachment, ParameterIDs::reduction);

    // Frequency Low knob
    makeKnob(freqLowLabel, freqLowSlider, "Freq Low", "Hz",
             Colors::knobFill3, freqLowAttachment, ParameterIDs::freqLow);

    // Frequency High knob
    makeKnob(freqHighLabel, freqHighSlider, "Freq High", "Hz",
             Colors::knobFill3, freqHighAttachment, ParameterIDs::freqHigh);

    // Mode selector
    addAndMakeVisible(modeLabel);
    modeLabel.setText("Mode", dontSendNotification);
    modeLabel.setFont(Font(10.0f));
    modeLabel.setColour(Label::textColourId, Colour(Colors::labelText));

    addAndMakeVisible(modeBox);
    modeBox.addItem("Gain Reduction", 1);
    modeBox.addItem("Dynamic EQ", 2);
    modeBox.setColour(ComboBox::backgroundColourId, Colour(Colors::border));
    modeBox.setColour(ComboBox::textColourId, Colour(Colors::titleText));
    modeBox.setColour(ComboBox::arrowColourId, Colour(Colors::labelText));
    modeAttachment.reset(new AudioProcessorValueTreeState::ComboBoxAttachment(
        processor.getAPVTS(), ParameterIDs::mode, modeBox));

    // Bypass button
    addAndMakeVisible(bypassButton);
    bypassButton.setButtonText("Bypass");
    bypassButton.setColour(TextButton::buttonColourId, Colour(0xFF404040));
    bypassButton.setColour(TextButton::textColourOnId, Colours::white);
    bypassButton.setColour(TextButton::textColourOffId, Colour(Colors::labelText));
    bypassAttachment.reset(new AudioProcessorValueTreeState::ButtonAttachment(
        processor.getAPVTS(), ParameterIDs::bypass, bypassButton));
}

VCSurgicalDeEsserEditor::~VCSurgicalDeEsserEditor()
{
}

//==============================================================================
// Paint
//==============================================================================
void VCSurgicalDeEsserEditor::paint(Graphics& g)
{
    g.fillAll(Colour(Colors::bg));
    g.setColour(Colour(Colors::border));
    g.drawRect(getLocalBounds().reduced(1), 1);
}

namespace {
    void layoutKnob(Component& slider, Component& label, int x, int y, int size)
    {
        slider.setBounds(x, y, size, size);
        label.setBounds(x - 4, y + size - 14, size + 8, 14);
    }
}

//==============================================================================
// Resize
//==============================================================================
void VCSurgicalDeEsserEditor::resized()
{
    int margin = 20;
    int knobSize = 70;
    int colSpacing = 90;
    int rowSpacing = 95;

    // Title area
    titleLabel.setBounds(margin, 12, 220, 28);
    subtitleLabel.setBounds(margin, 38, 180, 16);

    // Bypass button
    bypassButton.setBounds(getWidth() - 80, 14, 60, 26);

    // Row 1: Threshold, Reduction
    int row1Y = 70;
    layoutKnob(thresholdSlider, thresholdLabel, margin + 20, row1Y, knobSize);
    layoutKnob(reductionSlider, reductionLabel, margin + colSpacing + 40, row1Y, knobSize);

    // Row 2: Freq Low, Freq High
    int row2Y = row1Y + rowSpacing;
    layoutKnob(freqLowSlider, freqLowLabel, margin + 20, row2Y, knobSize);
    layoutKnob(freqHighSlider, freqHighLabel, margin + colSpacing + 40, row2Y, knobSize);

    // Mode selector
    modeLabel.setBounds(margin, row2Y + rowSpacing + 10, 50, 20);
    modeBox.setBounds(margin + 55, row2Y + rowSpacing + 8, 130, 24);
}
