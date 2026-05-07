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
VCPluginDSP::VCPluginDSP()
{
    // Initialize default parameters for Dynamic EQ
    mParams.frequency = 200.0f;
    mParams.gain = -6.0f;
    mParams.q = 1.0f;
    mParams.threshold = -12.0f;
    mParams.range = -12.0f;
    mParams.attack = 10.0f;
    mParams.release = 100.0f;
    mParams.mix = 100.0f;
    mParams.enabled = true;
}

VCPluginDSP::~VCPluginDSP()
{
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

#ifdef VC_STANDALONE
    // Initialize IIR states
    for (int i = 0; i < 2; ++i) {
        mEQState[i] = IIRState();
        mBPState[i] = IIRState();
        mEnvelope[i] = 0.0f;
    }
    updateEQCoefficients();
    updateBPCoefficients();
#else
    // In JUCE mode, coefficients are updated via setParams()
#endif
}

//==============================================================================
// Process interleaved stereo buffer
//==============================================================================
void VCPluginDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled)
        return;

#ifdef VC_STANDALONE
    processIIR(left, right, numSamples);
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

    // Using JUCE DSP module's IIR filter
    // Process each channel
    float wet = mParams.mix / 100.0f;
    float dry = 1.0f - wet;
    
    float threshGain = dBToLinear(mParams.threshold);
    float maxRangeGain = dBToLinear(mParams.range);
    float attackCoeff = std::exp(-1.0f / (mParams.attack * 0.001f * mSampleRate));
    float releaseCoeff = std::exp(-1.0f / (mParams.release * 0.001f * mSampleRate));

    // Simple implementation: apply static EQ gain per channel
    // Dynamic detection is done by analyzing input level
    float staticGain = dBToLinear(mParams.gain);
    
    for (size_t ch = 0; ch < block.getNumChannels(); ++ch) {
        auto* data = block.getChannelPointer(ch);
        for (size_t i = 0; i < block.getNumSamples(); ++i) {
            float in = data[i];
            
            // Apply static EQ gain (simplified - real impl uses IIR filter)
            float eqOut = in * staticGain;
            
            // Envelope follower for dynamic gain
            float env = std::abs(in);
            if (env > mEnvelope[ch]) {
                mEnvelope[ch] = attackCoeff * mEnvelope[ch] + (1.0f - attackCoeff) * env;
            } else {
                mEnvelope[ch] = releaseCoeff * mEnvelope[ch] + (1.0f - releaseCoeff) * env;
            }
            
            // Calculate dynamic gain based on threshold
            float dynamicGain = 1.0f;
            if (mEnvelope[ch] > threshGain) {
                float overRatio = (mEnvelope[ch] - threshGain) / threshGain;
                dynamicGain = 1.0f + (maxRangeGain - 1.0f) * VC_JCLAMP(overRatio, 0.0f, 1.0f);
            }
            
            // Apply dynamic gain
            float out = eqOut * dynamicGain;
            
            // Dry/Wet mix
            data[i] = dry * in + wet * out;
        }
    }
}
#endif

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
#ifdef VC_STANDALONE
    for (int i = 0; i < 2; ++i) {
        mEQState[i] = IIRState();
        mBPState[i] = IIRState();
        mEnvelope[i] = 0.0f;
    }
#endif
}

//==============================================================================
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;
    
#ifdef VC_STANDALONE
    updateEQCoefficients();
    updateBPCoefficients();
#endif
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

//==============================================================================
// Standalone IIR processing - Dynamic EQ implementation
//==============================================================================
#ifdef VC_STANDALONE

// Peaking EQ coefficients (Robert Bristow-Johnson formula)
void VCPluginDSP::updateEQCoefficients()
{
    float freq = VC_JCLAMP(mParams.frequency, 20.0f, (float)mSampleRate * 0.49f);
    float gainDB = mParams.gain;
    float Q = VC_JCLAMP(mParams.q, 0.1f, 10.0f);
    
    float A = std::pow(10.0f, gainDB / 40.0f);
    float omega = 2.0f * VC_PI * freq / static_cast<float>(mSampleRate);
    float sn = std::sin(omega);
    float cs = std::cos(omega);
    float alpha = sn / (2.0f * Q);
    
    float b0 = 1.0f + alpha * A;
    float b1 = -2.0f * cs;
    float b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    float a1 = -2.0f * cs;
    float a2 = 1.0f - alpha / A;
    
    // Normalize by a0
    for (int i = 0; i < 2; ++i) {
        mEQState[i].b0 = b0 / a0;
        mEQState[i].b1 = b1 / a0;
        mEQState[i].b2 = b2 / a0;
        mEQState[i].a1 = a1 / a0;
        mEQState[i].a2 = a2 / a0;
    }
}

// Bandpass filter coefficients for detection
void VCPluginDSP::updateBPCoefficients()
{
    float freq = VC_JCLAMP(mParams.frequency, 20.0f, (float)mSampleRate * 0.49f);
    float Q = VC_JCLAMP(mParams.q, 0.1f, 10.0f);
    
    float omega = 2.0f * VC_PI * freq / static_cast<float>(mSampleRate);
    float sn = std::sin(omega);
    float cs = std::cos(omega);
    float alpha = sn / (2.0f * Q);
    
    float b0 = alpha;
    float b1 = 0.0f;
    float b2 = -alpha;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cs;
    float a2 = 1.0f - alpha;
    
    // Normalize by a0
    for (int i = 0; i < 2; ++i) {
        mBPState[i].b0 = b0 / a0;
        mBPState[i].b1 = b1 / a0;
        mBPState[i].b2 = b2 / a0;
        mBPState[i].a1 = a1 / a0;
        mBPState[i].a2 = a2 / a0;
    }
}

void VCPluginDSP::processIIR(float* left, float* right, int numSamples)
{
    float wet = mParams.mix / 100.0f;
    float dry = 1.0f - wet;
    float threshGain = VCStandalone::decibelsToGain(mParams.threshold);
    float maxRangeGain = VCStandalone::decibelsToGain(mParams.range);
    float attackCoeff = std::exp(-1.0f / (mParams.attack * 0.001f * mSampleRate));
    float releaseCoeff = std::exp(-1.0f / (mParams.release * 0.001f * mSampleRate));
    
    float* channels[2] = { left, right };
    
    for (int i = 0; i < numSamples; ++i) {
        for (int ch = 0; ch < 2; ++ch) {
            float in = channels[ch][i];
            
            // 1. Apply static EQ
            float eqOut = mEQState[ch].process(in);
            
            // 2. Detect band energy via bandpass
            float bp = mBPState[ch].process(in);
            float env = std::abs(bp);
            if (env > mEnvelope[ch]) {
                mEnvelope[ch] = attackCoeff * mEnvelope[ch] + (1.0f - attackCoeff) * env;
            } else {
                mEnvelope[ch] = releaseCoeff * mEnvelope[ch] + (1.0f - releaseCoeff) * env;
            }
            
            // 3. Calculate dynamic gain
            float dynamicGain = 1.0f;
            if (mEnvelope[ch] > threshGain) {
                float overRatio = (mEnvelope[ch] - threshGain) / threshGain;
                dynamicGain = 1.0f + (maxRangeGain - 1.0f) * VC_JCLAMP(overRatio, 0.0f, 1.0f);
            }
            
            // 4. Apply dynamic gain to EQ output
            float out = eqOut * dynamicGain;
            channels[ch][i] = dry * in + wet * out;
        }
    }
}
#endif
