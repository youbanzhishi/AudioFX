#include "VCPluginDSP.h"

//==============================================================================
// Standalone mode includes
//==============================================================================
#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#endif

//==============================================================================
// Construction / Destruction
//==============================================================================
VCDeEsserDSP::VCDeEsserDSP()
{
    mParams.threshold = -20.0f;
    mParams.frequency = 6000.0f;
    mParams.reduction = -10.0f;
    mParams.mix = 100.0f;
    mParams.enabled = true;
}

VCDeEsserDSP::~VCDeEsserDSP()
{
}

//==============================================================================
// Prepare DSP for processing
//==============================================================================
void VCDeEsserDSP::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;

    // Resize internal buffer for stereo processing
    mInternalBuffer.resize(blockSize * 2);

    // Create pointer array for AudioBlock
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;

    // Initialize BPF states
    for (int ch = 0; ch < 2; ++ch) {
        mBpStates[ch].reset();
    }
    mEnvelope[0] = mEnvelope[1] = 0.0f;
    
    updateBPFCoefficients();
}

//==============================================================================
// Process interleaved stereo buffer
//==============================================================================
void VCDeEsserDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled)
        return;

#ifdef VC_STANDALONE
    processInternal(left, right, numSamples);
#else
    // JUCE: use AudioBlock (non-interleaved layout)
    // Copy to non-interleaved internal buffer: [LLLL...RRRR...]
    if ((int)mInternalBuffer.size() < numSamples * 2)
        mInternalBuffer.resize(numSamples * 2);

    float* leftBuf = mInternalBuffer.data();
    float* rightBuf = mInternalBuffer.data() + numSamples;

    for (int i = 0; i < numSamples; ++i) {
        leftBuf[i] = left[i];
        rightBuf[i] = right[i];
    }

    mInternalPtrs[0] = leftBuf;
    mInternalPtrs[1] = rightBuf;

    // Create AudioBlock from non-interleaved channel pointers
    juce::dsp::AudioBlock<float> block(mInternalPtrs.data(), 2, numSamples);
    process(block);

    // Copy back
    for (int i = 0; i < numSamples; ++i) {
        left[i] = leftBuf[i];
        right[i] = rightBuf[i];
    }
#endif
}

//==============================================================================
// JUCE AudioBlock processing (non-interleaved)
//==============================================================================
#ifndef VC_STANDALONE
void VCDeEsserDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled)
        return;

    float wet = mParams.mix / 100.0f;
    float dry = 1.0f - wet;
    float threshGain = dBToLinear(mParams.threshold);
    float maxReduction = dBToLinear(mParams.reduction);
    
    for (size_t ch = 0; ch < block.getNumChannels(); ++ch) {
        auto* data = block.getChannelPointer(ch);
        for (size_t i = 0; i < block.getNumSamples(); ++i) {
            float in = data[i];
            
            // Simple envelope follower on bandpass-filtered signal
            float bp = mBpStates[ch].process(in);  // bandpass
            float env = std::abs(bp);
            mEnvelope[ch] = mAttack * mEnvelope[ch] + (1.0f - mAttack) * env;
            
            // Gain reduction - simple threshold-based with max reduction cap
            float gain = 1.0f;
            if (mEnvelope[ch] > threshGain) {
                float overDb = linearToDb(mEnvelope[ch] / threshGain);
                // Reduction: scale over-threshold amount, capped by maxReduction
                float reductionDb = std::min(overDb * 0.8f, -mParams.reduction);  // reduction is negative, so -reduction is positive
                gain = dBToLinear(-reductionDb);
            }
            
            float processed = in * gain;
            data[i] = dry * in + wet * processed;
        }
    }
}
#endif

//==============================================================================
// Standalone internal processing
//==============================================================================
void VCDeEsserDSP::processInternal(float* left, float* right, int numSamples)
{
    float wet = mParams.mix / 100.0f;
    float dry = 1.0f - wet;
    float threshGain = dBToLinear(mParams.threshold);
    float maxReduction = dBToLinear(mParams.reduction);
    
    float* channels[2] = { left, right };

    for (int ch = 0; ch < 2; ++ch) {
        float* data = channels[ch];
        
        for (int i = 0; i < numSamples; ++i) {
            float in = data[i];
            
            // Simple envelope follower on bandpass-filtered signal
            float bp = mBpStates[ch].process(in);  // bandpass
            float env = std::abs(bp);
            mEnvelope[ch] = mAttack * mEnvelope[ch] + (1.0f - mAttack) * env;
            
            // Gain reduction - simple threshold-based with max reduction cap
            float gain = 1.0f;
            if (mEnvelope[ch] > threshGain) {
                float overDb = linearToDb(mEnvelope[ch] / threshGain);
                float reductionDb = std::min(overDb * 0.8f, -mParams.reduction);
                gain = dBToLinear(-reductionDb);
            }
            
            float processed = in * gain;
            data[i] = dry * in + wet * processed;
        }
    }
}

//==============================================================================
// Update BPF coefficients based on current frequency
//==============================================================================
void VCDeEsserDSP::updateBPFCoefficients()
{
    // Second-order bandpass filter
    float freq = (float)mParams.frequency;
    float Q = 2.0f;  // Quality factor - wider bandwidth for sibilance detection
    
    float omega = 2.0f * VC_PI * freq / static_cast<float>(mSampleRate);
    float sinOmega = std::sin(omega);
    float cosOmega = std::cos(omega);
    float alpha = sinOmega / (2.0f * Q);
    
    float b0 = alpha;
    float b1 = 0.0f;
    float b2 = -alpha;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cosOmega;
    float a2 = 1.0f - alpha;
    
    // Normalize
    for (int ch = 0; ch < 2; ++ch) {
        mBpStates[ch].b0 = b0 / a0;
        mBpStates[ch].b1 = b1 / a0;
        mBpStates[ch].b2 = b2 / a0;
        mBpStates[ch].a1 = a1 / a0;
        mBpStates[ch].a2 = a2 / a0;
    }
}

//==============================================================================
// Reset processing state
//==============================================================================
void VCDeEsserDSP::reset()
{
    for (int ch = 0; ch < 2; ++ch) {
        mBpStates[ch].reset();
    }
    mEnvelope[0] = mEnvelope[1] = 0.0f;
}

//==============================================================================
// Set parameters
//==============================================================================
void VCDeEsserDSP::setParams(const Params& p)
{
    bool freqChanged = (mParams.frequency != p.frequency);
    mParams = p;
    
    if (freqChanged) {
        updateBPFCoefficients();
    }
}

//==============================================================================
// Get parameters
//==============================================================================
VCDeEsserDSP::Params VCDeEsserDSP::getParams() const
{
    return mParams;
}

//==============================================================================
// Set enabled state
//==============================================================================
void VCDeEsserDSP::setEnabled(bool enabled)
{
    mEnabled = enabled;
}
