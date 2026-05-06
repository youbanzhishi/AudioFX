#include "PluginEditor.h"

//==============================================================================
// Constants
//==============================================================================
constexpr int PLUGIN_WIDTH = 720;
constexpr int PLUGIN_HEIGHT = 480;

constexpr int METER_WIDTH = 24;
constexpr int METER_HEIGHT = 200;

//==============================================================================
// Helper to create labeled slider
//==============================================================================
static void setupSlider(juce::Slider& slider, juce::AudioProcessorValueTreeState& apvts,
                        const juce::String& paramID, const juce::String& label,
                        float min, float max, float defaultVal, float step)
{
    slider.setSliderStyle(juce::Slider::Rotary);
    slider.setRange(min, max, step);
    slider.setValue(defaultVal);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::cyan);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::grey);
}

//==============================================================================
// Constructor
//==============================================================================
VCCompAudioProcessorEditor::VCCompAudioProcessorEditor(VCCompAudioProcessor& p)
    : AudioProcessorEditor(p),
      processor(p)
{
    setSize(PLUGIN_WIDTH, PLUGIN_HEIGHT);
    startTimerHz(updateTimerHz);
    
    // ===== Title =====
    titleLabel.setText("VC-Comp", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);
    
    // ===== A/B Switch =====
    paramSetBox.addItem("A", 1);
    paramSetBox.addItem("B", 2);
    paramSetBox.setSelectedId(1);
    addAndMakeVisible(paramSetBox);
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, ParameterIDs::paramSet, paramSetBox));
    
    // ===== Bypass Button =====
    bypassButton.setButtonText("Bypass");
    bypassButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    bypassButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::red);
    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, ParameterIDs::bypass, bypassButton);
    
    // ===== Input/Output Meters =====
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(grMeter);
    addAndMakeVisible(outputMeter);
    
    // ===== Threshold Slider =====
    setupSlider(thresholdSlider, processor.apvts, ParameterIDs::threshold, "Threshold", -60.0f, 0.0f, 0.0f, 0.1f);
    thresholdSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::red);
    addAndMakeVisible(thresholdSlider);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, ParameterIDs::threshold, thresholdSlider));
    
    // ===== Ratio Slider =====
    setupSlider(ratioSlider, processor.apvts, ParameterIDs::ratio, "Ratio", 0.5f, 50.0f, 1.0f, 0.01f);
    ratioSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::orange);
    addAndMakeVisible(ratioSlider);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, ParameterIDs::ratio, ratioSlider));
    
    // ===== Attack Slider =====
    setupSlider(attackSlider, processor.apvts, ParameterIDs::attack, "Attack", 0.5f, 500.0f, 16.0f, 0.1f);
    attackSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::greenyellow);
    addAndMakeVisible(attackSlider);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, ParameterIDs::attack, attackSlider));
    
    // ===== Release Slider =====
    setupSlider(releaseSlider, processor.apvts, ParameterIDs::release, "Release", 5.0f, 5000.0f, 160.0f, 1.0f);
    releaseSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::cyan);
    addAndMakeVisible(releaseSlider);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, ParameterIDs::release, releaseSlider));
    
    // ===== Gain Slider =====
    setupSlider(gainSlider, processor.apvts, ParameterIDs::gain, "Gain", -30.0f, 30.0f, 0.0f, 0.1f);
    gainSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::green);
    addAndMakeVisible(gainSlider);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, ParameterIDs::gain, gainSlider));
    
    // ===== Mix Slider =====
    setupSlider(mixSlider, processor.apvts, ParameterIDs::mix, "Mix", 0.0f, 100.0f, 100.0f, 0.1f);
    mixSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::magenta);
    addAndMakeVisible(mixSlider);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, ParameterIDs::mix, mixSlider));
    
    // ===== Trim Slider =====
    setupSlider(trimSlider, processor.apvts, ParameterIDs::trim, "Trim", -18.0f, 18.0f, 0.0f, 0.1f);
    trimSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::purple);
    addAndMakeVisible(trimSlider);
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, ParameterIDs::trim, trimSlider));
    
    // ===== Release Mode =====
    releaseModeBox.addItemList(juce::StringArray{"ARC", "Manual"}, 1);
    releaseModeBox.setSelectedId(1);
    addAndMakeVisible(releaseModeBox);
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, ParameterIDs::releaseMode, releaseModeBox));
    
    // ===== Comp Behavior =====
    compBehaviorBox.addItemList(juce::StringArray{"Electro", "Opto"}, 1);
    compBehaviorBox.setSelectedId(1);
    addAndMakeVisible(compBehaviorBox);
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, ParameterIDs::compBehavior, compBehaviorBox));
    
    // ===== Knee Mode =====
    kneeModeBox.addItemList(juce::StringArray{"Hard", "Soft", "Auto"}, 1);
    kneeModeBox.setSelectedId(2);  // Default Soft
    addAndMakeVisible(kneeModeBox);
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, ParameterIDs::kneeMode, kneeModeBox));
    
    // ===== Character =====
    characterBox.addItemList(juce::StringArray{"Warm", "Smooth"}, 1);
    characterBox.setSelectedId(2);
    addAndMakeVisible(characterBox);
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, ParameterIDs::character, characterBox));
    
    // ===== Sidechain Source =====
    scSourceBox.addItemList(juce::StringArray{"Internal", "External"}, 1);
    scSourceBox.setSelectedId(1);
    addAndMakeVisible(scSourceBox);
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, ParameterIDs::scSource, scSourceBox));
    
    // ===== Sidechain HPF =====
    scHPFBox.addItemList(juce::StringArray{"Off", "60 Hz", "100 Hz", "200 Hz", "500 Hz"}, 1);
    scHPFBox.setSelectedId(1);
    addAndMakeVisible(scHPFBox);
    comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, ParameterIDs::scHPF, scHPFBox));
    
    // ===== SC Listen Button =====
    scListenButton.setButtonText("SC Listen");
    scListenButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    scListenButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::lime);
    addAndMakeVisible(scListenButton);
    scListenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, ParameterIDs::scListen, scListenButton);
    
    // ===== Limiter LEDs =====
    addAndMakeVisible(limiterYellow);
    addAndMakeVisible(limiterRed);
}

VCCompAudioProcessorEditor::~VCCompAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
// Paint
//==============================================================================
void VCCompAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1E1E2E));
    
    // Panel backgrounds
    g.setColour(juce::Colour(0xFF2A2A3E).withAlpha(0.8f));
    
    // Top bar
    g.fillRect(0, 0, PLUGIN_WIDTH, 40);
    
    // Middle section
    g.fillRect(0, 40, PLUGIN_WIDTH, 260);
    
    // Bottom section
    g.fillRect(0, 300, PLUGIN_WIDTH, PLUGIN_HEIGHT - 300);
    
    // Divider lines
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawHorizontalLine(40, 0, (float)PLUGIN_WIDTH);
    g.drawHorizontalLine(300, 0, (float)PLUGIN_WIDTH);
    
    // Labels
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.setFont(11.0f);
    
    // Meter labels
    g.drawText("INPUT", 20, 220, 60, 20, juce::Justification::centred);
    g.drawText("GR", 310, 220, 60, 20, juce::Justification::centred);
    g.drawText("OUTPUT", 620, 220, 60, 20, juce::Justification::centred);
    
    // Limiter labels
    g.drawText("LIMITER", 550, 85, 80, 20, juce::Justification::centred);
    
    // Section labels
    g.setColour(juce::Colours::cyan.withAlpha(0.7f));
    g.setFont(12.0f);
    g.drawText("COMPRESSION CONTROLS", 90, 310, 200, 20, juce::Justification::left);
    g.drawText("CHARACTER & MIX", 340, 310, 200, 20, juce::Justification::left);
    g.drawText("SIDECHAIN", 540, 310, 160, 20, juce::Justification::left);
    
    // Grid lines for meters
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    for (int i = 0; i < 5; ++i)
    {
        float y = 60.0f + i * 40.0f;
        g.drawHorizontalLine(y, 15, 45);
        g.drawHorizontalLine(y, 305, 335);
        g.drawHorizontalLine(y, 615, 645);
    }
    
    // dB labels
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(9.0f);
    juce::StringArray dbLabels = {"0", "-6", "-12", "-18", "-24", "-36"};
    for (int i = 0; i < dbLabels.size(); ++i)
    {
        float y = 60.0f + i * 28.0f;
        g.drawText(dbLabels[i], 0, (int)y - 5, 15, 10, juce::Justification::right);
    }
}

//==============================================================================
// Resized
//==============================================================================
void VCCompAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    
    // Top bar
    titleLabel.setBounds(10, 8, 100, 24);
    paramSetBox.setBounds(120, 8, 40, 24);
    bypassButton.setBounds(PLUGIN_WIDTH - 80, 8, 70, 24);
    
    // ===== Middle Section - Meters and Core Controls =====
    
    // Input Meter + Threshold
    inputMeter.setBounds(15, 55, METER_WIDTH, METER_HEIGHT);
    thresholdSlider.setBounds(5, 260, 50, 40);
    
    // GR Meter + Ratio
    grMeter.setBounds(305, 55, METER_WIDTH, METER_HEIGHT);
    ratioSlider.setBounds(295, 260, 50, 40);
    
    // Output Meter + Gain + Limiter LEDs
    outputMeter.setBounds(615, 55, METER_WIDTH, METER_HEIGHT);
    gainSlider.setBounds(605, 260, 50, 40);
    limiterYellow.setBounds(560, 70, 16, 16);
    limiterRed.setBounds(560, 95, 16, 16);
    
    // ===== Bottom Section - Detailed Controls =====
    
    int bottomY = 340;
    int knobY = 400;
    
    // Row 1: Attack | Release | Mode toggles
    attackSlider.setBounds(30, knobY, 50, 50);
    releaseSlider.setBounds(110, knobY, 50, 50);
    releaseModeBox.setBounds(180, bottomY, 60, 24);
    compBehaviorBox.setBounds(250, bottomY, 70, 24);
    kneeModeBox.setBounds(330, bottomY, 60, 24);  // Knee selector
    
    // Row 2: Character | Mix | Trim
    characterBox.setBounds(30, bottomY, 60, 24);
    mixSlider.setBounds(100, knobY, 50, 50);
    trimSlider.setBounds(180, knobY, 50, 50);
    
    // Row 3: Sidechain controls
    scSourceBox.setBounds(540, bottomY, 80, 24);
    scHPFBox.setBounds(630, bottomY, 80, 24);
    scListenButton.setBounds(540, knobY, 80, 30);
}

//==============================================================================
// Timer - Update Meters
//==============================================================================
void VCCompAudioProcessorEditor::timerCallback()
{
    // Update meters from processor
    float inL = processor.getInputLevelL();
    float inR = processor.getInputLevelR();
    float outL = processor.getOutputLevelL();
    float outR = processor.getOutputLevelR();
    float gr = processor.getGainReduction();
    
    // Average L/R for display
    inputMeter.setLevel((inL + inR) * 0.5f);
    grMeter.setGR(gr);
    outputMeter.setLevel((outL + outR) * 0.5f);
    
    // Limiter LEDs
    limiterYellow.setActive(processor.isLimiterYellow(), false);
    limiterRed.setActive(false, processor.isLimiterRed());
}
