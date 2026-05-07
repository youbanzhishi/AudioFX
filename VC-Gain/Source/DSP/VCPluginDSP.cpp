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
    // Initialize default parameters
    mParams.gainDB = 0.0f;
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
    // Initialize IIR states (not used for VC-Gain, but kept for API compatibility)
    for (auto& state : mIIRStates) {
        state = IIRState();
    }
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

    // VC-Gain: simple gain + dry/wet mix
    // output = dry * in + wet * (in * gain)
    float gain = dBToLinear(mParams.gainDB);
    float wet = mParams.mix / 100.0f;
    float dry = 1.0f - wet;

    for (size_t ch = 0; ch < block.getNumChannels(); ++ch) {
        auto* data = block.getChannelPointer(ch);
        for (size_t i = 0; i < block.getNumSamples(); ++i) {
            float in = data[i];
            data[i] = dry * in + wet * (in * gain);
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
    for (auto& state : mIIRStates) {
        state = IIRState();
    }
#endif
}

//==============================================================================
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;
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
// Standalone IIR processing - Simplified to pure gain+mix
// VC-Gain does NOT use IIR filtering
//==============================================================================
#ifdef VC_STANDALONE
void VCPluginDSP::updateIIRCoefficients()
{
    // VC-Gain: no IIR filtering needed, gain is applied directly in processIIR
    // Coefficients are kept for API compatibility but not used
}

void VCPluginDSP::processIIR(float* left, float* right, int numSamples)
{
    // VC-Gain: simple gain + dry/wet mix
    // output = dry * in + wet * (in * gain)
    float gain = VCStandalone::decibelsToGain(mParams.gainDB);
    float wet = mParams.mix / 100.0f;
    float dry = 1.0f - wet;

    float* channels[2] = { left, right };
    for (int ch = 0; ch < 2; ++ch) {
        float* data = channels[ch];
        for (int i = 0; i < numSamples; ++i) {
            float in = data[i];
            data[i] = dry * in + wet * (in * gain);
        }
    }
}
#endif
