#pragma once

// ============================================================
// JUCE 模块头文件
// ============================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

// ============================================================
// TODO: 定义插件特定参数 ID
// namespace ParameterIDs
// {
//     // 示例参数
//     static const juce::String bypass = "bypass";
//     static const juce::String gain = "gain";
//     static const juce::String mix = "mix";
// }
// ============================================================

// ============================================================
// TODO: 可选 - 定义处理配置常量
// namespace Config
// {
//     static const int kFFTSize = 4096;
//     static const int kHopSize = 1024;
// }
// ============================================================

// ============================================================
// 主处理器类
// ============================================================
class [[PLUGIN_NAME]]Processor : public juce::AudioProcessor, 
                                  public juce::AudioProcessorValueTreeState::Listener
{
public:
    // ============================================================
    // 构造函数和析构函数
    // ============================================================
    [[PLUGIN_NAME]]Processor();
    ~[[PLUGIN_NAME]]Processor();

    // ============================================================
    // JUCE AudioProcessor 接口
    // ============================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) override;

    // ============================================================
    // 编辑器
    // ============================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // ============================================================
    // 插件信息
    // ============================================================
    const juce::String getName() const override { return "[[PLUGIN_NAME]]"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // ============================================================
    // 程序（预设）支持
    // ============================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // ============================================================
    // 状态保存/恢复
    // ============================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    // ============================================================
    // 参数监听回调
    // ============================================================
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    
    // ============================================================
    // 获取 AudioProcessorValueTreeState
    // ============================================================
    juce::AudioProcessorValueTreeState& getAPVTS() { return mAPVTS; }

    // ============================================================
    // TODO: 定义处理参数结构体（可选）
    // struct ProcessParams
    // {
    //     float gain = 0.0f;
    //     float mix = 100.0f;
    // };
    // ============================================================

private:
    // ============================================================
    // 创建参数布局
    // ============================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // ============================================================
    // TODO: 实现 DSP 处理方法
    // void processDSP(juce::AudioBuffer<float>& buffer);
    // ============================================================
    
    // ============================================================
    // DSP 处理规格
    // ============================================================
    juce::dsp::ProcessSpec mProcessSpec;
    
    // ============================================================
    // TODO: 添加 DSP 处理相关成员变量
    // 示例: 
    // - FFT 缓冲区
    // - 滤波器状态
    // - 包络 follower
    // ============================================================
    
    // ============================================================
    // 参数状态
    // ============================================================
    bool mBypass = false;
    
    // ============================================================
    // TODO: 添加处理参数
    // ProcessParams mParams;
    // ============================================================
    
    // ============================================================
    // AudioProcessorValueTreeState
    // ============================================================
    juce::AudioProcessorValueTreeState mAPVTS;
    
    // ============================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR ([[PLUGIN_NAME]]Processor)
};
