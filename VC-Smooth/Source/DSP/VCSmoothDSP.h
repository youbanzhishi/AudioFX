#pragma once

#ifdef VC_STANDALONE
// Standalone mode: use standard library math, no JUCE dependency
#include <vector>
#include <cmath>
#include <algorithm>

// Standalone constants
constexpr float VC_PI = 3.14159265358979323846f;

#define VC_DECLARE_NON_COPYABLE(x) // No-op in standalone
#define VC_JMIN(a, b) std::min(a, b)
#define VC_JMAX(a, b) std::max(a, b)
#define VC_JLIMIT(lo, hi, x) std::clamp(x, lo, hi)
#else
// JUCE mode
#include <juce_dsp/juce_dsp.h>
#define VC_DECLARE_NON_COPYABLE(x) JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(x)
#define VC_JMIN(a, b) juce::jmin(a, b)
#define VC_JMAX(a, b) juce::jmax(a, b)
#define VC_JLIMIT(lo, hi, x) juce::jlimit(lo, hi, x)
#endif

//==============================================================================
// FFT 处理配置常量
namespace VCSmoothConfig
{
    static constexpr int kFFTSize = 4096;      // 帧长 (~93ms @ 44100Hz)
    static constexpr int kHopSize = 1024;      // 75% overlap
    static constexpr int kHalfFFTSize = kFFTSize / 2;
    static constexpr int kLatency = kFFTSize / 2;
}

//==============================================================================
// VC-Smooth DSP 核心类
class VCSmoothDSP
{
public:
    //==============================================================================
    // 处理参数结构
    struct Params
    {
        float depth = 0.5f;          // 0-1, 共振峰压缩深度
        float speed = 2.0f;          // 0.1-10, 包络跟踪速度
        float freqLow = 200.0f;      // Hz, 低频边界
        float freqHigh = 16000.0f;   // Hz, 高频边界
        float sharpness = 1.5f;      // 0.1-5.0, 锐度(阈值系数)
        float mix = 1.0f;           // 0-1, 干湿比
        float inputGain = 0.0f;      // dB, 输入增益
        float outputGain = 0.0f;     // dB, 输出增益
    };
    
    //==============================================================================
    VCSmoothDSP();
    ~VCSmoothDSP();
    
    //==============================================================================
    // 准备处理
    void prepare(double sampleRate, int blockSize);
    
    //==============================================================================
    // 处理立体声浮点缓冲区
    void process(float* left, float* right, int numSamples);
    
    //==============================================================================
    // 参数设置
    void setParams(const Params& p);
    Params getParams() const;
    
    //==============================================================================
    // 重置DSP状态
    void reset();
    
    //==============================================================================
    // 获取延迟样本数
    int getLatencySamples() const { return VCSmoothConfig::kLatency; }
    
    //==============================================================================
    // 获取采样率
    double getSampleRate() const { return mSampleRate; }

private:
    //==============================================================================
    // 频谱处理核心
    void spectralProcessing(float* leftChannel, float* rightChannel, int numSamples);
    void processFFTFrame(float* leftFrame, float* rightFrame);
    
    //==============================================================================
    // 工具函数
    inline int freqToBin(float hz) const;
    inline float dBToLinear(float dB) const;
    
    //==============================================================================
    // DSP 状态
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    
    // 处理参数
    Params mParams;
    
    // 频率范围 bin
    int mFreqLowBin = 0;
    int mFreqHighBin = VCSmoothConfig::kHalfFFTSize - 1;
    
    //==============================================================================
    // FFT 缓冲区和状态
    // 输入缓冲（用于 overlap-add）
    std::vector<float> mInputBufferL;
    std::vector<float> mInputBufferR;
    
    // 输出缓冲（用于 overlap-add）
    std::vector<float> mOutputBufferL;
    std::vector<float> mOutputBufferR;
    
    // FFT 工作区
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
    
    //==============================================================================
    // FFT 对象
#ifdef VC_STANDALONE
    // Standalone FFT implementation
    void fftPerform(bool forward, float* real, float* imag, int size);
#else
    juce::dsp::FFT mFFT;
#endif
    
    //==============================================================================
    VC_DECLARE_NON_COPYABLE(VCSmoothDSP)
};
