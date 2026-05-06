#pragma once

#include "PluginProcessor.h"

// JUCE GUI 头文件
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
class VCSmoothEditor : public juce::AudioProcessorEditor
{
public:
    VCSmoothEditor (VCSmoothProcessor&);
    ~VCSmoothEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    //==============================================================================
    // 控件
    // 顶部栏
    juce::TextButton bypassBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    
    juce::TextButton abBtn;
    juce::Label abLabel;
    
    // 核心参数
    juce::Label depthLabel;
    juce::Slider depthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttachment;
    
    juce::Label speedLabel;
    juce::Slider speedSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttachment;
    
    juce::Label freqLowLabel;
    juce::Slider freqLowSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqLowAttachment;
    
    juce::Label freqHighLabel;
    juce::Slider freqHighSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqHighAttachment;
    
    juce::Label sharpnessLabel;
    juce::Slider sharpnessSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sharpnessAttachment;
    
    juce::Label mixLabel;
    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    
    // 输入输出
    juce::Label inputLabel;
    juce::Slider inputSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment;
    
    juce::Label outputLabel;
    juce::Slider outputSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    
    // 预设
    juce::Label presetLabel;
    juce::ComboBox presetBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttachment;
    
    // 处理器引用
    VCSmoothProcessor& processor;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCSmoothEditor)
};
