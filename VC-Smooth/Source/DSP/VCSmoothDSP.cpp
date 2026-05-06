#include "VCSmoothDSP.h"

using namespace juce;

//==============================================================================
// 工具函数
//==============================================================================
inline float VCSmoothDSP::dBToLinear(float dB) const
{
    return std::pow(10.0f, dB / 20.0f);
}

inline int VCSmoothDSP::freqToBin(float hz) const
{
    if (hz <= 0.0f)
        return 0;
    return static_cast<int>(hz * VCSmoothConfig::kFFTSize / mSampleRate);
}

//==============================================================================
// 构造函数
//==============================================================================
VCSmoothDSP::VCSmoothDSP()
    : mFFT(int(std::log2(VCSmoothConfig::kFFTSize)))
{
    // 初始化缓冲区 (2帧 + 重叠)
    int bufferSize = VCSmoothConfig::kFFTSize * 2;
    mInputBufferL.resize(bufferSize, 0.0f);
    mInputBufferR.resize(bufferSize, 0.0f);
    mOutputBufferL.resize(bufferSize, 0.0f);
    mOutputBufferR.resize(bufferSize, 0.0f);
    
    // FFT 工作区
    mFFTReal.resize(VCSmoothConfig::kFFTSize * 2, 0.0f);
    mFFTImag.resize(VCSmoothConfig::kFFTSize * 2, 0.0f);
    
    // 创建 Hann 窗
    mWindow.resize(VCSmoothConfig::kFFTSize);
    for (int i = 0; i < VCSmoothConfig::kFFTSize; ++i)
    {
        mWindow[i] = 0.5f * (1.0f - std::cos(2.0f * MathConstants<float>::pi * i / (VCSmoothConfig::kFFTSize - 1)));
    }
    
    // 初始化频谱和增益数组
    mAvgSpectrumL.resize(VCSmoothConfig::kHalfFFTSize, 0.0f);
    mAvgSpectrumR.resize(VCSmoothConfig::kHalfFFTSize, 0.0f);
    mGainSmoothL.resize(VCSmoothConfig::kHalfFFTSize, 1.0f);
    mGainSmoothR.resize(VCSmoothConfig::kHalfFFTSize, 1.0f);
}

VCSmoothDSP::~VCSmoothDSP()
{
}

//==============================================================================
// 准备处理
//==============================================================================
void VCSmoothDSP::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;
    
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
    mFreqLowBin = freqToBin(mParams.freqLow);
    mFreqHighBin = freqToBin(mParams.freqHigh);
    mFreqHighBin = jmin(mFreqHighBin, VCSmoothConfig::kHalfFFTSize - 1);
}

//==============================================================================
// 重置 DSP 状态
//==============================================================================
void VCSmoothDSP::reset()
{
    std::fill(mInputBufferL.begin(), mInputBufferL.end(), 0.0f);
    std::fill(mInputBufferR.begin(), mInputBufferR.end(), 0.0f);
    std::fill(mOutputBufferL.begin(), mOutputBufferL.end(), 0.0f);
    std::fill(mOutputBufferR.begin(), mOutputBufferR.end(), 0.0f);
    std::fill(mAvgSpectrumL.begin(), mAvgSpectrumL.end(), 0.0f);
    std::fill(mAvgSpectrumR.begin(), mAvgSpectrumR.end(), 0.0f);
    std::fill(mGainSmoothL.begin(), mGainSmoothL.end(), 1.0f);
    std::fill(mGainSmoothR.begin(), mGainSmoothR.end(), 1.0f);
    mBufferPos = 0;
}

//==============================================================================
// 参数设置
//==============================================================================
void VCSmoothDSP::setParams(const Params& p)
{
    mParams = p;
    // 更新频率范围 bin
    mFreqLowBin = freqToBin(mParams.freqLow);
    mFreqHighBin = freqToBin(mParams.freqHigh);
    mFreqHighBin = jmin(mFreqHighBin, VCSmoothConfig::kHalfFFTSize - 1);
}

VCSmoothDSP::Params VCSmoothDSP::getParams() const
{
    return mParams;
}

//==============================================================================
// 主处理函数
//==============================================================================
void VCSmoothDSP::process(float* leftChannel, float* rightChannel, int numSamples)
{
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
        // 保存干信号用于 mix
        std::vector<float> dryLeft(numSamples);
        std::vector<float> dryRight(numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            dryLeft[i] = leftChannel[i];
            dryRight[i] = rightChannel[i];
        }
        
        // 频谱处理
        spectralProcessing(leftChannel, rightChannel, numSamples);
        
        // 干湿混合
        if (mParams.mix < 0.999f)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                leftChannel[i] = dryLeft[i] * (1.0f - mParams.mix) + leftChannel[i] * mParams.mix;
                rightChannel[i] = dryRight[i] * (1.0f - mParams.mix) + rightChannel[i] * mParams.mix;
            }
        }
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
void VCSmoothDSP::spectralProcessing(float* leftChannel, float* rightChannel, int numSamples)
{
    // 临时帧缓冲区
    static thread_local std::vector<float> frameL(VCSmoothConfig::kFFTSize, 0.0f);
    static thread_local std::vector<float> frameR(VCSmoothConfig::kFFTSize, 0.0f);
    static thread_local std::vector<float> outFrameL(VCSmoothConfig::kFFTSize, 0.0f);
    static thread_local std::vector<float> outFrameR(VCSmoothConfig::kFFTSize, 0.0f);
    
    // 处理每个 hop
    int processedSamples = 0;
    
    while (processedSamples < numSamples)
    {
        // 检查是否有一个完整的帧需要处理
        int samplesNeeded = VCSmoothConfig::kFFTSize - mBufferPos;
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
        if (mBufferPos >= VCSmoothConfig::kFFTSize)
        {
            // 提取帧并加窗
            for (int i = 0; i < VCSmoothConfig::kFFTSize; ++i)
            {
                frameL[i] = mInputBufferL[i] * mWindow[i];
                frameR[i] = mInputBufferR[i] * mWindow[i];
            }
            
            // 处理 FFT 帧
            processFFTFrame(frameL.data(), frameR.data());
            
            // 将处理后的帧叠加到输出
            for (int i = 0; i < VCSmoothConfig::kFFTSize; ++i)
            {
                outFrameL[i] = frameL[i] * mWindow[i];
                outFrameR[i] = frameR[i] * mWindow[i];
            }
            
            // Overlap-add: 将输出帧添加到输出缓冲区
            for (int i = 0; i < VCSmoothConfig::kFFTSize; ++i)
            {
                mOutputBufferL[i] += outFrameL[i];
                mOutputBufferR[i] += outFrameR[i];
            }
            
            // 将输出缓冲区的样本写入输出
            int outputSamples = jmin(VCSmoothConfig::kHopSize, numSamples);
            for (int i = 0; i < outputSamples; ++i)
            {
                leftChannel[i] = mOutputBufferL[i];
                rightChannel[i] = mOutputBufferR[i];
            }
            
            // 移动输出缓冲区 (向左移动一个 hop)
            for (int i = 0; i < VCSmoothConfig::kFFTSize - VCSmoothConfig::kHopSize; ++i)
            {
                mOutputBufferL[i] = mOutputBufferL[i + VCSmoothConfig::kHopSize];
                mOutputBufferR[i] = mOutputBufferR[i + VCSmoothConfig::kHopSize];
            }
            for (int i = VCSmoothConfig::kFFTSize - VCSmoothConfig::kHopSize; i < VCSmoothConfig::kFFTSize; ++i)
            {
                mOutputBufferL[i] = 0.0f;
                mOutputBufferR[i] = 0.0f;
            }
            
            // 移动输入缓冲区 (向左移动一个 hop)
            for (int i = 0; i < VCSmoothConfig::kFFTSize; ++i)
            {
                mInputBufferL[i] = mInputBufferL[i + VCSmoothConfig::kHopSize];
                mInputBufferR[i] = mInputBufferR[i + VCSmoothConfig::kHopSize];
            }
            for (int i = VCSmoothConfig::kFFTSize; i < VCSmoothConfig::kFFTSize * 2; ++i)
            {
                mInputBufferL[i] = 0.0f;
                mInputBufferR[i] = 0.0f;
            }
            
            mBufferPos -= VCSmoothConfig::kHopSize;
        }
    }
}

//==============================================================================
// FFT 帧处理
//==============================================================================
void VCSmoothDSP::processFFTFrame(float* leftFrame, float* rightFrame)
{
    // ========== 1. FFT 正变换 ==========
    // 准备 FFT 输入 (交替实部虚部)
    for (int i = 0; i < VCSmoothConfig::kFFTSize; ++i)
    {
        mFFTReal[i] = leftFrame[i];
        mFFTImag[i] = 0.0f;
    }
    
    // 执行 FFT
    mFFT.performRealOnlyForwardTransform(mFFTReal.data(), true);
    
    // 获取左声道幅度谱
    std::vector<float> magL(VCSmoothConfig::kHalfFFTSize, 0.0f);
    for (int i = 0; i < VCSmoothConfig::kHalfFFTSize; ++i)
    {
        float real = mFFTReal[i * 2];
        float imag = mFFTReal[i * 2 + 1];
        magL[i] = std::sqrt(real * real + imag * imag) + 1e-10f;
    }
    
    // 准备右声道 FFT 输入
    for (int i = 0; i < VCSmoothConfig::kFFTSize; ++i)
    {
        mFFTReal[i] = rightFrame[i];
        mFFTImag[i] = 0.0f;
    }
    mFFT.performRealOnlyForwardTransform(mFFTReal.data(), true);
    
    // 获取右声道幅度谱
    std::vector<float> magR(VCSmoothConfig::kHalfFFTSize, 0.0f);
    for (int i = 0; i < VCSmoothConfig::kHalfFFTSize; ++i)
    {
        float real = mFFTReal[i * 2];
        float imag = mFFTReal[i * 2 + 1];
        magR[i] = std::sqrt(real * real + imag * imag) + 1e-10f;
    }
    
    // ========== 2. 频谱峰值检测与增益计算 ==========
    // 计算包络跟踪系数 (Speed 参数控制)
    float attackCoef = std::exp(-1.0f / (mParams.speed * 0.1f * mSampleRate / 512.0f));
    float releaseCoef = std::exp(-1.0f / (mParams.speed * 1.0f * mSampleRate / 512.0f));
    float attackCoefInv = 1.0f - attackCoef;
    float releaseCoefInv = 1.0f - releaseCoef;
    
    // 增益平滑系数
    float gainSmoothingCoef = std::exp(-1.0f / (mSampleRate * 0.005f)); // 5ms 平滑
    
    // 临时增益
    std::vector<float> targetGainL(VCSmoothConfig::kHalfFFTSize, 1.0f);
    std::vector<float> targetGainR(VCSmoothConfig::kHalfFFTSize, 1.0f);
    
    // 计算阈值
    float threshold = 1.0f + mParams.sharpness;
    
    // 处理频率范围内的 bins
    for (int i = mFreqLowBin; i <= mFreqHighBin && i < VCSmoothConfig::kHalfFFTSize; ++i)
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
    for (int i = 0; i < VCSmoothConfig::kHalfFFTSize; ++i)
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
    for (int i = 0; i < VCSmoothConfig::kHalfFFTSize; ++i)
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
    for (int i = VCSmoothConfig::kHalfFFTSize; i < VCSmoothConfig::kFFTSize; ++i)
    {
        int mirrorIdx = (VCSmoothConfig::kFFTSize - i) * 2;
        mFFTReal[i * 2] = mFFTReal[mirrorIdx];
        mFFTReal[i * 2 + 1] = -mFFTReal[mirrorIdx + 1];
    }
    
    mFFT.performRealOnlyInverseTransform(mFFTReal.data());
    
    // 复制左声道结果
    for (int i = 0; i < VCSmoothConfig::kFFTSize; ++i)
    {
        leftFrame[i] = mFFTReal[i] / VCSmoothConfig::kFFTSize; // 归一化
    }
    
    // 右声道处理
    for (int i = 0; i < VCSmoothConfig::kHalfFFTSize; ++i)
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
    for (int i = VCSmoothConfig::kHalfFFTSize; i < VCSmoothConfig::kFFTSize; ++i)
    {
        int mirrorIdx = (VCSmoothConfig::kFFTSize - i) * 2;
        mFFTReal[i * 2] = mFFTReal[mirrorIdx];
        mFFTReal[i * 2 + 1] = -mFFTReal[mirrorIdx + 1];
    }
    
    mFFT.performRealOnlyInverseTransform(mFFTReal.data());
    
    // 复制右声道结果
    for (int i = 0; i < VCSmoothConfig::kFFTSize; ++i)
    {
        rightFrame[i] = mFFTReal[i] / VCSmoothConfig::kFFTSize; // 归一化
    }
}
