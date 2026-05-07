#include "VCPluginDSP.h"

//==============================================================================
// Construction / Destruction
//==============================================================================
VCPluginDSP::VCPluginDSP()
{
    // Initialize default parameters
    mParams.threshold = -6.0f;
    mParams.ceiling = -0.3f;
    mParams.release = 50.0f;
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

    // Reset envelope
    mEnvelope = 0.0f;

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
    // Bypass: if disabled or params say bypass, pass through unchanged
    if (!mEnabled || !mParams.enabled)
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
// Internal limiter processing
//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    float wet = mParams.mix / 100.0f;
    float dry = 1.0f - wet;
    float threshGain = dBToLinear(mParams.threshold);
    float ceilingGain = dBToLinear(mParams.ceiling);

    // Release coefficient: std::exp(-1.0 / (release_ms * 0.001 * sampleRate))
    float releaseCoeff = std::exp(-1.0f / (mParams.release * 0.001f * (float)mSampleRate));

    // Attack coefficient: near-instant (0.1ms)
    float attackCoeff = std::exp(-1.0f / (0.0001f * (float)mSampleRate));

    for (int i = 0; i < numSamples; ++i) {
        float inL = left[i];
        float inR = right[i];

        // Peak detection (stereo link)
        float peak = VC_JMAX(std::abs(inL), std::abs(inR));

        // Smooth envelope follower
        if (peak > mEnvelope) {
            mEnvelope = attackCoeff * mEnvelope + (1.0f - attackCoeff) * peak;
        } else {
            mEnvelope = releaseCoeff * mEnvelope + (1.0f - releaseCoeff) * peak;
        }

        // Calculate gain reduction
        float gainReduction = 1.0f;
        if (mEnvelope > threshGain) {
            gainReduction = threshGain / mEnvelope;
        }

        // Apply gain reduction and scale to ceiling
        float makeupGain = ceilingGain / threshGain;
        float outputL = inL * gainReduction * makeupGain;
        float outputR = inR * gainReduction * makeupGain;

        // Final safety clamp to ceiling
        outputL = VC_JCLAMP(outputL, -ceilingGain, ceilingGain);
        outputR = VC_JCLAMP(outputR, -ceilingGain, ceilingGain);

        // Dry/Wet mix
        left[i] = dry * inL + wet * outputL;
        right[i] = dry * inR + wet * outputR;
    }
}

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
    mEnvelope = 0.0f;
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
