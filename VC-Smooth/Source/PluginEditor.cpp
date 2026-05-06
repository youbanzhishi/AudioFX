#include "PluginEditor.h"

using namespace juce;

//==============================================================================
VCSmoothEditor::VCSmoothEditor (VCSmoothProcessor& p)
    : AudioProcessorEditor (&p)
    , processor (p)
{
    // 设置编辑器大小
    setSize (650, 480);
    
    // ========== 创建控件 ==========
    
    // Bypass 按钮
    addAndMakeVisible (bypassBtn);
    bypassBtn.setButtonText ("Bypass");
    bypassBtn.setColour (TextButton::buttonColourId, Colours::red.withAlpha (0.8f));
    bypassAttachment.reset (new AudioProcessorValueTreeState::ButtonAttachment (
        processor.getAPVTS(), ParameterIDs::bypass, bypassBtn));
    
    // A/B 按钮
    addAndMakeVisible (abBtn);
    abBtn.setButtonText ("A/B");
    abBtn.setColour (TextButton::buttonColourId, Colours::orange.withAlpha (0.8f));
    
    // 预设选择
    addAndMakeVisible (presetLabel);
    presetLabel.setText ("Preset:", dontSendNotification);
    presetLabel.attachToComponent (&presetBox, true);
    
    addAndMakeVisible (presetBox);
    presetBox.addItem ("Default", 1);
    presetBox.addItem ("Vocal Warm", 2);
    presetBox.addItem ("Vocal Bright", 3);
    presetBox.addItem ("Aggressive", 4);
    presetBox.setSelectedItemIndex (0);
    presetAttachment.reset (new AudioProcessorValueTreeState::ComboBoxAttachment (
        processor.getAPVTS(), ParameterIDs::paramSet, presetBox));
    
    // ========== 核心参数 ==========
    
    // Depth (0-1)
    addAndMakeVisible (depthLabel);
    depthLabel.setText ("Depth", dontSendNotification);
    depthLabel.setJustificationType (Justification::centred);
    
    addAndMakeVisible (depthSlider);
    depthSlider.setSliderStyle (Slider::Rotary);
    depthSlider.setTextBoxStyle (Slider::TextBoxBelow, false, 60, 20);
    depthSlider.setColour (Slider::rotarySliderFillColourId, Colours::cyan);
    depthSlider.setColour (Slider::rotarySliderOutlineColourId, Colours::darkgrey);
    depthAttachment.reset (new AudioProcessorValueTreeState::SliderAttachment (
        processor.getAPVTS(), ParameterIDs::depth, depthSlider));
    
    // Speed (0.1-10)
    addAndMakeVisible (speedLabel);
    speedLabel.setText ("Speed", dontSendNotification);
    speedLabel.setJustificationType (Justification::centred);
    
    addAndMakeVisible (speedSlider);
    speedSlider.setSliderStyle (Slider::Rotary);
    speedSlider.setTextBoxStyle (Slider::TextBoxBelow, false, 60, 20);
    speedSlider.setColour (Slider::rotarySliderFillColourId, Colours::cyan);
    speedSlider.setColour (Slider::rotarySliderOutlineColourId, Colours::darkgrey);
    speedAttachment.reset (new AudioProcessorValueTreeState::SliderAttachment (
        processor.getAPVTS(), ParameterIDs::speed, speedSlider));
    
    // Freq Low (20-20000 Hz)
    addAndMakeVisible (freqLowLabel);
    freqLowLabel.setText ("Freq Low", dontSendNotification);
    freqLowLabel.setJustificationType (Justification::centred);
    
    addAndMakeVisible (freqLowSlider);
    freqLowSlider.setSliderStyle (Slider::Rotary);
    freqLowSlider.setTextBoxStyle (Slider::TextBoxBelow, false, 60, 20);
    freqLowSlider.setColour (Slider::rotarySliderFillColourId, Colours::lightgreen);
    freqLowAttachment.reset (new AudioProcessorValueTreeState::SliderAttachment (
        processor.getAPVTS(), ParameterIDs::freqLow, freqLowSlider));
    
    // Freq High (20-20000 Hz)
    addAndMakeVisible (freqHighLabel);
    freqHighLabel.setText ("Freq High", dontSendNotification);
    freqHighLabel.setJustificationType (Justification::centred);
    
    addAndMakeVisible (freqHighSlider);
    freqHighSlider.setSliderStyle (Slider::Rotary);
    freqHighSlider.setTextBoxStyle (Slider::TextBoxBelow, false, 60, 20);
    freqHighSlider.setColour (Slider::rotarySliderFillColourId, Colours::lightgreen);
    freqHighAttachment.reset (new AudioProcessorValueTreeState::SliderAttachment (
        processor.getAPVTS(), ParameterIDs::freqHigh, freqHighSlider));
    
    // Sharpness (0.1-5.0)
    addAndMakeVisible (sharpnessLabel);
    sharpnessLabel.setText ("Sharpness", dontSendNotification);
    sharpnessLabel.setJustificationType (Justification::centred);
    
    addAndMakeVisible (sharpnessSlider);
    sharpnessSlider.setSliderStyle (Slider::Rotary);
    sharpnessSlider.setTextBoxStyle (Slider::TextBoxBelow, false, 60, 20);
    sharpnessSlider.setColour (Slider::rotarySliderFillColourId, Colours::orange);
    sharpnessSlider.setColour (Slider::rotarySliderOutlineColourId, Colours::darkgrey);
    sharpnessAttachment.reset (new AudioProcessorValueTreeState::SliderAttachment (
        processor.getAPVTS(), ParameterIDs::sharpness, sharpnessSlider));
    
    // Mix (0-1)
    addAndMakeVisible (mixLabel);
    mixLabel.setText ("Mix", dontSendNotification);
    mixLabel.setJustificationType (Justification::centred);
    
    addAndMakeVisible (mixSlider);
    mixSlider.setSliderStyle (Slider::Rotary);
    mixSlider.setTextBoxStyle (Slider::TextBoxBelow, false, 60, 20);
    mixSlider.setColour (Slider::rotarySliderFillColourId, Colours::cyan);
    mixSlider.setColour (Slider::rotarySliderOutlineColourId, Colours::darkgrey);
    mixAttachment.reset (new AudioProcessorValueTreeState::SliderAttachment (
        processor.getAPVTS(), ParameterIDs::mix, mixSlider));
    
    // ========== 输入输出 ==========
    
    // Input
    addAndMakeVisible (inputLabel);
    inputLabel.setText ("Input", dontSendNotification);
    inputLabel.setJustificationType (Justification::centred);
    
    addAndMakeVisible (inputSlider);
    inputSlider.setSliderStyle (Slider::Rotary);
    inputSlider.setTextBoxStyle (Slider::TextBoxBelow, false, 50, 18);
    inputSlider.setColour (Slider::rotarySliderFillColourId, Colours::yellow);
    inputAttachment.reset (new AudioProcessorValueTreeState::SliderAttachment (
        processor.getAPVTS(), ParameterIDs::inputGain, inputSlider));
    
    // Output
    addAndMakeVisible (outputLabel);
    outputLabel.setText ("Output", dontSendNotification);
    outputLabel.setJustificationType (Justification::centred);
    
    addAndMakeVisible (outputSlider);
    outputSlider.setSliderStyle (Slider::Rotary);
    outputSlider.setTextBoxStyle (Slider::TextBoxBelow, false, 50, 18);
    outputSlider.setColour (Slider::rotarySliderFillColourId, Colours::yellow);
    outputAttachment.reset (new AudioProcessorValueTreeState::SliderAttachment (
        processor.getAPVTS(), ParameterIDs::outputGain, outputSlider));
}

VCSmoothEditor::~VCSmoothEditor()
{
}

//==============================================================================
void VCSmoothEditor::paint (Graphics& g)
{
    // 背景
    g.fillAll (Colour (0xFF1E2530));
    
    // 标题区域
    g.setColour (Colours::white);
    g.setFont (Font (18.0f, Font::bold));
    g.drawFittedText ("VC-Smooth", 15, 10, 150, 30, Justification::left, 1);
    
    // 副标题
    g.setColour (Colours::grey);
    g.setFont (Font (12.0f));
    g.drawFittedText ("Spectral Resonance Suppressor", 15, 38, 220, 20, Justification::left, 1);
    
    // 分隔线
    g.setColour (Colours::grey.withAlpha (0.3f));
    g.drawHorizontalLine (58, 0.0f, static_cast<float> (getWidth()));
    g.drawHorizontalLine (200, 0.0f, static_cast<float> (getWidth()));
    g.drawHorizontalLine (340, 0.0f, static_cast<float> (getWidth()));
    
    // 分区标题
    g.setColour (Colour (0xFF4ECDC4));
    g.setFont (Font (11.0f, Font::bold));
    g.drawFittedText ("CORE PARAMETERS", 15, 65, 150, 18, Justification::left, 1);
    
    g.drawFittedText ("FREQUENCY RANGE", 15, 210, 150, 18, Justification::left, 1);
    
    g.drawFittedText ("INPUT / OUTPUT", 15, 350, 150, 18, Justification::left, 1);
    
    // 频率范围区域 - 绘制处理范围
    int rangeY = 235;
    int rangeHeight = 100;
    int width = getWidth();
    
    g.setColour (Colour (0xFF2A3040));
    g.fillRoundedRectangle (juce::Rectangle<float> (20, rangeY, width - 40, rangeHeight), 8);
    
    // 获取参数并绘制处理范围
    auto params = processor.getParams();
    float lowPos = jmap (params.freqLow, 20.0f, 20000.0f, 0.0f, 1.0f);
    float highPos = jmap (params.freqHigh, 20.0f, 20000.0f, 0.0f, 1.0f);
    
    // 频率范围条
    g.setColour (Colour (0xFF4ECDC4).withAlpha (0.4f));
    int rangeStartX = 20 + static_cast<int> (lowPos * (width - 40));
    int rangeWidth = static_cast<int> ((highPos - lowPos) * (width - 40));
    g.fillRoundedRectangle (juce::Rectangle<float> (rangeStartX, rangeY + 25, rangeWidth, 50), 5);
    
    // 频率刻度
    g.setColour (Colours::grey);
    g.setFont (Font (9.0f));
    StringArray freqMarks = { "20", "100", "1k", "5k", "10k", "20k" };
    for (int i = 0; i < freqMarks.size(); ++i)
    {
        float pos = jmap (static_cast<float> (i), 0.0f, 5.0f, 0.0f, 1.0f);
        int x = 20 + static_cast<int> (pos * (width - 40));
        g.drawFittedText (freqMarks[i], x - 15, rangeY + rangeHeight - 18, 30, 15, Justification::centred, 1);
    }
    
    // 底部信息
    g.setColour (Colours::grey);
    g.setFont (Font (10.0f));
    g.drawFittedText ("VocalChain Series - Spectral Resonance Suppressor", 
                      20, getHeight() - 25, 350, 15, Justification::left, 1);
    
    // 版本信息
    g.drawFittedText ("FFT: 4096 | 75% Overlap", 
                      getWidth() - 160, getHeight() - 25, 150, 15, Justification::right, 1);
}

void VCSmoothEditor::resized ()
{
    int width = getWidth();
    int height = getHeight();
    juce::ignoreUnused (height);
    
    // 顶部栏
    int topY = 15;
    presetBox.setBounds (width - 150, topY, 100, 25);
    bypassBtn.setBounds (width - 60, topY, 50, 25);
    abBtn.setBounds (width - 120, topY, 50, 25);
    
    // ========== 核心参数区域 ==========
    int coreY = 90;
    int knobSize = 70;
    int spacing = 90;
    
    // Depth (0-1)
    depthSlider.setBounds (20, coreY, knobSize, knobSize);
    depthLabel.setBounds (20, coreY + knobSize + 2, knobSize, 18);
    
    // Speed (0.1-10)
    speedSlider.setBounds (20 + spacing, coreY, knobSize, knobSize);
    speedLabel.setBounds (20 + spacing, coreY + knobSize + 2, knobSize, 18);
    
    // Sharpness (0.1-5.0)
    sharpnessSlider.setBounds (20 + spacing * 2, coreY, knobSize, knobSize);
    sharpnessLabel.setBounds (20 + spacing * 2, coreY + knobSize + 2, knobSize, 18);
    
    // Mix (0-1)
    mixSlider.setBounds (20 + spacing * 3, coreY, knobSize, knobSize);
    mixLabel.setBounds (20 + spacing * 3, coreY + knobSize + 2, knobSize, 18);
    
    // ========== 频率范围区域 ==========
    int freqY = 115;
    int freqKnobSize = 60;
    
    // Freq Low
    freqLowSlider.setBounds (100, freqY, freqKnobSize, freqKnobSize);
    freqLowLabel.setBounds (90, freqY + freqKnobSize + 2, 80, 18);
    
    // Freq High
    freqHighSlider.setBounds (width - 160, freqY, freqKnobSize, freqKnobSize);
    freqHighLabel.setBounds (width - 170, freqY + freqKnobSize + 2, 80, 18);
    
    // ========== 输入输出区域 ==========
    int ioY = 370;
    int ioKnobSize = 60;
    
    inputSlider.setBounds (width / 2 - ioKnobSize - 40, ioY, ioKnobSize, ioKnobSize);
    inputLabel.setBounds (width / 2 - ioKnobSize - 40, ioY + ioKnobSize + 2, ioKnobSize, 18);
    
    outputSlider.setBounds (width / 2 + 40, ioY, ioKnobSize, ioKnobSize);
    outputLabel.setBounds (width / 2 + 40, ioY + ioKnobSize + 2, ioKnobSize, 18);
}
