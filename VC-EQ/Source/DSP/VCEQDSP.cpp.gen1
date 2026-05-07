#include "VCEQDSP.h"

#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#endif

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
    
#ifdef VC_STANDALONE
    // Initialize IIR states
    for (int b = 0; b < kNumBands; ++b) {
        for (int c = 0; c < 2; ++c) {
            mIIRStates[b][c] = IIRState();
        }
    }
#else
    // 准备所有滤波器
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<uint32_t>(blockSize);
    spec.numChannels = 2;
    
    for (auto& band : mBands)
    {
        band.filter.prepare(spec);
    }
#endif
    
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
    
#ifdef VC_STANDALONE
    // Standalone: process each band with manual IIR
    processIIR(left, right, numSamples);
#else
    // JUCE: use AudioBlock (non-interleaved layout)
    // Copy to non-interleaved internal buffer: [LLLL...RRRR...]
    if ((int)mInternalBuffer.size() < numSamples * 2)
        mInternalBuffer.resize(numSamples * 2);
    
    float* leftBuf = mInternalBuffer.data();
    float* rightBuf = mInternalBuffer.data() + numSamples;
    
    for (int i = 0; i < numSamples; ++i)
    {
        leftBuf[i] = left[i];
        rightBuf[i] = right[i];
    }
    
    mInternalPtrs[0] = leftBuf;
    mInternalPtrs[1] = rightBuf;
    
    // Create AudioBlock from non-interleaved channel pointers
    juce::dsp::AudioBlock<float> block(mInternalPtrs.data(), 2, numSamples);
    process(block);
    
    // Copy back
    for (int i = 0; i < numSamples; ++i)
    {
        left[i] = leftBuf[i];
        right[i] = rightBuf[i];
    }
#endif
}

#ifndef VC_STANDALONE
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
#endif

void VCEQDSP::reset()
{
#ifdef VC_STANDALONE
    for (int b = 0; b < kNumBands; ++b) {
        for (int c = 0; c < 2; ++c) {
            mIIRStates[b][c] = IIRState();
        }
    }
#else
    for (auto& band : mBands)
    {
        band.filter.reset();
    }
#endif
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

#ifdef VC_STANDALONE
void VCEQDSP::updateIIRCoefficients(int band, double sampleRate)
{
    auto& b = mBands[band];
    float gainFactor = VCStandalone::decibelsToGain(b.gainDB);
    float omega = 2.0f * VC_PI * static_cast<float>(b.frequency) / static_cast<float>(sampleRate);
    float sinOmega = std::sin(omega);
    float cosOmega = std::cos(omega);
    float alpha = sinOmega / (2.0f * b.q);
    
    IIRState state;
    
    switch (b.type)
    {
        case FilterType::LowShelf:
        {
            // Audio EQ Cookbook: A = 10^(dBgain/40) = sqrt(gainFactor)
            float A = std::sqrt(gainFactor);
            float two_sqrtA_alpha = 2.0f * std::sqrt(A) * alpha;
            state.b0 = A * ((A + 1.0f) - (A - 1.0f) * cosOmega + two_sqrtA_alpha);
            state.b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosOmega);
            state.b2 = A * ((A + 1.0f) - (A - 1.0f) * cosOmega - two_sqrtA_alpha);
            float a0 = (A + 1.0f) + (A - 1.0f) * cosOmega + two_sqrtA_alpha;
            state.a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosOmega);
            state.a2 = (A + 1.0f) + (A - 1.0f) * cosOmega - two_sqrtA_alpha;
            state.b0 /= a0; state.b1 /= a0; state.b2 /= a0;
            state.a1 /= a0; state.a2 /= a0;
            break;
        }
        case FilterType::HighShelf:
        {
            // Audio EQ Cookbook: A = 10^(dBgain/40) = sqrt(gainFactor)
            float A = std::sqrt(gainFactor);
            float two_sqrtA_alpha = 2.0f * std::sqrt(A) * alpha;
            state.b0 = A * ((A + 1.0f) + (A - 1.0f) * cosOmega + two_sqrtA_alpha);
            state.b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosOmega);
            state.b2 = A * ((A + 1.0f) + (A - 1.0f) * cosOmega - two_sqrtA_alpha);
            float a0 = (A + 1.0f) - (A - 1.0f) * cosOmega + two_sqrtA_alpha;
            state.a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosOmega);
            state.a2 = (A + 1.0f) - (A - 1.0f) * cosOmega - two_sqrtA_alpha;
            state.b0 /= a0; state.b1 /= a0; state.b2 /= a0;
            state.a1 /= a0; state.a2 /= a0;
            break;
        }
        case FilterType::Parametric:
        default:
        {
            // Audio EQ Cookbook: A = 10^(dBgain/40) = sqrt(gainFactor)
            float A = std::sqrt(gainFactor);
            state.b0 = 1.0f + alpha * A;
            state.b1 = -2.0f * cosOmega;
            state.b2 = 1.0f - alpha * A;
            float a0 = 1.0f + alpha / A;
            state.a1 = -2.0f * cosOmega;
            state.a2 = 1.0f - alpha / A;
            state.b0 /= a0; state.b1 /= a0; state.b2 /= a0;
            state.a1 /= a0; state.a2 /= a0;
            break;
        }
        case FilterType::LowPass:
        {
            state.b0 = (1.0f - cosOmega) / 2.0f;
            state.b1 = 1.0f - cosOmega;
            state.b2 = (1.0f - cosOmega) / 2.0f;
            float a0 = 1.0f + alpha;
            state.a1 = -2.0f * cosOmega;
            state.a2 = 1.0f - alpha;
            state.b0 /= a0; state.b1 /= a0; state.b2 /= a0;
            state.a1 /= a0; state.a2 /= a0;
            break;
        }
        case FilterType::HighPass:
        {
            state.b0 = (1.0f + cosOmega) / 2.0f;
            state.b1 = -(1.0f + cosOmega);
            state.b2 = (1.0f + cosOmega) / 2.0f;
            float a0 = 1.0f + alpha;
            state.a1 = -2.0f * cosOmega;
            state.a2 = 1.0f - alpha;
            state.b0 /= a0; state.b1 /= a0; state.b2 /= a0;
            state.a1 /= a0; state.a2 /= a0;
            break;
        }
    }
    
    for (int c = 0; c < 2; ++c) {
        mIIRStates[band][c] = state;
    }
}

void VCEQDSP::processIIR(float* left, float* right, int numSamples)
{
    float* channels[2] = { left, right };
    
    for (int band = 0; band < kNumBands; ++band)
    {
        if (!mBands[band].enabled)
            continue;
        
        for (int ch = 0; ch < 2; ++ch)
        {
            IIRState& s = mIIRStates[band][ch];
            float* data = channels[ch];
            
            for (int i = 0; i < numSamples; ++i)
            {
                float x = data[i];
                float y = s.b0 * x + s.b1 * s.x1 + s.b2 * s.x2 - s.a1 * s.y1 - s.a2 * s.y2;
                s.x2 = s.x1; s.x1 = x;
                s.y2 = s.y1; s.y1 = y;
                data[i] = y;
            }
        }
    }
}
#endif

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
    
#ifdef VC_STANDALONE
    updateIIRCoefficients(band, mSampleRate);
#else
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
#endif
}
