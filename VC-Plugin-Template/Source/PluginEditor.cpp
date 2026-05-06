#include "PluginEditor.h"

using namespace juce;

// ============================================================
// 常量定义
// ============================================================
constexpr int PLUGIN_WIDTH = 500;
constexpr int PLUGIN_HEIGHT = 400;

// ============================================================
// 构造函数
// ============================================================
[[PLUGIN_NAME]]Editor::[[PLUGIN_NAME]]Editor ([[PLUGIN_NAME]]Processor& p)
    : AudioProcessorEditor (&p)
    , processor (p)
{
    // 设置编辑器大小
    setSize (PLUGIN_WIDTH, PLUGIN_HEIGHT);
    
    // ============================================================
    // TODO: 创建和添加控件
    //
    // 示例:
    // // Bypass 按钮
    // addAndMakeVisible(bypassBtn);
    // bypassBtn.setButtonText("Bypass");
    // bypassAttachment.reset(new AudioProcessorValueTreeState::ButtonAttachment(
    //     processor.getAPVTS(), ParameterIDs::bypass, bypassBtn));
    //
    // // Gain 旋钮
    // addAndMakeVisible(gainLabel);
    // gainLabel.setText("Gain", dontSendNotification);
    //
    // addAndMakeVisible(gainSlider);
    // gainSlider.setSliderStyle(Slider::Rotary);
    // gainSlider.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 20);
    // gainSlider.setColour(Slider::rotarySliderFillColourId, Colours::cyan);
    // gainAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
    //     processor.getAPVTS(), ParameterIDs::gain, gainSlider));
    // ============================================================
}

[[PLUGIN_NAME]]Editor::~[[PLUGIN_NAME]]Editor()
{
}

// ============================================================
// 绘制
// ============================================================
void [[PLUGIN_NAME]]Editor::paint (Graphics& g)
{
    // 背景
    g.fillAll (Colour (0xFF1E2530));
    
    // ============================================================
    // TODO: 绘制界面元素
    // ============================================================
    
    // 标题
    g.setColour (Colours::white);
    g.setFont (Font (18.0f, Font::bold));
    g.drawFittedText ("[[PLUGIN_NAME]]", 15, 15, 200, 30, Justification::left, 1);
    
    // 副标题
    g.setColour (Colours::grey);
    g.setFont (Font (12.0f));
    g.drawFittedText ("Audio Plugin", 15, 42, 200, 20, Justification::left, 1);
}

// ============================================================
// 布局
// ============================================================
void [[PLUGIN_NAME]]Editor::resized ()
{
    // ============================================================
    // TODO: 布局控件位置
    //
    // 示例:
    // int y = 80;
    // int knobSize = 70;
    // int spacing = 90;
    //
    // gainSlider.setBounds(20, y, knobSize, knobSize);
    // gainLabel.setBounds(20, y + knobSize + 2, knobSize, 18);
    //
    // bypassBtn.setBounds(getWidth() - 80, 15, 60, 25);
    // ============================================================
}
