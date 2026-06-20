#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// VC-SurgicalDeEsser Plugin Editor
//==============================================================================
class VCSurgicalDeEsserEditor : public juce::AudioProcessorEditor
{
public:
    VCSurgicalDeEsserEditor(VCSurgicalDeEsserProcessor&);
    ~VCSurgicalDeEsserEditor() override;

    //============================================================================
    // Painting and Layout
    //============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    //============================================================================
    // UI Controls
    //============================================================================
    juce::Label titleLabel;
    juce::Label subtitleLabel;

    // Threshold
    juce::Label thresholdLabel;
    juce::Slider thresholdSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttachment;

    // Reduction
    juce::Label reductionLabel;
    juce::Slider reductionSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reductionAttachment;

    // Frequency Low
    juce::Label freqLowLabel;
    juce::Slider freqLowSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqLowAttachment;

    // Frequency High
    juce::Label freqHighLabel;
    juce::Slider freqHighSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqHighAttachment;

    // Mode selector
    juce::Label modeLabel;
    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;

    // Bypass button
    juce::TextButton bypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    //============================================================================
    // Processor reference
    //============================================================================
    VCSurgicalDeEsserProcessor& processor;

    //============================================================================
    // Non-copyable
    //============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCSurgicalDeEsserEditor)
};
