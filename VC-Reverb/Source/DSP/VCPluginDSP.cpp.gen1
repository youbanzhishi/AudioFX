#include "VCPluginDSP.h"

//==============================================================================
// Standalone mode includes
//==============================================================================
#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#include <cstring>
#endif

//==============================================================================
// Construction / Destruction
//==============================================================================
VCPluginDSP::VCPluginDSP()
{
    // Initialize default parameters
    mParams.roomSize = 50.0f;
    mParams.decay = 50.0f;
    mParams.damping = 50.0f;
    mParams.preDelay = 20.0f;
    mParams.mix = 30.0f;
    mParams.enabled = true;
}

VCPluginDSP::~VCPluginDSP()
{
}

//==============================================================================
// Calculate scaled buffer size based on room size
//==============================================================================
int VCPluginDSP::calcCombBufferSize(int baseSize)
{
    float scale = 0.5f + mRoomSizeFactor * 1.5f;  // 0.5 to 2.0
    return static_cast<int>(baseSize * scale + 0.5f);
}

int VCPluginDSP::calcAllpassBufferSize(int baseSize)
{
    float scale = 0.5f + mRoomSizeFactor * 1.5f;
    return static_cast<int>(baseSize * scale + 0.5f);
}

//==============================================================================
// Update internal parameters from Params
//==============================================================================
void VCPluginDSP::updateParameters()
{
    mRoomSizeFactor = mParams.roomSize / 100.0f;
    
    // Damping: map 0-100% to a subtle range (0.0 to 0.3)
    // Lower values = less damping (brighter reverb), higher = more damping (darker)
    mDampingFactor = mParams.damping / 100.0f;
    
    // Calculate feedback from decay (0-100 -> 0.5-0.98)
    float decayNormalized = mParams.decay / 100.0f;
    mDecayFeedback = 0.5f + decayNormalized * 0.48f;
    
    // Pre-delay in samples
    mPreDelaySamples = static_cast<int>(mParams.preDelay / 1000.0f * mSampleRate);
    mPreDelaySamples = VC_JCLAMP(mPreDelaySamples, 0, MAX_PREDELAY_SAMPLES - 1);
    
    // Propagate parameter changes to Comb and Allpass filters
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 4; ++i) {
            mCombs[ch][i].setFeedback(mDecayFeedback);
            mCombs[ch][i].setDamping(mDampingFactor);
        }
        for (int i = 0; i < 2; ++i) {
            mAllpasses[ch][i].setFeedback(0.5f);
        }
    }
    
    // Post-reverb wet signal filter coefficients
    // Lowpass: high-cut at 8kHz (remove high-frequency shimmer)
    // One-pole IIR: y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
    // alpha = 1 - exp(-2*PI*fc/fs)
    float lpCutoff = VC_JCLAMP(mParams.wetLPF, 1000.0f, 16000.0f);
    mWetLPCoeff = 1.0f - std::exp(-2.0f * VC_PI * lpCutoff / static_cast<float>(mSampleRate));
    
    // Highpass: low-cut at 200Hz (remove low-frequency rumble)
    // y[n] = alpha * y[n-1] + alpha * (x[n] - x[n-1])
    // We use a simplified one-pole: compute HP from (input - LP output)
    float hpCutoff = VC_JCLAMP(mParams.wetHPF, 20.0f, 500.0f);
    mWetHPCoeff = 1.0f - std::exp(-2.0f * VC_PI * hpCutoff / static_cast<float>(mSampleRate));
    
}

//==============================================================================
// Prepare DSP for processing
//==============================================================================
void VCPluginDSP::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;

    // Resize internal buffer for stereo processing
    mInternalBuffer.resize(blockSize * 2);

    // Create pointer array for AudioBlock
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;

    // Resize pre-delay buffers (max size)
    mPreDelayBuffer[0].assign(MAX_PREDELAY_SAMPLES, 0.0f);
    mPreDelayBuffer[1].assign(MAX_PREDELAY_SAMPLES, 0.0f);
    mPreDelayWritePos = 0;
    
    // Update parameters and initialize filter buffers
    updateParameters();
    
    // Initialize Comb filters for both channels
    // Left channel
    mCombs[0][0].setBufferSize(calcCombBufferSize(BASE_COMB_L0));
    mCombs[0][1].setBufferSize(calcCombBufferSize(BASE_COMB_L1));
    mCombs[0][2].setBufferSize(calcCombBufferSize(BASE_COMB_L2));
    mCombs[0][3].setBufferSize(calcCombBufferSize(BASE_COMB_L3));
    
    // Right channel (slightly different sizes for stereo)
    mCombs[1][0].setBufferSize(calcCombBufferSize(BASE_COMB_R0));
    mCombs[1][1].setBufferSize(calcCombBufferSize(BASE_COMB_R1));
    mCombs[1][2].setBufferSize(calcCombBufferSize(BASE_COMB_R2));
    mCombs[1][3].setBufferSize(calcCombBufferSize(BASE_COMB_R3));
    
    // Initialize Allpass filters
    mAllpasses[0][0].setBufferSize(calcAllpassBufferSize(BASE_ALLPASS_L0));
    mAllpasses[0][1].setBufferSize(calcAllpassBufferSize(BASE_ALLPASS_L1));
    mAllpasses[1][0].setBufferSize(calcAllpassBufferSize(BASE_ALLPASS_R0));
    mAllpasses[1][1].setBufferSize(calcAllpassBufferSize(BASE_ALLPASS_R1));
    
    // Set filter parameters
    reset();
}

//==============================================================================
// Process interleaved stereo buffer
//==============================================================================
void VCPluginDSP::process(float* left, float* right, int numSamples)
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
void VCPluginDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled)
        return;

    // Process both channels
    float* left = block.getChannelPointer(0);
    float* right = block.getChannelPointer(1);
    int numSamples = static_cast<int>(block.getNumSamples());
    
    processInternal(left, right, numSamples);
}
#endif

//==============================================================================
// Internal reverb processing
//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    updateParameters();
    
    float wetGain = 1.0f;
    float dryGain = 1.0f;
    float mixFactor = mParams.mix / 100.0f;
    
    for (int i = 0; i < numSamples; ++i) {
        float inL = left[i];
        float inR = right[i];
        
        // Store dry signal
        float dryL = inL;
        float dryR = inR;
        
        // Apply pre-delay
        float predelayL = 0.0f, predelayR = 0.0f;
        if (mPreDelaySamples > 0) {
            int readPos = (mPreDelayWritePos - mPreDelaySamples + MAX_PREDELAY_SAMPLES) % MAX_PREDELAY_SAMPLES;
            predelayL = mPreDelayBuffer[0][readPos];
            predelayR = mPreDelayBuffer[1][readPos];
        }
        
        // Write to pre-delay buffer
        mPreDelayBuffer[0][mPreDelayWritePos] = inL;
        mPreDelayBuffer[1][mPreDelayWritePos] = inR;
        mPreDelayWritePos = (mPreDelayWritePos + 1) % MAX_PREDELAY_SAMPLES;
        
        float delayedL = (mPreDelaySamples > 0) ? predelayL : inL;
        float delayedR = (mPreDelaySamples > 0) ? predelayR : inR;
        
        // Process through Comb filters in parallel
        float combOutL = 0.0f;
        float combOutR = 0.0f;
        
        for (int c = 0; c < 4; ++c) {
            combOutL += mCombs[0][c].process(delayedL);
            combOutR += mCombs[1][c].process(delayedR);
        }
        
        // Normalize comb output
        combOutL *= 0.15f;
        combOutR *= 0.15f;
        
        // Process through Allpass filters in series
        float allpassOutL = mAllpasses[0][0].process(combOutL);
        allpassOutL = mAllpasses[0][1].process(allpassOutL);
        
        float allpassOutR = mAllpasses[1][0].process(combOutR);
        allpassOutR = mAllpasses[1][1].process(allpassOutR);
        
        // Apply wet signal filtering (only affects reverb tail, dry is untouched)
        // Lowpass: high-cut removes high-frequency shimmer from allpass
        mWetLPFState[0] = mWetLPCoeff * allpassOutL + (1.0f - mWetLPCoeff) * mWetLPFState[0];
        mWetLPFState[1] = mWetLPCoeff * allpassOutR + (1.0f - mWetLPCoeff) * mWetLPFState[1];
        float wetFilteredL = mWetLPFState[0];
        float wetFilteredR = mWetLPFState[1];
        
        // Highpass: low-cut removes low-frequency rumble from reverb tail
        mWetHPFState[0] = mWetHPCoeff * wetFilteredL + (1.0f - mWetHPCoeff) * mWetHPFState[0];
        mWetHPFState[1] = mWetHPCoeff * wetFilteredR + (1.0f - mWetHPCoeff) * mWetHPFState[1];
        wetFilteredL = wetFilteredL - mWetHPFState[0];
        wetFilteredR = wetFilteredR - mWetHPFState[1];
        
        // Mix dry and wet (wet is now filtered)
        float outL = dryL * (1.0f - mixFactor) + wetFilteredL * mixFactor;
        float outR = dryR * (1.0f - mixFactor) + wetFilteredR * mixFactor;
        
        left[i] = outL;
        right[i] = outR;
    }
}

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
    // Clear all filter buffers
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 4; ++i) {
            mCombs[ch][i].clear();
        }
        for (int i = 0; i < 2; ++i) {
            mAllpasses[ch][i].clear();
        }
        std::fill(mPreDelayBuffer[ch].begin(), mPreDelayBuffer[ch].end(), 0.0f);
    }
    mPreDelayWritePos = 0;
    
    // Reset wet filter states
    mWetLPFState[0] = 0.0f; mWetLPFState[1] = 0.0f;
    mWetHPFState[0] = 0.0f; mWetHPFState[1] = 0.0f;
    
    // Update filter parameters
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 4; ++i) {
            mCombs[ch][i].setFeedback(mDecayFeedback);
            mCombs[ch][i].setDamping(mDampingFactor);
        }
        for (int i = 0; i < 2; ++i) {
            mAllpasses[ch][i].setFeedback(0.5f);
        }
    }
}

//==============================================================================
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;
    mEnabled = p.enabled;
    updateParameters();
}

//==============================================================================
// Get parameters
//==============================================================================
VCPluginDSP::Params VCPluginDSP::getParams() const
{
    return mParams;
}

//==============================================================================
// Set enabled state
//==============================================================================
void VCPluginDSP::setEnabled(bool enabled)
{
    mEnabled = enabled;
}
