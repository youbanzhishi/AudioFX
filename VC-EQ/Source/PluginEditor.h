#pragma once

#include "PluginProcessor.h"

// JUCE GUI 头文件
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
class VCEQEditor : public juce::AudioProcessorEditor
{
public:
    VCEQEditor (VCEQProcessor&);
    ~VCEQEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==============================================================================
    // 频段控制组件
    struct BandControl : public juce::Component
    {
        BandControl (VCEQProcessor& p, int bandIndex, const juce::String& name);
        ~BandControl() override;
        
        void resized() override;
        
        VCEQProcessor& processor;
        int bandIndex;
        juce::String bandName;
        
        // 控件
        juce::Label freqLabel, qLabel, gainLabel, typeLabel;
        juce::Slider freqSlider, qSlider, gainSlider;
        juce::ComboBox typeBox;
        juce::TextButton enabledBtn, soloBtn;
        
        // 控件到参数的绑定
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachment;
    };
    
    // 顶部控制
    juce::Label inputGainLabel, outputGainLabel;
    juce::Slider inputGainSlider, outputGainSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;
    
    juce::TextButton bypassBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    
    juce::TextButton abBtn;
    juce::Label abLabel;
    
    // 频段控制
    juce::OwnedArray<BandControl> bandControls;
    
    // 预设选择
    juce::ComboBox presetBox;
    juce::Label presetLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttachment;
    
    VCEQProcessor& processor;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCEQEditor)
};
