/**
 * VC-PhaseScope - VST3 插件
 * 
 * 相位检测插件 - 带图形界面
 */

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "VC-PhaseScope.h"

class PhaseScopeProcessor : public juce::AudioProcessor {
public:
    PhaseScopeProcessor();
    ~PhaseScopeProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VC-PhaseScope"; }

    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // 公开相位数据供UI使用
    float getPhaseCorrelation() const { return currentCorrelation_; }
    audiofx::KickBassDetector::ConflictLevel getKickBassConflict() const { return kickBassConflict_; }

private:
    float currentCorrelation_ = 0.0f;
    audiofx::KickBassDetector kickBassDetector_;
    audiofx::KickBassDetector::ConflictLevel kickBassConflict_ = audiofx::KickBassDetector::ConflictLevel::None;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaseScopeProcessor)
};

//==============================================================================
class PhaseScopeEditor : public juce::AudioProcessorEditor,
                         public juce::Timer {
public:
    PhaseScopeEditor(PhaseScopeProcessor&);
    ~PhaseScopeEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    PhaseScopeProcessor& processor_;
    
    // UI组件
    juce::Label phaseCorrelationLabel_;
    juce::Label kickBassLabel_;
    juce::Label statusLabel_;
    
    // 绘制相位矢量显示
    void drawPhaseScope(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawCorrelationMeter(juce::Graphics& g, juce::Rectangle<int> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaseScopeEditor)
};

//==============================================================================
// 实现

PhaseScopeProcessor::PhaseScopeProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo())
                     .withOutput("Output", juce::AudioChannelSet::stereo()))
{
}

PhaseScopeProcessor::~PhaseScopeProcessor() {}

void PhaseScopeProcessor::prepareToPlay(double, int) {
    currentCorrelation_ = 0.0f;
    kickBassConflict_ = audiofx::KickBassDetector::ConflictLevel::None;
}

void PhaseScopeProcessor::releaseResources() {}

bool PhaseScopeProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    return layouts.getMainInputChannels() == 2 && 
           layouts.getMainOutputChannels() == 2;
}

void PhaseScopeProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    const float* left = buffer.getReadPointer(0);
    const float* right = buffer.getReadPointer(1);
    int samples = buffer.getNumSamples();

    // 计算相位相关度
    currentCorrelation_ = audiofx::PhaseCorrelation::compute(left, right, samples);

    // 检测底鼓/贝斯冲突
    kickBassConflict_ = kickBassDetector_.detect(left, right, getSampleRate(), samples);
}

//==============================================================================
PhaseScopeEditor::PhaseScopeEditor(PhaseScopeProcessor& p)
    : AudioProcessorEditor(p), processor_(p)
{
    setSize(400, 300);
    
    // 设置标签
    phaseCorrelationLabel_.setText("Phase: --", juce::dontSendNotification);
    phaseCorrelationLabel_.setFont(juce::Font(24.0f, juce::Font::bold));
    addAndMakeVisible(phaseCorrelationLabel_);
    
    kickBassLabel_.setText("Kick/Bass: --", juce::dontSendNotification);
    kickBassLabel_.setFont(juce::Font(18.0f));
    addAndMakeVisible(kickBassLabel_);
    
    statusLabel_.setText("Status: OK", juce::dontSendNotification);
    statusLabel_.setFont(juce::Font(14.0f));
    addAndMakeVisible(statusLabel_);
    
    startTimer(30);  // ~30fps更新
}

PhaseScopeEditor::~PhaseScopeEditor() {
    stopTimer();
}

void PhaseScopeEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::darkgrey);
    
    auto bounds = getLocalBounds();
    
    // 顶部：相位相关度仪表
    auto meterBounds = bounds.removeFromTop(60).reduced(10);
    drawCorrelationMeter(g, meterBounds);
    
    // 中间：相位矢量显示
    auto scopeBounds = bounds.removeFromTop(150).reduced(10);
    drawPhaseScope(g, scopeBounds);
    
    // 底部：状态信息
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText(statusLabel_.getText(), bounds, juce::Justification::centred);
}

void PhaseScopeEditor::resized() {
    auto bounds = getLocalBounds();
    
    phaseCorrelationLabel_.setBounds(bounds.removeFromTop(40).reduced(10));
    kickBassLabel_.setBounds(bounds.removeFromTop(30).reduced(10));
    statusLabel_.setBounds(bounds.removeFromBottom(30).reduced(10));
}

void PhaseScopeEditor::timerCallback() {
    float correlation = processor_.getPhaseCorrelation();
    auto conflict = processor_.getKickBassConflict();
    
    // 更新标签
    phaseCorrelationLabel_.setText(juce::String::formatted("Phase: %.2f", correlation), 
                                  juce::dontSendNotification);
    
    kickBassLabel_.setText(juce::String::formatted("Kick/Bass: %s", 
                                   audiofx::KickBassDetector::getLevelName(conflict)),
                                   juce::dontSendNotification);
    
    // 根据状态设置颜色
    if (conflict == audiofx::KickBassDetector::ConflictLevel::High) {
        statusLabel_.setText("⚠ HIGH CONFLICT - Check low frequencies!", juce::dontSendNotification);
        statusLabel_.setColour(juce::TextEditor::textColourId, juce::Colours::red);
    } else if (conflict == audiofx::KickBassDetector::ConflictLevel::Medium) {
        statusLabel_.setText("⚠ Medium conflict detected", juce::dontSendNotification);
        statusLabel_.setColour(juce::TextEditor::textColourId, juce::Colours::orange);
    } else {
        statusLabel_.setText("✓ Phase OK", juce::dontSendNotification);
        statusLabel_.setColour(juce::TextEditor::textColourId, juce::Colours::green);
    }
    
    repaint();
}

void PhaseScopeEditor::drawCorrelationMeter(juce::Graphics& g, juce::Rectangle<int> bounds) {
    float correlation = processor_.getPhaseCorrelation();
    
    // 背景
    g.setColour(juce::Colours::black);
    g.fillRect(bounds);
    
    // 绘制-1到+1的刻度
    g.setColour(juce::Colours::grey);
    for (int i = 0; i <= 10; ++i) {
        float x = bounds.getX() + bounds.getWidth() * i / 10.0f;
        g.drawLine(x, bounds.getY(), x, bounds.getBottom());
    }
    
    // 绘制中心线（0点）
    float centerX = bounds.getX() + bounds.getWidth() * 0.5f;
    g.setColour(juce::Colours::white);
    g.drawLine(centerX, bounds.getY(), centerX, bounds.getBottom(), 2.0f);
    
    // 绘制当前值指示器
    float indicatorX = bounds.getX() + (correlation + 1.0f) / 2.0f * bounds.getWidth();
    indicatorX = juce::jlimit(bounds.getX(), bounds.getRight(), indicatorX);
    
    // 颜色根据值变化
    if (correlation > 0.5f) {
        g.setColour(juce::Colours::green);
    } else if (correlation > 0.0f) {
        g.setColour(juce::Colours::yellow);
    } else if (correlation > -0.5f) {
        g.setColour(juce::Colours::orange);
    } else {
        g.setColour(juce::Colours::red);
    }
    
    g.fillRect(indicatorX - 3, bounds.getY(), 6, bounds.getHeight());
    
    // 刻度标签
    g.setColour(juce::Colours::white);
    g.setFont(10.0f);
    g.drawText("-1", bounds.getX(), bounds.getBottom() + 2, 20, 10, juce::Justification::left);
    g.drawText("0", centerX - 5, bounds.getBottom() + 2, 20, 10, juce::Justification::left);
    g.drawText("+1", bounds.getRight() - 20, bounds.getBottom() + 2, 20, 10, juce::Justification::left);
}

void PhaseScopeEditor::drawPhaseScope(juce::Graphics& g, juce::Rectangle<int> bounds) {
    // 圆形相位矢量显示
    float correlation = processor_.getPhaseCorrelation();
    
    float centerX = bounds.getCentreX();
    float centerY = bounds.getCentreY();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 10;
    
    // 外圆
    g.setColour(juce::Colours::darkgrey);
    g.fillEllipse(centerX - radius, centerY - radius, radius * 2, radius * 2);
    g.setColour(juce::Colours::grey);
    g.drawEllipse(centerX - radius, centerY - radius, radius * 2, radius * 2, 2.0f);
    
    // 中心十字
    g.setColour(juce::Colours::grey);
    g.drawLine(centerX - radius * 0.8f, centerY, centerX + radius * 0.8f, centerY);
    g.drawLine(centerX, centerY - radius * 0.8f, centerX, centerY + radius * 0.8f);
    
    // 绘制矢量点
    // 角度基于相关度：+1在顶部，-1在底部
    float angle = (1.0f - correlation) * juce::MathConstants<float>::pi;  // 映射到角度
    float vectorLength = radius * 0.6f;
    float dotX = centerX + std::sin(angle) * vectorLength;
    float dotY = centerY - std::cos(angle) * vectorLength;
    
    // 矢量颜色
    if (correlation > 0.5f) {
        g.setColour(juce::Colours::green);
    } else if (correlation > 0.0f) {
        g.setColour(juce::Colours::yellow);
    } else if (correlation > -0.5f) {
        g.setColour(juce::Colours::orange);
    } else {
        g.setColour(juce::Colours::red);
    }
    
    g.fillEllipse(dotX - 8, dotY - 8, 16, 16);
    
    // 标签
    g.setColour(juce::Colours::white);
    g.setFont(10.0f);
    g.drawText("M", centerX - 5, centerY + radius + 2, 20, 10, juce::Justification::centred);
    g.drawText("S", centerX + radius + 5, centerY - 5, 20, 10, juce::Justification::left);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new PhaseScopeProcessor();
}
