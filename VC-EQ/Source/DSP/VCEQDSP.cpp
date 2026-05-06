#include "VCEQDSP.h"

VCEQDSP::VCEQDSP()
{
    for (int i = 0; i < kNumBands; ++i)
    {
        mBands[i].frequency = kDefaultFrequencies[i];
        mBands[i].q = kDefaultQ[i];
        mBands[i].gainDB = kDefaultGains[i];
        
        if (i == 0)
            mBands[i].type = FilterType::LowShelf;
        else if (i == kNumBands - 1)
            mBands[i].type = FilterType::HighShelf;
        else
            mBands[i].type = FilterType::Parametric;
    }
}

VCEQDSP::~VCEQDSP()
{
}

void VCEQDSP::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;
    
    mInternalBuffer.resize(blockSize * 2);
    
    // 创建指针数组用于 AudioBlock
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;
    
    // 准备所有滤波器
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<uint32_t>(blockSize);
    spec.numChannels = 2;
    
    for (auto& band : mBands)
    {
        band.filter.prepare(spec);
    }
    
    updateFilterCoefficients(0);
    updateFilterCoefficients(1);
    updateFilterCoefficients(2);
    updateFilterCoefficients(3);
    updateFilterCoefficients(4);
}

void VCEQDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled)
        return;
    
    // 复制到内部缓冲区
    for (int i = 0; i < numSamples; ++i)
    {
        mInternalBuffer[i * 2] = left[i];
        mInternalBuffer[i * 2 + 1] = right[i];
    }
    
    // 更新指针
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + numSamples;
    
    // 创建 AudioBlock
    juce::dsp::AudioBlock<float> block(mInternalPtrs.data(), 2, numSamples);
    process(block);
    
    // 复制回输出
    for (int i = 0; i < numSamples; ++i)
    {
        left[i] = mInternalBuffer[i * 2];
        right[i] = mInternalBuffer[i * 2 + 1];
    }
}

void VCEQDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled)
        return;
    
    for (int i = 0; i < kNumBands; ++i)
    {
        if (!mBands[i].enabled)
            continue;
        
        juce::dsp::ProcessContextReplacing<float> context(block);
        mBands[i].filter.process(context);
    }
}

void VCEQDSP::reset()
{
    for (auto& band : mBands)
    {
        band.filter.reset();
    }
}

void VCEQDSP::setBand(int index, const BandParams& params)
{
    if (index < 0 || index >= kNumBands)
        return;
    
    mBands[index].enabled = params.enabled;
    mBands[index].type = params.type;
    mBands[index].frequency = params.frequency;
    mBands[index].q = params.q;
    mBands[index].gainDB = params.gainDB;
    
    updateFilterCoefficients(index);
}

VCEQDSP::BandParams VCEQDSP::getBand(int index) const
{
    BandParams params;
    
    if (index >= 0 && index < kNumBands)
    {
        params.enabled = mBands[index].enabled;
        params.type = mBands[index].type;
        params.frequency = mBands[index].frequency;
        params.q = mBands[index].q;
        params.gainDB = mBands[index].gainDB;
    }
    
    return params;
}

void VCEQDSP::setAllBands(const BandParams params[kNumBands])
{
    for (int i = 0; i < kNumBands; ++i)
    {
        setBand(i, params[i]);
    }
}

void VCEQDSP::setEnabled(bool enabled)
{
    mEnabled = enabled;
}

void VCEQDSP::updateFilterCoefficients(int band)
{
    if (band < 0 || band >= kNumBands)
        return;
    
    auto& b = mBands[band];
    
    if (mCachedParams[band].freq == b.frequency &&
        mCachedParams[band].q == b.q &&
        mCachedParams[band].gainDB == b.gainDB &&
        mCachedParams[band].type == b.type &&
        mCachedParams[band].enabled == b.enabled)
    {
        return;
    }
    
    mCachedParams[band].freq = b.frequency;
    mCachedParams[band].q = b.q;
    mCachedParams[band].gainDB = b.gainDB;
    mCachedParams[band].type = b.type;
    mCachedParams[band].enabled = b.enabled;
    
    float gainFactor = juce::Decibels::decibelsToGain(b.gainDB);
    
    switch (b.type)
    {
        case FilterType::LowShelf:
            b.filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                mSampleRate, b.frequency, b.q, gainFactor);
            break;
            
        case FilterType::HighShelf:
            b.filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                mSampleRate, b.frequency, b.q, gainFactor);
            break;
            
        case FilterType::LowPass:
            b.filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(
                mSampleRate, b.frequency, b.q);
            break;
            
        case FilterType::HighPass:
            b.filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(
                mSampleRate, b.frequency, b.q);
            break;
            
        case FilterType::Parametric:
        default:
            b.filter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                mSampleRate, b.frequency, b.q, gainFactor);
            break;
    }
}
