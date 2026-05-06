#pragma once

// JUCE 模块头文件
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
// EQ频段配置
namespace EQBands
{
    enum BandIndex
    {
        LowShelf = 0,
        LowMid,
        Mid,
        HighMid,
        HighShelf
    };
    
    static const int kNumBands = 5;
    
    // 默认频率(Hz)
    static const float kDefaultFrequencies[kNumBands] = { 80.0f, 300.0f, 1000.0f, 3000.0f, 8000.0f };
    // 默认Q值
    static const float kDefaultQ[kNumBands] = { 0.707f, 1.0f, 1.0f, 1.0f, 0.707f };
    // 默认增益(dB)
    static const float kDefaultGains[kNumBands] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
}

//==============================================================================
// VC-EQ参数ID
namespace ParameterIDs
{
    // 输入输出
    static const juce::String inputGain = "input_gain";
    static const juce::String outputGain = "output_gain";
    static const juce::String bypass = "bypass";
    static const juce::String abSwitch = "ab_switch";
    
    // 5个频段通用参数
    static const juce::String freqSuffix = "_freq";
    static const juce::String qSuffix = "_q";
    static const juce::String gainSuffix = "_gain";
    static const juce::String typeSuffix = "_type";
    static const juce::String enabledSuffix = "_enabled";
    static const juce::String soloSuffix = "_solo";
    
    // 预设
    static const juce::String preset = "preset";
    
    static juce::String getBandParam(int band, const juce::String& suffix)
    {
        return "band" + juce::String(band) + suffix;
    }
}

//==============================================================================
class VCEQProcessor : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    VCEQProcessor();
    ~VCEQProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "VC-EQ"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    //==============================================================================
    // 参数监听
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    
    // 获取AudioProcessorValueTreeState
    juce::AudioProcessorValueTreeState& getAPVTS() { return mAPVTS; }
    
    // 频段类型
    enum FilterType { LowShelf, HighShelf, Parametric, LowPass, HighPass };
    
private:
    //==============================================================================
    // 创建参数布局
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // 更新滤波器系数
    void updateFilterCoefficients(int band);
    void updateAllFilters();
    
    // 频段Solo处理
    void handleSoloChange();
    
    //==============================================================================
    juce::AudioProcessorValueTreeState mAPVTS;
    
    // DSP处理模块
    juce::dsp::ProcessSpec mProcessSpec;
    
    // 5个频段的IIR滤波器
    struct BandFilter
    {
        juce::dsp::IIR::Filter<float> filter;
        bool enabled = true;
        bool solo = false;
        FilterType type = Parametric;
        float frequency = 1000.0f;
        float q = 1.0f;
        float gainDB = 0.0f;
    };
    
    BandFilter mBands[EQBands::kNumBands];
    
    // 输入输出增益
    float mInputGain = 1.0f;
    float mOutputGain = 1.0f;
    
    // 缓存的系数（用于检测变化）
    struct CachedCoefficients
    {
        float freq = 0.0f;
        float q = 0.0f;
        float gainDB = 0.0f;
        bool enabled = true;
    };
    CachedCoefficients mCachedParams[EQBands::kNumBands];
    
    // 是否处于Bypass状态
    bool mBypass = false;
    
    // A/B切换状态
    int mABState = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCEQProcessor)
};
