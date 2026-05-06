#pragma once

// JUCE 模块头文件
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

//==============================================================================
// VC-Smooth 参数 ID
namespace ParameterIDs
{
    // 核心参数
    static const juce::String depth{"depth"};
    static const juce::String speed{"speed"};
    static const juce::String freqLow{"freqLow"};
    static const juce::String freqHigh{"freqHigh"};
    static const juce::String sharpness{"sharpness"};
    static const juce::String mix{"mix"};
    
    // 辅助参数
    static const juce::String inputGain{"inputGain"};
    static const juce::String outputGain{"outputGain"};
    static const juce::String bypass{"bypass"};
    static const juce::String paramSet{"paramSet"};
}

//==============================================================================
// FFT 处理配置
namespace FFTConfig
{
    static constexpr int kFFTSize = 4096;      // 帧长 (~93ms @ 44100Hz)
    static constexpr int kHopSize = 1024;      // 75% overlap
    static constexpr int kLatency = kFFTSize / 2;
}

//==============================================================================
class VCSmoothProcessor : public juce::AudioProcessor, 
                          public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    VCSmoothProcessor();
    ~VCSmoothProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "VC-Smooth"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override { return static_cast<double>(FFTConfig::kLatency) / 44100.0; }

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
    
    //==============================================================================
    // 获取 AudioProcessorValueTreeState
    juce::AudioProcessorValueTreeState& getAPVTS() { return mAPVTS; }
    
    //==============================================================================
    // 处理参数结构
    struct ProcessParams
    {
        float depth = 0.5f;          // 0-1
        float speed = 2.0f;          // 0.1-10
        float freqLow = 200.0f;     // Hz
        float freqHigh = 16000.0f;  // Hz
        float sharpness = 1.5f;     // 0.1-5.0
        float mix = 1.0f;           // 0-1
        float inputGain = 0.0f;     // dB
        float outputGain = 0.0f;    // dB
    };
    
    ProcessParams getParams() const { return mParams; }

private:
    //==============================================================================
    // 创建参数布局
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    //==============================================================================
    // 频谱处理核心
    void spectralProcessing(float* leftChannel, float* rightChannel, int numSamples);
    void processFFTFrame(float* leftFrame, float* rightFrame);
    
    // 工具函数
    inline float hzToBin(float hz) const;
    inline int freqToBin(float hz, double sampleRate) const;
    
    //==============================================================================
    juce::AudioProcessorValueTreeState mAPVTS;
    
    // DSP 处理规格
    juce::dsp::ProcessSpec mProcessSpec;
    
    // 处理参数
    ProcessParams mParams;
    
    // Bypass 状态
    bool mBypass = false;
    
    // A/B 参数组
    int mParamSet = 0;
    
    //==============================================================================
    // FFT 缓冲区和状态
    static constexpr int kFFTSize = FFTConfig::kFFTSize;
    static constexpr int kHopSize = FFTConfig::kHopSize;
    static constexpr int kHalfFFTSize = kFFTSize / 2;
    
    // 输入缓冲（用于 overlap-add）
    std::vector<float> mInputBufferL;
    std::vector<float> mInputBufferR;
    
    // 输出缓冲（用于 overlap-add）
    std::vector<float> mOutputBufferL;
    std::vector<float> mOutputBufferR;
    
    // FFT 工作区 (实部和虚部分开存储)
    std::vector<float> mFFTReal;
    std::vector<float> mFFTImag;
    
    // Hann 窗
    std::vector<float> mWindow;
    
    // 频谱包络 (用于平滑)
    std::vector<float> mAvgSpectrumL;
    std::vector<float> mAvgSpectrumR;
    
    // 增益数据 (时域平滑)
    std::vector<float> mGainSmoothL;
    std::vector<float> mGainSmoothR;
    
    // 处理位置指针
    int mBufferPos = 0;
    
    // FFT 对象
    juce::dsp::FFT mFFT;
    
    // 频率范围 bin
    int mFreqLowBin = 0;
    int mFreqHighBin = kHalfFFTSize - 1;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCSmoothProcessor)
};
