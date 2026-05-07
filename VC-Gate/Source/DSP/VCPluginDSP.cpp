//==============================================================================
// VC-Gate DSP Core Implementation - Noise Gate / Downward Expander
//
// Algorithm overview:
//   1. RMS envelope follower detects input level
//   2. Compare envelope (in dB) against threshold
//   3. If above threshold: gate opens (gain ramps toward 0dB via attack)
//   4. If below threshold: hold timer starts, then gate closes (gain ramps toward range via release)
//   5. Expansion: for signals below threshold, gain is computed as:
//        gain_dB = range * (1 - gate_gain)
//      where gate_gain is 1.0 (open, no reduction) to 0.0 (closed, full range attenuation)
//   6. With ratio > 1, the transition is gradual (expander), not a hard switch
//==============================================================================

#include "VCPluginDSP.h"

#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#endif

//==============================================================================
// Construction / Destruction
//==============================================================================
VCPluginDSP::VCPluginDSP()
{
    // Default parameters are set in Params struct initializer
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

    // Reset envelope follower
    mEnvelopeFollower.reset();
    // CRITICAL: Initialize envelope coefficients in prepare() to avoid
    // the VC-Comp bug where releaseCoef was 0 before first process() call
    mEnvelopeFollower.setAttackTime(mParams.attack, static_cast<float>(sampleRate));
    mEnvelopeFollower.setReleaseTime(mParams.release, static_cast<float>(sampleRate));

    // Initialize gate state
    mGateGain = 0.0f;
    mHoldCounter = 0;
    mGateOpen = false;

    // Initialize attack/release smoothing coefficients
    mAttackCoef = std::exp(-1.0f / (mParams.attack * 0.001f * static_cast<float>(sampleRate)));
    mAttackCoefInv = 1.0f - mAttackCoef;
    mReleaseCoef = std::exp(-1.0f / (mParams.release * 0.001f * static_cast<float>(sampleRate)));
    mReleaseCoefInv = 1.0f - mReleaseCoef;

    // Resize internal buffer for stereo processing (AudioBlock conversion)
    mInternalBuffer.resize(blockSize * 2);
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;
}

//==============================================================================
// Process stereo buffer
//==============================================================================
void VCPluginDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled)
        return;

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

    auto numSamples = static_cast<int>(block.getNumSamples());

    if ((int)mInternalBuffer.size() < numSamples * 2)
        mInternalBuffer.resize(static_cast<size_t>(numSamples) * 2);

    float* leftBuf = mInternalBuffer.data();
    float* rightBuf = mInternalBuffer.data() + numSamples;

    for (int i = 0; i < numSamples; ++i) {
        leftBuf[i] = block.getSample(0, static_cast<size_t>(i));
        rightBuf[i] = block.getSample(1, static_cast<size_t>(i));
    }

    processInternal(leftBuf, rightBuf, numSamples);

    for (int i = 0; i < numSamples; ++i) {
        block.setSample(0, static_cast<size_t>(i), leftBuf[i]);
        block.setSample(1, static_cast<size_t>(i), rightBuf[i]);
    }
}
#endif

//==============================================================================
// Internal gate processing
//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    // Convert parameters to linear/amenable forms
    float thresholdLin = dBToLinear(mParams.threshold);
    float rangeLin = dBToLinear(mParams.range);  // e.g., -80dB -> very small number
    float ratio = mParams.ratio;

    // Hold in samples
    int holdSamples = static_cast<int>(mParams.hold * 0.001f * static_cast<float>(mSampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = left[i];
        float inR = right[i];

        // 1. Envelope detection: use max of L/R for stereo detection
        float envL = mEnvelopeFollower.processSample(inL);
        float envR = mEnvelopeFollower.processSample(inR);
        float envMax = VC_JMAX(envL, envR);
        // envMax is squared RMS, take sqrt to get linear envelope
        float envelopeLin = std::sqrt(envMax);

        // 2. Compare with threshold
        if (envelopeLin >= thresholdLin)
        {
            // Signal above threshold: gate should open
            mGateOpen = true;
            mHoldCounter = holdSamples;  // Reset hold timer
        }
        else
        {
            // Signal below threshold
            if (mHoldCounter > 0)
            {
                // In hold period: gate stays open
                mHoldCounter--;
            }
            else
            {
                // Hold expired: gate should close
                mGateOpen = false;
            }
        }

        // 3. Smooth gate gain based on state
        float targetGain = mGateOpen ? 1.0f : 0.0f;

        if (mGateGain < targetGain)
        {
            // Opening: use attack time
            mGateGain = mAttackCoefInv * targetGain + mAttackCoef * mGateGain;
            // Prevent overshoot
            if (mGateGain > targetGain) mGateGain = targetGain;
        }
        else if (mGateGain > targetGain)
        {
            // Closing: use release time
            mGateGain = mReleaseCoefInv * targetGain + mReleaseCoef * mGateGain;
            // Prevent undershoot
            if (mGateGain < targetGain) mGateGain = targetGain;
        }

        // 4. Compute gain reduction with ratio (downward expansion)
        // gate_gain = 1.0 -> no reduction (0dB)
        // gate_gain = 0.0 -> full range attenuation
        // With ratio: when gate is partially open, the expansion curve is:
        //   For signal below threshold by x dB:
        //     reduction = x * (1 - 1/ratio) when fully closed
        // But we implement it simpler with the range-based approach:
        //   gain_dB = range * (1 - gate_gain)
        // When ratio = 1: no expansion (bypass)
        // When ratio > 1: expansion kicks in proportional to (1 - 1/ratio)
        float expansionFactor = 1.0f - 1.0f / ratio;
        float gainDB = mParams.range * (1.0f - mGateGain) * expansionFactor;
        float gainLin = dBToLinear(gainDB);

        // 5. Apply gain
        left[i] = inL * gainLin;
        right[i] = inR * gainLin;
    }
}

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
    mEnvelopeFollower.reset();
    mGateGain = 0.0f;
    mHoldCounter = 0;
    mGateOpen = false;
}

//==============================================================================
// Set parameters - update coefficients when timing parameters change
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;

    // Update envelope follower coefficients
    if (mSampleRate > 0.0)
    {
        mEnvelopeFollower.setAttackTime(mParams.attack, static_cast<float>(mSampleRate));
        // Use release for both envelope follower and gate smoothing
        mEnvelopeFollower.setReleaseTime(mParams.release, static_cast<float>(mSampleRate));

        // Update gate gain smoothing coefficients
        mAttackCoef = std::exp(-1.0f / (mParams.attack * 0.001f * static_cast<float>(mSampleRate)));
        mAttackCoefInv = 1.0f - mAttackCoef;
        mReleaseCoef = std::exp(-1.0f / (mParams.release * 0.001f * static_cast<float>(mSampleRate)));
        mReleaseCoefInv = 1.0f - mReleaseCoef;
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
