#include "PluginEditor.h"

//==============================================================================
// BandControl 实现
//==============================================================================
VCEQEditor::BandControl::BandControl (VCEQProcessor& p, int index, const juce::String& name)
    : processor (p),
      bandIndex (index),
      bandName (name),
      freqLabel (name + " Freq", "Freq"),
      qLabel (name + " Q", "Q"),
      gainLabel (name + " Gain", "Gain"),
      typeLabel (name + " Type", "Type")
{
    addAndMakeVisible (freqLabel);
    addAndMakeVisible (qLabel);
    addAndMakeVisible (gainLabel);
    addAndMakeVisible (typeLabel);
    
    addAndMakeVisible (freqSlider);
    addAndMakeVisible (qSlider);
    addAndMakeVisible (gainSlider);
    addAndMakeVisible (typeBox);
    addAndMakeVisible (enabledBtn);
    addAndMakeVisible (soloBtn);
    
    // 设置旋钮样式
    for (auto* slider : { &freqSlider, &qSlider, &gainSlider })
    {
        slider->setSliderStyle (juce::Slider::Rotary);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
        slider->setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::cyan);
        slider->setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::darkgrey);
    }
    
    // 滤波器类型选项
    typeBox.addItem ("Low Shelf", 1);
    typeBox.addItem ("High Shelf", 2);
    typeBox.addItem ("Parametric", 3);
    
    // 按钮文字
    enabledBtn.setButtonText ("ON");
    soloBtn.setButtonText ("S");
    
    // 按钮颜色 - 使用 TextButton 的颜色
    enabledBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colours::green);
    enabledBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::darkgreen);
    soloBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colours::yellow);
    soloBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::darkgoldenrod);
    
    // 绑定参数
    auto& apvts = processor.getAPVTS();
    
    freqAttachment.reset (new juce::AudioProcessorValueTreeState::SliderAttachment (
        apvts, ParameterIDs::getBandParam(bandIndex, ParameterIDs::freqSuffix), freqSlider));
    
    qAttachment.reset (new juce::AudioProcessorValueTreeState::SliderAttachment (
        apvts, ParameterIDs::getBandParam(bandIndex, ParameterIDs::qSuffix), qSlider));
    
    gainAttachment.reset (new juce::AudioProcessorValueTreeState::SliderAttachment (
        apvts, ParameterIDs::getBandParam(bandIndex, ParameterIDs::gainSuffix), gainSlider));
    
    typeAttachment.reset (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (
        apvts, ParameterIDs::getBandParam(bandIndex, ParameterIDs::typeSuffix), typeBox));
    
    enabledAttachment.reset (new juce::AudioProcessorValueTreeState::ButtonAttachment (
        apvts, ParameterIDs::getBandParam(bandIndex, ParameterIDs::enabledSuffix), enabledBtn));
    
    soloAttachment.reset (new juce::AudioProcessorValueTreeState::ButtonAttachment (
        apvts, ParameterIDs::getBandParam(bandIndex, ParameterIDs::soloSuffix), soloBtn));
}

VCEQEditor::BandControl::~BandControl()
{
}

void VCEQEditor::BandControl::resized()
{
    auto bounds = getLocalBounds().reduced (4);
    
    // 布局：旋钮行 + 按钮行
    auto topRow = bounds.removeFromTop (bounds.getHeight() * 0.75);
    auto bottomRow = bounds;
    
    // 顶部：三个旋钮 + 类型选择
    auto freqArea = topRow.removeFromLeft (topRow.getWidth() / 3);
    auto qArea = topRow.removeFromLeft (topRow.getWidth() / 2);
    auto gainArea = topRow;
    
    freqLabel.setBounds (freqArea.removeFromTop (18));
    freqSlider.setBounds (freqArea);
    
    qLabel.setBounds (qArea.removeFromTop (18));
    qSlider.setBounds (qArea);
    
    gainLabel.setBounds (gainArea.removeFromTop (18));
    gainSlider.setBounds (gainArea);
    
    // 类型选择（放在左侧）
    typeLabel.setBounds (10, 10, 40, 18);
    typeBox.setBounds (10, 30, 80, 24);
    
    // 底部：启用和Solo按钮
    auto btnWidth = bottomRow.getWidth() / 2;
    enabledBtn.setBounds (bottomRow.removeFromLeft (btnWidth).reduced (4));
    soloBtn.setBounds (bottomRow.reduced (4));
}

//==============================================================================
// VCEQEditor 实现
//==============================================================================
VCEQEditor::VCEQEditor (VCEQProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p),
      inputGainLabel ("Input Gain", "Input"),
      outputGainLabel ("Output Gain", "Output"),
      abLabel ("AB", "A"),
      presetLabel ("Preset", "Preset")
{
    setSize (900, 500);
    
    // 顶部控制
    addAndMakeVisible (inputGainLabel);
    addAndMakeVisible (outputGainLabel);
    addAndMakeVisible (inputGainSlider);
    addAndMakeVisible (outputGainSlider);
    addAndMakeVisible (bypassBtn);
    addAndMakeVisible (abBtn);
    addAndMakeVisible (abLabel);
    
    bypassBtn.setButtonText ("Bypass");
    bypassBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colours::red);
    bypassBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::darkred);
    
    abBtn.setButtonText ("A/B");
    
    // 绑定顶部参数
    auto& apvts = processor.getAPVTS();
    
    inputGainAttachment.reset (new juce::AudioProcessorValueTreeState::SliderAttachment (
        apvts, ParameterIDs::inputGain, inputGainSlider));
    
    outputGainAttachment.reset (new juce::AudioProcessorValueTreeState::SliderAttachment (
        apvts, ParameterIDs::outputGain, outputGainSlider));
    
    bypassAttachment.reset (new juce::AudioProcessorValueTreeState::ButtonAttachment (
        apvts, ParameterIDs::bypass, bypassBtn));
    
    // 设置输入输出旋钮样式
    for (auto* slider : { &inputGainSlider, &outputGainSlider })
    {
        slider->setSliderStyle (juce::Slider::Rotary);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
        slider->setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::orange);
        slider->setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::darkgrey);
    }
    
    // 创建5个频段控制
    const juce::String bandNames[] = { "Low Shelf", "Low Mid", "Mid", "High Mid", "High Shelf" };
    for (int i = 0; i < EQBands::kNumBands; ++i)
    {
        bandControls.add (new BandControl (processor, i, bandNames[i]));
        addAndMakeVisible (bandControls[i]);
    }
    
    // 预设选择
    addAndMakeVisible (presetLabel);
    addAndMakeVisible (presetBox);
    presetBox.addItem ("Flat", 1);
    presetBox.addItem ("Bass Boost", 2);
    presetBox.addItem ("Presence", 3);
    presetBox.addItem ("Air", 4);
    
    presetAttachment.reset (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (
        apvts, ParameterIDs::preset, presetBox));
}

VCEQEditor::~VCEQEditor()
{
}

void VCEQEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
    
    // 标题
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (24.0f));
    g.drawText ("VC-EQ 5-Band Parametric EQ", 20, 10, 400, 30, juce::Justification::left);
}

void VCEQEditor::resized()
{
    auto bounds = getLocalBounds().reduced (10);
    
    // 顶部区域
    auto topArea = bounds.removeFromTop (60);
    
    // 左侧：输入/输出增益
    auto gainArea = topArea.removeFromLeft (300);
    inputGainLabel.setBounds (gainArea.removeFromLeft (100).reduced (4));
    inputGainSlider.setBounds (gainArea.removeFromLeft (100).reduced (4));
    outputGainLabel.setBounds (gainArea.removeFromLeft (100).reduced (4));
    outputGainSlider.setBounds (gainArea.reduced (4));
    
    // 中间：Bypass 和 A/B
    auto centerArea = topArea;
    bypassBtn.setBounds (centerArea.removeFromLeft (100).reduced (8));
    abBtn.setBounds (centerArea.removeFromLeft (80).reduced (8));
    abLabel.setBounds (centerArea.removeFromLeft (30).reduced (4));
    
    // 右侧：预设
    auto presetArea = topArea;
    presetLabel.setBounds (presetArea.removeFromLeft (60).reduced (4));
    presetBox.setBounds (presetArea.removeFromLeft (150).reduced (4));
    
    // 中间区域：5个频段
    auto bandsArea = bounds;
    int bandWidth = bandsArea.getWidth() / EQBands::kNumBands;
    for (int i = 0; i < EQBands::kNumBands; ++i)
    {
        bandControls[i]->setBounds (bandsArea.removeFromLeft (bandWidth).reduced (2));
    }
    
    // 底部留空
}
