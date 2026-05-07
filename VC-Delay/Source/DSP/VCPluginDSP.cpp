#include "VCPluginDSP.h"

//==============================================================================
// Construction / Destruction
//==============================================================================
VCPluginDSP::VCPluginDSP()
{
    // Initialize default parameters
    mParams.delayTime = 250.0f;
    mParams.feedback = 30.0f;
    mParams.mix = 50.0f;
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

    // Initialize delay buffer: 2 seconds max
    mMaxDelaySamples = static_cast<int>(2.0 * sampleRate);
    mDelayBuffer.resize(mMaxDelaySamples * 2, 0.0f);

    // Initialize positions
    mWritePos = 0;
    int delaySamples = static_cast<int>(mParams.delayTime * 0.001f * sampleRate);
    mReadPos = (mWritePos - delaySamples + mMaxDelaySamples) % mMaxDelaySamples;

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
void VCPluginDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled)
        return;

    // Use internal processing
    processInternal(left, right, numSamples);
}

//==============================================================================
// JUCE AudioBlock processing (non-interleaved)
//==============================================================================
#ifndef VC_STANDALONE
void VCPluginDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled)
        return;

    // Convert non-interleaved to interleaved for internal processing
    if ((int)mInternalBuffer.size() < (int)block.getNumSamples() * 2)
        mInternalBuffer.resize((size_t)block.getNumSamples() * 2);

    float* leftBuf = mInternalBuffer.data();
    float* rightBuf = mInternalBuffer.data() + block.getNumSamples();

    // Copy from AudioBlock (non-interleaved)
    for (size_t i = 0; i < block.getNumSamples(); ++i) {
        leftBuf[i] = block.getSample(0, i);
        rightBuf[i] = block.getSample(1, i);
    }

    // Process
    processInternal(leftBuf, rightBuf, (int)block.getNumSamples());

    // Copy back to AudioBlock
    for (size_t i = 0; i < block.getNumSamples(); ++i) {
        block.setSample(0, i, leftBuf[i]);
        block.setSample(1, i, rightBuf[i]);
    }
}
#endif

//==============================================================================
// Internal delay processing with circular buffer
//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    float wet = mParams.mix / 100.0f;
    float dry = 1.0f - wet;
    float fb = mParams.feedback / 100.0f;

    // Calculate delay in samples
    int delaySamples = static_cast<int>(mParams.delayTime * 0.001f * mSampleRate);
    delaySamples = VC_JCLAMP(delaySamples, 1, mMaxDelaySamples / 2 - 1);

    for (int i = 0; i < numSamples; ++i) {
        float inL = left[i];
        float inR = right[i];

        // Read from delay buffer
        float delL = mDelayBuffer[mReadPos * 2];
        float delR = mDelayBuffer[mReadPos * 2 + 1];

        // Write: input + feedback
        mDelayBuffer[mWritePos * 2] = inL + fb * delL;
        mDelayBuffer[mWritePos * 2 + 1] = inR + fb * delR;

        // Output: dry + wet
        left[i] = dry * inL + wet * delL;
        right[i] = dry * inR + wet * delR;

        // Advance pointers
        mWritePos = (mWritePos + 1) % mMaxDelaySamples;
        mReadPos = (mReadPos + 1) % mMaxDelaySamples;
    }
}

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
    // Clear delay buffer
    std::fill(mDelayBuffer.begin(), mDelayBuffer.end(), 0.0f);

    // Reset positions
    mWritePos = 0;
    int delaySamples = static_cast<int>(mParams.delayTime * 0.001f * mSampleRate);
    mReadPos = (mWritePos - delaySamples + mMaxDelaySamples) % mMaxDelaySamples;
}

//==============================================================================
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;

    // Update read position if delay time changed
    if (mMaxDelaySamples > 0) {
        int delaySamples = static_cast<int>(mParams.delayTime * 0.001f * mSampleRate);
        delaySamples = VC_JCLAMP(delaySamples, 1, mMaxDelaySamples / 2 - 1);
        mReadPos = (mWritePos - delaySamples + mMaxDelaySamples) % mMaxDelaySamples;
    }
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
