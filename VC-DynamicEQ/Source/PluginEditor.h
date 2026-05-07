#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Plugin Editor
// VC-DynamicEQ - Dynamic Equalizer
//==============================================================================
class VCDynamicEQEditor : public juce::AudioProcessorEditor
{
public:
    VCDynamicEQEditor(VCDynamicEQProcessor&);
    ~VCDynamicEQEditor() override;

    //============================================================================
    // Painting and Layout
    //============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    //============================================================================
    // Processor reference
    //============================================================================
    VCDynamicEQProcessor& processor;

    //============================================================================
    // UI Components
    //============================================================================
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    
    // Bypass button
    juce::ToggleButton bypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    
    // Frequency
    juce::Slider freqSlider;
    juce::Label freqLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment;
    
    // Gain
    juce::Slider gainSlider;
    juce::Label gainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    
    // Q
    juce::Slider qSlider;
    juce::Label qLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttachment;
    
    // Threshold
    juce::Slider thresholdSlider;
    juce::Label thresholdLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttachment;
    
    // Range
    juce::Slider rangeSlider;
    juce::Label rangeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rangeAttachment;
    
    // Attack
    juce::Slider attackSlider;
    juce::Label attackLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    
    // Release
    juce::Slider releaseSlider;
    juce::Label releaseLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    
    // Mix
    juce::Slider mixSlider;
    juce::Label mixLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    //============================================================================
    // Non-copyable
    //============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCDynamicEQEditor)
};
