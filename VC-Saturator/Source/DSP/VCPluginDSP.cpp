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
VCSaturatorDSP::VCSaturatorDSP()
{
    mParams.drive = 0.0f;
    mParams.mix = 100.0f;
    mParams.algorithm = 0;
    mParams.enabled = true;
}

VCSaturatorDSP::~VCSaturatorDSP()
{
}

//==============================================================================
// Prepare DSP for processing
//==============================================================================
void VCSaturatorDSP::prepare(double sampleRate, int blockSize)
{
    mSampleRate = sampleRate;
    mBlockSize = blockSize;

    // Resize internal buffer for stereo processing
    mInternalBuffer.resize(blockSize * 2);

    // Create pointer array for AudioBlock
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;
}

//==============================================================================
// Process interleaved stereo buffer
//==============================================================================
void VCSaturatorDSP::process(float* left, float* right, int numSamples)
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
void VCSaturatorDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled)
        return;

    float wet = mParams.mix / 100.0f;
    float dry = 1.0f - wet;
    float driveGain = dBToLinear(mParams.drive);
    
    for (size_t ch = 0; ch < block.getNumChannels(); ++ch) {
        auto* data = block.getChannelPointer(ch);
        for (size_t i = 0; i < block.getNumSamples(); ++i) {
            float in = data[i];
            float processed = applySaturation(in, driveGain);
            data[i] = dry * in + wet * processed;
        }
    }
}
#endif

//==============================================================================
// Standalone internal processing
//==============================================================================
void VCSaturatorDSP::processInternal(float* left, float* right, int numSamples)
{
    float wet = mParams.mix / 100.0f;
    float dry = 1.0f - wet;
    float driveGain = dBToLinear(mParams.drive);
    
    float* channels[2] = { left, right };

    for (int ch = 0; ch < 2; ++ch) {
        float* data = channels[ch];
        for (int i = 0; i < numSamples; ++i) {
            float in = data[i];
            float processed = applySaturation(in, driveGain);
            data[i] = dry * in + wet * processed;
        }
    }
}

//==============================================================================
// Waveshaping functions
//==============================================================================
float VCSaturatorDSP::tapeSaturate(float x, float drive)
{
    // Tape saturation: tanh - soft, smooth saturation
    return std::tanh(x * drive);
}

float VCSaturatorDSP::tubeSaturate(float x, float drive)
{
    // Tube saturation: asymmetric, warmer
    float xd = x * drive;
    return xd / (1.0f + std::abs(xd));
}

float VCSaturatorDSP::hardClip(float x, float drive)
{
    // Hard clip: harsh, digital distortion
    return VC_JCLAMP(x * drive, -1.0f, 1.0f);
}

float VCSaturatorDSP::applySaturation(float x, float drive)
{
    switch (mParams.algorithm) {
        case 0: return tapeSaturate(x, drive);   // tape
        case 1: return tubeSaturate(x, drive);   // tube
        case 2: return hardClip(x, drive);        // clip
        default: return tapeSaturate(x, drive);   // default to tape
    }
}

//==============================================================================
// Reset processing state
//==============================================================================
void VCSaturatorDSP::reset()
{
    // No state to reset for pure waveshaping
}

//==============================================================================
// Set parameters
//==============================================================================
void VCSaturatorDSP::setParams(const Params& p)
{
    mParams = p;
}

//==============================================================================
// Get parameters
//==============================================================================
VCSaturatorDSP::Params VCSaturatorDSP::getParams() const
{
    return mParams;
}

//==============================================================================
// Set enabled state
//==============================================================================
void VCSaturatorDSP::setEnabled(bool enabled)
{
    mEnabled = enabled;
}
