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
    // Initialize IIR states
    for (auto& state : mIIRStates) {
        state = IIRState();
    }
    updateIIRCoefficients();
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

    // TODO: Implement your DSP algorithm here
    // Example: simple gain processing
    float gain = dBToLinear(mParams.gainDB);

    for (size_t ch = 0; ch < block.getNumChannels(); ++ch) {
        auto* data = block.getChannelPointer(ch);
        for (size_t i = 0; i < block.getNumSamples(); ++i) {
            data[i] *= gain;
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
    
#ifdef VC_STANDALONE
    updateIIRCoefficients();
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
// Standalone IIR processing
//==============================================================================
#ifdef VC_STANDALONE
void VCPluginDSP::updateIIRCoefficients()
{
    // Default: unity gain (bypass)
    float gain = VCStandalone::decibelsToGain(mParams.gainDB);

    // Simple single-pole lowpass as placeholder
    float omega = 2.0f * VC_PI * 1000.0f / static_cast<float>(mSampleRate);
    float alpha = std::sin(omega) / (2.0f * 0.707f);

    for (auto& state : mIIRStates) {
        state.b0 = (1.0f - alpha) / (1.0f + alpha) * gain;
        state.b1 = 0.0f;
        state.b2 = 0.0f;
        state.a1 = (1.0f - alpha) / (1.0f + alpha);
        state.a2 = 0.0f;
    }
}

void VCPluginDSP::processIIR(float* left, float* right, int numSamples)
{
    float* channels[2] = { left, right };

    for (int ch = 0; ch < 2; ++ch) {
        IIRState& s = mIIRStates[ch];
        float* data = channels[ch];

        for (int i = 0; i < numSamples; ++i) {
            float x = data[i];
            float y = s.b0 * x + s.b1 * s.x1 + s.b2 * s.x2 
                    - s.a1 * s.y1 - s.a2 * s.y2;
            s.x2 = s.x1; s.x1 = x;
            s.y2 = s.y1; s.y1 = y;
            data[i] = y;
        }
    }
}
#endif
