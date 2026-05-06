#pragma once

#include "PluginProcessor.h"

// JUCE GUI 头文件
#include <juce_gui_basics/juce_gui_basics.h>

// ============================================================
// 主编辑器类
// ============================================================
class [[PLUGIN_NAME]]Editor : public juce::AudioProcessorEditor
{
public:
    [[PLUGIN_NAME]]Editor ([[PLUGIN_NAME]]Processor&);
    ~[[PLUGIN_NAME]]Editor() override;

    // ============================================================
    // 绘制和布局
    // ============================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // ============================================================
    // TODO: 定义控件成员
    //
    // 示例:
    // juce::Label gainLabel;
    // juce::Slider gainSlider;
    // std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    //
    // juce::TextButton bypassBtn;
    // std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    // ============================================================
    
    // ============================================================
    // 处理器引用
    // ============================================================
    [[PLUGIN_NAME]]Processor& processor;
    
    // ============================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ([[PLUGIN_NAME]]Editor)
};
