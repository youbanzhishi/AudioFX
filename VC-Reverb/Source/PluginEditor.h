#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// VC-Reverb Plugin Editor
//==============================================================================
class VCReverbEditor : public juce::AudioProcessorEditor
{
public:
    VCReverbEditor(VCReverbProcessor&);
    ~VCReverbEditor() override;

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
    
    // Room Size
    juce::Label roomLabel;
    juce::Slider roomSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> roomAttachment;
    
    // Decay
    juce::Label decayLabel;
    juce::Slider decaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    
    // Damping
    juce::Label dampingLabel;
    juce::Slider dampingSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dampingAttachment;
    
    // Pre-Delay
    juce::Label preDelayLabel;
    juce::Slider preDelaySlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> preDelayAttachment;
    
    // Mix
    juce::Label mixLabel;
    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    
    // Bypass
    juce::TextButton bypassButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    //============================================================================
    // Processor reference
    //============================================================================
    VCReverbProcessor& processor;

    //============================================================================
    // Non-copyable
    //============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCReverbEditor)
};
