#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// VC-Hall480 Plugin Editor
//==============================================================================
class VCHall480Editor : public juce::AudioProcessorEditor
{
public:
    VCHall480Editor(VCHall480Processor&);
    ~VCHall480Editor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    //==========================================================================
    // UI Controls
    //==========================================================================
    juce::Label titleLabel;
    juce::Label subtitleLabel;

    // Algorithm selector
    juce::Label algoLabel;
    juce::ComboBox algoBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algoAttachment;

    // Row 1: Room, Decay, Diffusion
    juce::Label roomLabel;
    juce::Slider roomSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> roomAttachment;

    juce::Label decayLabel;
    juce::Slider decaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;

    juce::Label diffusionLabel;
    juce::Slider diffusionSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> diffusionAttachment;

    // Row 2: Shape, Spread, Hi Decay, Lo Decay
    juce::Label shapeLabel;
    juce::Slider shapeSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shapeAttachment;

    juce::Label spreadLabel;
    juce::Slider spreadSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spreadAttachment;

    juce::Label hiDecayLabel;
    juce::Slider hiDecaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hiDecayAttachment;

    juce::Label loDecayLabel;
    juce::Slider loDecaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> loDecayAttachment;

    // Row 3: Chorus Rate, Chorus Depth, Pre-Delay, Mix
    juce::Label chorusRateLabel;
    juce::Slider chorusRateSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusRateAttachment;

    juce::Label chorusDepthLabel;
    juce::Slider chorusDepthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chorusDepthAttachment;

    juce::Label preDelayLabel;
    juce::Slider preDelaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preDelayAttachment;

    juce::Label mixLabel;
    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    // Bypass
    juce::TextButton bypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    VCHall480Processor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCHall480Editor)
};
