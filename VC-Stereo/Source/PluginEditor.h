#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// VC-Stereo Plugin Editor
//==============================================================================
class VCStereoEditor : public juce::AudioProcessorEditor
{
public:
    VCStereoEditor(VCStereoProcessor&);
    ~VCStereoEditor() override;

    //============================================================================
    // Painting and Layout
    //============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    //============================================================================
    // UI Controls
    //============================================================================
    juce::Slider widthSlider;
    juce::Label  widthLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttachment;

    juce::Slider panSlider;
    juce::Label  panLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAttachment;

    juce::ToggleButton monoBassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoBassAttachment;

    juce::Slider bassFreqSlider;
    juce::Label  bassFreqLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bassFreqAttachment;

    //============================================================================
    // Processor reference
    //============================================================================
    VCStereoProcessor& processor;

    //============================================================================
    // Non-copyable
    //============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCStereoEditor)
};
