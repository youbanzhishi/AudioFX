#include "PluginEditor.h"

using namespace juce;

//==============================================================================
constexpr int PLUGIN_WIDTH = 560;
constexpr int PLUGIN_HEIGHT = 520;

// Color palette — dark blue-grey theme
namespace Colors
{
    constexpr uint32 bg          = 0xFF1A1E2E;
    constexpr uint32 border      = 0xFF2A3040;
    constexpr uint32 titleText   = 0xFFD0D8E8;
    constexpr uint32 labelText   = 0xFF8898A8;
    constexpr uint32 knobFill1   = 0xFF6B9FD4; // blue
    constexpr uint32 knobFill2   = 0xFF7BC96F; // green
    constexpr uint32 knobFill3   = 0xFFE28C42; // orange
    constexpr uint32 knobFill4   = 0xFFC77DFF; // purple
    constexpr uint32 knobFill5   = 0xFF5CB3FF; // light blue
    constexpr uint32 knobFill6   = 0xFFFF7B7B; // red
}

//==============================================================================
// Construction / Destruction
//==============================================================================
VCHall480Editor::VCHall480Editor(VCHall480Processor& p)
    : AudioProcessorEditor(&p)
    , processor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);

    // Title
    addAndMakeVisible(titleLabel);
    titleLabel.setText("VC-Hall480", dontSendNotification);
    titleLabel.setFont(Font(22.0f, Font::bold));
    titleLabel.setColour(Label::textColourId, Colour(Colors::titleText));

    addAndMakeVisible(subtitleLabel);
    subtitleLabel.setText("Lexicon 480L-Class Reverb", dontSendNotification);
    subtitleLabel.setFont(Font(11.0f));
    subtitleLabel.setColour(Label::textColourId, Colour(Colors::labelText));

    // Algorithm selector
    addAndMakeVisible(algoLabel);
    algoLabel.setText("Algorithm", dontSendNotification);
    algoLabel.setFont(Font(11.0f));
    algoLabel.setColour(Label::textColourId, Colour(Colors::labelText));

    addAndMakeVisible(algoBox);
    algoBox.addItem("Hall", 1);
    algoBox.addItem("Random Hall", 2);
    algoBox.addItem("Plate", 3);
    algoAttachment.reset(new AudioProcessorValueTreeState::ComboBoxAttachment(
        processor.getAPVTS(), ParameterIDs::algorithm, algoBox));

    // Helper lambda for creating knobs
    auto makeKnob = [this](Label& label, Slider& slider, const String& name,
                           uint32 color, std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>& attachment,
                           const String& paramID) {
        addAndMakeVisible(label);
        label.setText(name, dontSendNotification);
        label.setFont(Font(10.0f));
        label.setColour(Label::textColourId, Colour(Colors::labelText));
        label.setJustificationType(Justification::centred);

        addAndMakeVisible(slider);
        slider.setSliderStyle(Slider::Rotary);
        slider.setTextBoxStyle(Slider::TextBoxBelow, false, 48, 16);
        slider.setColour(Slider::rotarySliderFillColourId, Colour(color));
        slider.setColour(Slider::textBoxTextColourId, Colour(Colors::labelText));
        slider.setColour(Slider::textBoxBackgroundColourId, Colour(Colors::border));
        slider.setColour(Slider::textBoxHighlightColourId, Colour(color));

        attachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
            processor.getAPVTS(), paramID, slider));
    };

    // Row 1: Room, Decay, Diffusion
    makeKnob(roomLabel, roomSlider, "Room", Colors::knobFill1, roomAttachment, ParameterIDs::room);
    makeKnob(decayLabel, decaySlider, "Decay", Colors::knobFill2, decayAttachment, ParameterIDs::decay);
    makeKnob(diffusionLabel, diffusionSlider, "Diffuse", Colors::knobFill3, diffusionAttachment, ParameterIDs::diffusion);

    // Row 2: Shape, Spread, Hi Decay, Lo Decay
    makeKnob(shapeLabel, shapeSlider, "Shape", Colors::knobFill4, shapeAttachment, ParameterIDs::shape);
    makeKnob(spreadLabel, spreadSlider, "Spread", Colors::knobFill5, spreadAttachment, ParameterIDs::spread);
    makeKnob(hiDecayLabel, hiDecaySlider, "Hi Decay", Colors::knobFill6, hiDecayAttachment, ParameterIDs::hiDecay);
    makeKnob(loDecayLabel, loDecaySlider, "Lo Decay", Colors::knobFill1, loDecayAttachment, ParameterIDs::loDecay);

    // Row 3: Chorus Rate, Chorus Depth, Pre-Delay, Mix
    makeKnob(chorusRateLabel, chorusRateSlider, "Ch Rate", Colors::knobFill2, chorusRateAttachment, ParameterIDs::chorusRate);
    makeKnob(chorusDepthLabel, chorusDepthSlider, "Ch Depth", Colors::knobFill3, chorusDepthAttachment, ParameterIDs::chorusDepth);
    makeKnob(preDelayLabel, preDelaySlider, "Pre-Dly", Colors::knobFill4, preDelayAttachment, ParameterIDs::preDelay);
    makeKnob(mixLabel, mixSlider, "Mix", Colors::knobFill5, mixAttachment, ParameterIDs::mix);

    // Bypass
    addAndMakeVisible(bypassButton);
    bypassButton.setButtonText("Bypass");
    bypassButton.setColour(TextButton::buttonColourId, Colour(0xFF404040));
    bypassButton.setColour(TextButton::textColourOnId, Colours::white);
    bypassAttachment.reset(new AudioProcessorValueTreeState::ButtonAttachment(
        processor.getAPVTS(), ParameterIDs::bypass, bypassButton));
}

VCHall480Editor::~VCHall480Editor()
{
}

//==============================================================================
// Paint
//==============================================================================
void VCHall480Editor::paint(Graphics& g)
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
void VCHall480Editor::resized()
{
    int margin = 16;
    int knobSize = 64;
    int colSpacing = 80;
    int rowSpacing = 100;

    // Title area
    titleLabel.setBounds(margin, 12, 200, 26);
    subtitleLabel.setBounds(margin, 36, 220, 14);

    // Algorithm selector (top right)
    algoLabel.setBounds(margin, 60, 80, 16);
    algoBox.setBounds(margin + 80, 58, 130, 22);

    // Bypass button
    bypassButton.setBounds(getWidth() - 90, 12, 74, 26);

    int row1Y = 88;
    int row2Y = row1Y + rowSpacing;
    int row3Y = row2Y + rowSpacing;

    // Row 1: Room, Decay, Diffusion (3 knobs, centered)
    int row1Start = margin + 50;
    layoutKnob(roomSlider, roomLabel, row1Start, row1Y, knobSize);
    layoutKnob(decaySlider, decayLabel, row1Start + colSpacing, row1Y, knobSize);
    layoutKnob(diffusionSlider, diffusionLabel, row1Start + colSpacing * 2, row1Y, knobSize);

    // Row 2: Shape, Spread, Hi Decay, Lo Decay (4 knobs)
    int row2Start = margin + 10;
    layoutKnob(shapeSlider, shapeLabel, row2Start, row2Y, knobSize);
    layoutKnob(spreadSlider, spreadLabel, row2Start + colSpacing, row2Y, knobSize);
    layoutKnob(hiDecaySlider, hiDecayLabel, row2Start + colSpacing * 2, row2Y, knobSize);
    layoutKnob(loDecaySlider, loDecayLabel, row2Start + colSpacing * 3, row2Y, knobSize);

    // Row 3: Chorus Rate, Chorus Depth, Pre-Delay, Mix (4 knobs)
    int row3Start = margin + 10;
    layoutKnob(chorusRateSlider, chorusRateLabel, row3Start, row3Y, knobSize);
    layoutKnob(chorusDepthSlider, chorusDepthLabel, row3Start + colSpacing, row3Y, knobSize);
    layoutKnob(preDelaySlider, preDelayLabel, row3Start + colSpacing * 2, row3Y, knobSize);
    layoutKnob(mixSlider, mixLabel, row3Start + colSpacing * 3, row3Y, knobSize);
}
