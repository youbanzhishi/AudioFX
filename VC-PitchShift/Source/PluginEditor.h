#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// VC-PitchShift Plugin Editor
//==============================================================================
class VCPitchShiftEditor : public juce::AudioProcessorEditor
{
public:
    VCPitchShiftEditor(VCPitchShiftProcessor&);
    ~VCPitchShiftEditor() override;

    //============================================================================
    // Painting and Layout
    //============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    //============================================================================
    // UI Controls
    //============================================================================
    juce::Slider semitonesSlider;
    juce::Label  semitonesLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> semitonesAttachment;

    juce::Slider centsSlider;
    juce::Label  centsLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> centsAttachment;

    juce::ToggleButton formantButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> formantAttachment;

    //============================================================================
    // Processor reference
    //============================================================================
    VCPitchShiftProcessor& processor;

    //============================================================================
    // Non-copyable
    //============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCPitchShiftEditor)
};
