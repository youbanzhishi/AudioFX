#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// 工具函数
//==============================================================================
static inline float dBToLinear(float dB) { return std::pow(10.0f, dB / 20.0f); }
static inline float linearToDb(float linear) { 
    return 20.0f * std::log10(std::max(linear, 1e-10f)); 
}

//==============================================================================
// 构造函数
//==============================================================================
VCSmoothProcessor::VCSmoothProcessor()
    : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  AudioChannelSet::stereo())
                       .withOutput ("Output", AudioChannelSet::stereo()))
    , mAPVTS (*this, nullptr, Identifier ("VCSmoothParameters"), createParameterLayout())
    , mFFT (int(std::log2(kFFTSize)))
{
    // 注册参数监听器
    mAPVTS.addParameterListener (ParameterIDs::bypass, this);
    mAPVTS.addParameterListener (ParameterIDs::depth, this);
    mAPVTS.addParameterListener (ParameterIDs::speed, this);
    mAPVTS.addParameterListener (ParameterIDs::freqLow, this);
    mAPVTS.addParameterListener (ParameterIDs::freqHigh, this);
    mAPVTS.addParameterListener (ParameterIDs::sharpness, this);
    mAPVTS.addParameterListener (ParameterIDs::mix, this);
    mAPVTS.addParameterListener (ParameterIDs::inputGain, this);
    mAPVTS.addParameterListener (ParameterIDs::outputGain, this);
    
    // 初始化缓冲区 (2帧 + 重叠)
    int bufferSize = kFFTSize * 2;
    mInputBufferL.resize(bufferSize, 0.0f);
    mInputBufferR.resize(bufferSize, 0.0f);
    mOutputBufferL.resize(bufferSize, 0.0f);
    mOutputBufferR.resize(bufferSize, 0.0f);
    
    // FFT 工作区
    mFFTReal.resize(kFFTSize * 2, 0.0f);
    mFFTImag.resize(kFFTSize * 2, 0.0f);
    
    // 创建 Hann 窗
    mWindow.resize(kFFTSize);
    for (int i = 0; i < kFFTSize; ++i)
    {
        mWindow[i] = 0.5f * (1.0f - std::cos(2.0f * MathConstants<float>::pi * i / (kFFTSize - 1)));
    }
    
    // 初始化频谱和增益数组
    mAvgSpectrumL.resize(kHalfFFTSize, 0.0f);
    mAvgSpectrumR.resize(kHalfFFTSize, 0.0f);
    mGainSmoothL.resize(kHalfFFTSize, 1.0f);
    mGainSmoothR.resize(kHalfFFTSize, 1.0f);
}

VCSmoothProcessor::~VCSmoothProcessor()
{
}

//==============================================================================
// 参数布局
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout VCSmoothProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    
    // ========== 核心参数 ==========
    
    // Depth (0-1, default 0.5)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::depth, "Depth",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    
    // Speed (0.1-10, default 2.0)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::speed, "Speed",
        NormalisableRange<float> (0.1f, 10.0f, 0.1f), 2.0f));
    
    // Freq Low (20-20000 Hz, default 200Hz)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::freqLow, "Freq Low",
        NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f), 200.0f,
        "Hz"));
    
    // Freq High (20-20000 Hz, default 16000Hz)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::freqHigh, "Freq High",
        NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f), 16000.0f,
        "Hz"));
    
    // Sharpness (0.1-5.0, default 1.5)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::sharpness, "Sharpness",
        NormalisableRange<float> (0.1f, 5.0f, 0.1f), 1.5f));
    
    // Mix (0-1, default 1.0)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::mix, "Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
    
    // ========== 辅助参数 ==========
    
    // Input Gain (-24 to +24 dB)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::inputGain, "Input",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        "dB"));
    
    // Output Gain (-24 to +24 dB)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::outputGain, "Output",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        "dB"));
    
    // Bypass
    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterIDs::bypass, "Bypass", false));
    
    // A/B 参数组切换
    params.push_back (std::make_unique<AudioParameterInt> (
        ParameterIDs::paramSet, "A/B", 0, 1, 0));
    
    return { params.begin(), params.end() };
}

//==============================================================================
// 准备播放
//==============================================================================
void VCSmoothProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = samplesPerBlock;
    mProcessSpec.numChannels = getMainBusNumOutputChannels();
    
    // 重置缓冲区
    std::fill(mInputBufferL.begin(), mInputBufferL.end(), 0.0f);
    std::fill(mInputBufferR.begin(), mInputBufferR.end(), 0.0f);
    std::fill(mOutputBufferL.begin(), mOutputBufferL.end(), 0.0f);
    std::fill(mOutputBufferR.begin(), mOutputBufferR.end(), 0.0f);
    
    // 重置频谱
    std::fill(mAvgSpectrumL.begin(), mAvgSpectrumL.end(), 0.0f);
    std::fill(mAvgSpectrumR.begin(), mAvgSpectrumR.end(), 0.0f);
    std::fill(mGainSmoothL.begin(), mGainSmoothL.end(), 1.0f);
    std::fill(mGainSmoothR.begin(), mGainSmoothR.end(), 1.0f);
    
    // 重置处理位置
    mBufferPos = 0;
    
    // 更新频率范围 bin
    mFreqLowBin = freqToBin(mParams.freqLow, sampleRate);
    mFreqHighBin = freqToBin(mParams.freqHigh, sampleRate);
    mFreqHighBin = jmin(mFreqHighBin, kHalfFFTSize - 1);
    
    // 设置延迟
    setLatencySamples(FFTConfig::kLatency);
}

void VCSmoothProcessor::releaseResources()
{
    std::fill(mInputBufferL.begin(), mInputBufferL.end(), 0.0f);
    std::fill(mInputBufferR.begin(), mInputBufferR.end(), 0.0f);
    std::fill(mOutputBufferL.begin(), mOutputBufferL.end(), 0.0f);
    std::fill(mOutputBufferR.begin(), mOutputBufferR.end(), 0.0f);
}

//==============================================================================
// 总线布局检查
//==============================================================================
bool VCSmoothProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

//==============================================================================
// 工具函数实现
//==============================================================================
inline int VCSmoothProcessor::freqToBin(float hz, double sampleRate) const
{
    if (hz <= 0.0f)
        return 0;
    return static_cast<int>(hz * kFFTSize / sampleRate);
}

inline float VCSmoothProcessor::hzToBin(float hz) const
{
    return static_cast<float>(hz * kFFTSize / mProcessSpec.sampleRate);
}

//==============================================================================
// 主处理块
//==============================================================================
void VCSmoothProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    if (mBypass)
        return;
    
    int numSamples = buffer.getNumSamples();
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getWritePointer(1);
    
    // 应用输入增益
    float inputGainLinear = dBToLinear(mParams.inputGain);
    for (int i = 0; i < numSamples; ++i)
    {
        leftChannel[i] *= inputGainLinear;
        rightChannel[i] *= inputGainLinear;
    }
    
    // 如果 Depth 为 0，跳过频谱处理（直接输出干信号）
    if (mParams.depth > 0.001f)
    {
        // 进行频谱处理
        spectralProcessing(leftChannel, rightChannel, numSamples);
    }
    
    // 应用输出增益
    float outputGainLinear = dBToLinear(mParams.outputGain);
    for (int i = 0; i < numSamples; ++i)
    {
        leftChannel[i] *= outputGainLinear;
        rightChannel[i] *= outputGainLinear;
    }
}

//==============================================================================
// 频谱处理核心
//==============================================================================
void VCSmoothProcessor::spectralProcessing(float* leftChannel, float* rightChannel, int numSamples)
{
    // 临时帧缓冲区
    static thread_local std::vector<float> frameL(kFFTSize, 0.0f);
    static thread_local std::vector<float> frameR(kFFTSize, 0.0f);
    static thread_local std::vector<float> outFrameL(kFFTSize, 0.0f);
    static thread_local std::vector<float> outFrameR(kFFTSize, 0.0f);
    
    // 处理每个 hop
    int processedSamples = 0;
    
    while (processedSamples < numSamples)
    {
        // 检查是否有一个完整的帧需要处理
        int samplesNeeded = kFFTSize - mBufferPos;
        int samplesAvailable = numSamples - processedSamples;
        int samplesToCopy = jmin(samplesNeeded, samplesAvailable);
        
        // 复制输入样本到缓冲区
        for (int i = 0; i < samplesToCopy; ++i)
        {
            mInputBufferL[mBufferPos + i] = leftChannel[processedSamples + i];
            mInputBufferR[mBufferPos + i] = rightChannel[processedSamples + i];
        }
        
        mBufferPos += samplesToCopy;
        processedSamples += samplesToCopy;
        
        // 如果有一帧准备好了
        if (mBufferPos >= kFFTSize)
        {
            // 提取帧并加窗
            for (int i = 0; i < kFFTSize; ++i)
            {
                frameL[i] = mInputBufferL[i] * mWindow[i];
                frameR[i] = mInputBufferR[i] * mWindow[i];
            }
            
            // 处理 FFT 帧
            processFFTFrame(frameL.data(), frameR.data());
            
            // 将处理后的帧叠加到输出
            for (int i = 0; i < kFFTSize; ++i)
            {
                outFrameL[i] = frameL[i] * mWindow[i];
                outFrameR[i] = frameR[i] * mWindow[i];
            }
            
            // Overlap-add: 将输出帧添加到输出缓冲区
            for (int i = 0; i < kFFTSize; ++i)
            {
                mOutputBufferL[i] += outFrameL[i];
                mOutputBufferR[i] += outFrameR[i];
            }
            
            // 将输出缓冲区的样本写入输出
            int outputSamples = jmin(kHopSize, numSamples);
            for (int i = 0; i < outputSamples; ++i)
            {
                leftChannel[i] = mOutputBufferL[i];
                rightChannel[i] = mOutputBufferR[i];
            }
            
            // 移动输出缓冲区 (向左移动一个 hop)
            for (int i = 0; i < kFFTSize - kHopSize; ++i)
            {
                mOutputBufferL[i] = mOutputBufferL[i + kHopSize];
                mOutputBufferR[i] = mOutputBufferR[i + kHopSize];
            }
            for (int i = kFFTSize - kHopSize; i < kFFTSize; ++i)
            {
                mOutputBufferL[i] = 0.0f;
                mOutputBufferR[i] = 0.0f;
            }
            
            // 移动输入缓冲区 (向左移动一个 hop)
            for (int i = 0; i < kFFTSize; ++i)
            {
                mInputBufferL[i] = mInputBufferL[i + kHopSize];
                mInputBufferR[i] = mInputBufferR[i + kHopSize];
            }
            for (int i = kFFTSize; i < kFFTSize * 2; ++i)
            {
                mInputBufferL[i] = 0.0f;
                mInputBufferR[i] = 0.0f;
            }
            
            mBufferPos -= kHopSize;
        }
    }
}

//==============================================================================
// FFT 帧处理
//==============================================================================
void VCSmoothProcessor::processFFTFrame(float* leftFrame, float* rightFrame)
{
    // ========== 1. FFT 正变换 ==========
    // 准备 FFT 输入 (交替实部虚部)
    for (int i = 0; i < kFFTSize; ++i)
    {
        mFFTReal[i] = leftFrame[i];
        mFFTImag[i] = 0.0f;
    }
    
    // 执行 FFT
    mFFT.performRealOnlyForwardTransform(mFFTReal.data(), true);
    
    // 获取左声道幅度谱
    std::vector<float> magL(kHalfFFTSize, 0.0f);
    for (int i = 0; i < kHalfFFTSize; ++i)
    {
        float real = mFFTReal[i * 2];
        float imag = mFFTReal[i * 2 + 1];
        magL[i] = std::sqrt(real * real + imag * imag) + 1e-10f;
    }
    
    // 准备右声道 FFT 输入
    for (int i = 0; i < kFFTSize; ++i)
    {
        mFFTReal[i] = rightFrame[i];
        mFFTImag[i] = 0.0f;
    }
    mFFT.performRealOnlyForwardTransform(mFFTReal.data(), true);
    
    // 获取右声道幅度谱
    std::vector<float> magR(kHalfFFTSize, 0.0f);
    for (int i = 0; i < kHalfFFTSize; ++i)
    {
        float real = mFFTReal[i * 2];
        float imag = mFFTReal[i * 2 + 1];
        magR[i] = std::sqrt(real * real + imag * imag) + 1e-10f;
    }
    
    // ========== 2. 频谱峰值检测与增益计算 ==========
    // 计算包络跟踪系数 (Speed 参数控制)
    float attackCoef = std::exp(-1.0f / (mParams.speed * 0.1f * mProcessSpec.sampleRate / 512.0f));
    float releaseCoef = std::exp(-1.0f / (mParams.speed * 1.0f * mProcessSpec.sampleRate / 512.0f));
    float attackCoefInv = 1.0f - attackCoef;
    float releaseCoefInv = 1.0f - releaseCoef;
    
    // 增益平滑系数
    float gainSmoothingCoef = std::exp(-1.0f / (mProcessSpec.sampleRate * 0.005f)); // 5ms 平滑
    
    // 临时增益
    std::vector<float> targetGainL(kHalfFFTSize, 1.0f);
    std::vector<float> targetGainR(kHalfFFTSize, 1.0f);
    
    // 计算阈值
    float threshold = 1.0f + mParams.sharpness;
    
    // 处理频率范围内的 bins
    for (int i = mFreqLowBin; i <= mFreqHighBin && i < kHalfFFTSize; ++i)
    {
        // 更新平均谱 (指数移动平均)
        if (magL[i] > mAvgSpectrumL[i])
            mAvgSpectrumL[i] = attackCoefInv * magL[i] + attackCoef * mAvgSpectrumL[i];
        else
            mAvgSpectrumL[i] = releaseCoefInv * magL[i] + releaseCoef * mAvgSpectrumL[i];
        
        if (magR[i] > mAvgSpectrumR[i])
            mAvgSpectrumR[i] = attackCoefInv * magR[i] + releaseCoef * mAvgSpectrumR[i];
        else
            mAvgSpectrumR[i] = releaseCoefInv * magR[i] + releaseCoef * mAvgSpectrumR[i];
        
        // 检测共振峰: 当前幅度 vs 平均线
        float ratioL = magL[i] / mAvgSpectrumL[i];
        float ratioR = magR[i] / mAvgSpectrumR[i];
        
        // 计算增益衰减
        if (ratioL > threshold)
        {
            // 超标量 (归一化到最大超标量)
            float excess = (ratioL - threshold) / (mParams.sharpness * 2.0f);
            excess = jlimit(0.0f, 1.0f, excess);
            
            // 增益 = 1 - Depth × 超标量
            targetGainL[i] = 1.0f - mParams.depth * excess;
            targetGainL[i] = jlimit(0.01f, 1.0f, targetGainL[i]);
        }
        
        if (ratioR > threshold)
        {
            float excess = (ratioR - threshold) / (mParams.sharpness * 2.0f);
            excess = jlimit(0.0f, 1.0f, excess);
            targetGainR[i] = 1.0f - mParams.depth * excess;
            targetGainR[i] = jlimit(0.01f, 1.0f, targetGainR[i]);
        }
    }
    
    // 时域平滑增益
    float smoothingCoefInv = 1.0f - gainSmoothingCoef;
    for (int i = 0; i < kHalfFFTSize; ++i)
    {
        if (targetGainL[i] < mGainSmoothL[i])
            mGainSmoothL[i] = smoothingCoefInv * targetGainL[i] + gainSmoothingCoef * mGainSmoothL[i];
        else
            mGainSmoothL[i] = targetGainL[i];
        
        if (targetGainR[i] < mGainSmoothR[i])
            mGainSmoothR[i] = smoothingCoefInv * targetGainR[i] + gainSmoothingCoef * mGainSmoothR[i];
        else
            mGainSmoothR[i] = targetGainR[i];
    }
    
    // ========== 3. 应用频域增益 ==========
    // 左声道
    for (int i = 0; i < kHalfFFTSize; ++i)
    {
        int idx = i * 2;
        float real = mFFTReal[idx];
        float imag = mFFTReal[idx + 1];
        float mag = std::sqrt(real * real + imag * imag) + 1e-10f;
        float phase = std::atan2(imag, real);
        
        // 应用平滑增益
        float newMag = mag * mGainSmoothL[i];
        mFFTReal[idx] = newMag * std::cos(phase);
        mFFTReal[idx + 1] = newMag * std::sin(phase);
    }
    
    // ========== 4. IFFT ==========
    // 对于实数输入，需要先填充共轭对称
    for (int i = kHalfFFTSize; i < kFFTSize; ++i)
    {
        int mirrorIdx = (kFFTSize - i) * 2;
        mFFTReal[i * 2] = mFFTReal[mirrorIdx];
        mFFTReal[i * 2 + 1] = -mFFTReal[mirrorIdx + 1];
    }
    
    mFFT.performRealOnlyInverseTransform(mFFTReal.data());
    
    // 复制左声道结果
    for (int i = 0; i < kFFTSize; ++i)
    {
        leftFrame[i] = mFFTReal[i] / kFFTSize; // 归一化
    }
    
    // 右声道处理
    for (int i = 0; i < kHalfFFTSize; ++i)
    {
        int idx = i * 2;
        float real = mFFTReal[idx];
        float imag = mFFTReal[idx + 1];
        float mag = std::sqrt(real * real + imag * imag) + 1e-10f;
        float phase = std::atan2(imag, real);
        
        // 应用平滑增益
        float newMag = mag * mGainSmoothR[i];
        mFFTReal[idx] = newMag * std::cos(phase);
        mFFTReal[idx + 1] = newMag * std::sin(phase);
    }
    
    // 填充共轭对称
    for (int i = kHalfFFTSize; i < kFFTSize; ++i)
    {
        int mirrorIdx = (kFFTSize - i) * 2;
        mFFTReal[i * 2] = mFFTReal[mirrorIdx];
        mFFTReal[i * 2 + 1] = -mFFTReal[mirrorIdx + 1];
    }
    
    mFFT.performRealOnlyInverseTransform(mFFTReal.data());
    
    // 复制右声道结果
    for (int i = 0; i < kFFTSize; ++i)
    {
        rightFrame[i] = mFFTReal[i] / kFFTSize; // 归一化
    }
}

//==============================================================================
// 参数变化回调
//==============================================================================
void VCSmoothProcessor::parameterChanged (const String& parameterID, float newValue)
{
    if (parameterID == ParameterIDs::bypass)
    {
        mBypass = newValue > 0.5f;
    }
    else if (parameterID == ParameterIDs::depth)
    {
        mParams.depth = newValue;
    }
    else if (parameterID == ParameterIDs::speed)
    {
        mParams.speed = newValue;
    }
    else if (parameterID == ParameterIDs::freqLow)
    {
        mParams.freqLow = newValue;
        mFreqLowBin = freqToBin(newValue, mProcessSpec.sampleRate);
    }
    else if (parameterID == ParameterIDs::freqHigh)
    {
        mParams.freqHigh = newValue;
        mFreqHighBin = freqToBin(newValue, mProcessSpec.sampleRate);
        mFreqHighBin = jmin(mFreqHighBin, kHalfFFTSize - 1);
    }
    else if (parameterID == ParameterIDs::sharpness)
    {
        mParams.sharpness = newValue;
    }
    else if (parameterID == ParameterIDs::mix)
    {
        mParams.mix = newValue;
    }
    else if (parameterID == ParameterIDs::inputGain)
    {
        mParams.inputGain = newValue;
    }
    else if (parameterID == ParameterIDs::outputGain)
    {
        mParams.outputGain = newValue;
    }
    else if (parameterID == ParameterIDs::paramSet)
    {
        mParamSet = static_cast<int>(newValue);
    }
}

//==============================================================================
// 状态保存/恢复
//==============================================================================
void VCSmoothProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    if (xml != nullptr)
    {
        MemoryOutputStream mos (destData, true);
        xml->writeTo (mos, {});
    }
}

void VCSmoothProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xmlState = parseXML (String ((const char*)data, sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState (ValueTree::fromXml (*xmlState));
}

//==============================================================================
// 创建编辑器
//==============================================================================
AudioProcessorEditor* VCSmoothProcessor::createEditor()
{
    return new VCSmoothEditor (*this);
}

//==============================================================================
// 插件入口点
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCSmoothProcessor();
}
