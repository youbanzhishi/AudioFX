//==============================================================================
// VC-Gate DSP Core Implementation - Noise Gate / Downward Expander (Gen2)
//
// Gen2 Algorithm overview:
//   1. Sidechain HPF filters low-frequency rumble from the detection signal
//   2. RMS envelope follower detects filtered input level
//   3. Hysteresis: separate open/close thresholds prevent gate chattering
//      - Open threshold = threshold
//      - Close threshold = threshold - hysteresis
//   4. Attack-hold: minimum time the gate stays open after attack
//   5. Lookahead buffer: delays audio so gate can react before transients
//   6. Range control: adjustable gate depth
//   7. Expansion: for signals below threshold, gain is computed as:
//        gain_dB = range * (1 - gate_gain) * expansionFactor
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
    mEnvelopeFollower.setAttackTime(mParams.attack, static_cast<float>(sampleRate));
    mEnvelopeFollower.setReleaseTime(mParams.release, static_cast<float>(sampleRate));

    // Gen2: Initialize sidechain HPF
    mSidechainHPF.reset();
    mSidechainHPF.setCutoff(mParams.sidechainHpf, static_cast<float>(sampleRate));

    // Gen2: Initialize lookahead buffer
    mLookahead.prepare(mParams.lookahead, sampleRate);
    mLookahead.reset();

    // Initialize gate state
    mGateGain = 0.0f;
    mHoldCounter = 0;
    mAttackHoldCounter = 0;
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
// Internal gate processing (Gen2)
//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    // Convert parameters to linear/amenable forms
    // Gen2: Hysteresis - separate open and close thresholds
    float openThresholdLin = dBToLinear(mParams.threshold);
    float closeThresholdLin = dBToLinear(mParams.threshold - mParams.hysteresis);
    float rangeLin = dBToLinear(mParams.range);
    float ratio = mParams.ratio;

    // Hold in samples
    int holdSamples = static_cast<int>(mParams.hold * 0.001f * static_cast<float>(mSampleRate));
    // Gen2: Attack-hold in samples
    int attackHoldSamples = static_cast<int>(mParams.attackHold * 0.001f * static_cast<float>(mSampleRate));

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = left[i];
        float inR = right[i];

        // 1. Gen2: Sidechain HPF - filter low-frequency content from detection
        float scL = mSidechainHPF.processSample(inL);
        float scR = mSidechainHPF.processSample(inR);

        // 2. Envelope detection: use filtered signal for stereo detection
        float envL = mEnvelopeFollower.processSample(scL);
        float envR = mEnvelopeFollower.processSample(scR);
        float envMax = VC_JMAX(envL, envR);
        float envelopeLin = std::sqrt(envMax);

        // 3. Gen2: Hysteresis gate logic
        // If gate is closed, use openThreshold to decide if it should open
        // If gate is open, use closeThreshold to decide if it should close
        if (!mGateOpen)
        {
            // Gate is closed - check open threshold
            if (envelopeLin >= openThresholdLin)
            {
                mGateOpen = true;
                mHoldCounter = holdSamples;
                mAttackHoldCounter = attackHoldSamples;  // Gen2: start attack-hold
            }
        }
        else
        {
            // Gate is open - check close threshold (lower due to hysteresis)
            if (envelopeLin < closeThresholdLin)
            {
                // Signal below close threshold
                if (mAttackHoldCounter > 0)
                {
                    // Gen2: still in attack-hold period, keep open
                    mAttackHoldCounter--;
                }
                else if (mHoldCounter > 0)
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
            else
            {
                // Signal still above close threshold: reset hold timers
                mHoldCounter = holdSamples;
                mAttackHoldCounter = attackHoldSamples;
            }
        }

        // 4. Smooth gate gain based on state
        float targetGain = mGateOpen ? 1.0f : 0.0f;

        if (mGateGain < targetGain)
        {
            // Opening: use attack time (fast)
            mGateGain = mAttackCoefInv * targetGain + mAttackCoef * mGateGain;
            if (mGateGain > targetGain) mGateGain = targetGain;
        }
        else if (mGateGain > targetGain)
        {
            // Closing: use release time
            mGateGain = mReleaseCoefInv * targetGain + mReleaseCoef * mGateGain;
            if (mGateGain < targetGain) mGateGain = targetGain;
        }

        // 5. Compute gain reduction with ratio (downward expansion)
        float expansionFactor = 1.0f - 1.0f / ratio;
        float gainDB = mParams.range * (1.0f - mGateGain) * expansionFactor;
        float gainLin = dBToLinear(gainDB);

        // 6. Gen2: Lookahead - delay the audio to align with the gate decision
        float delayedL = inL;
        float delayedR = inR;
        if (mLookahead.getSize() > 1)
        {
            mLookahead.process(delayedL, delayedR);
        }

        // 7. Apply gain to the delayed signal
        left[i] = delayedL * gainLin;
        right[i] = delayedR * gainLin;
    }
}

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
    mEnvelopeFollower.reset();
    mSidechainHPF.reset();
    mLookahead.reset();
    mGateGain = 0.0f;
    mHoldCounter = 0;
    mAttackHoldCounter = 0;
    mGateOpen = false;
}

//==============================================================================
// Set parameters - update coefficients when timing parameters change
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;

    if (mSampleRate > 0.0)
    {
        mEnvelopeFollower.setAttackTime(mParams.attack, static_cast<float>(mSampleRate));
        mEnvelopeFollower.setReleaseTime(mParams.release, static_cast<float>(mSampleRate));

        // Gen2: Update sidechain HPF
        mSidechainHPF.setCutoff(mParams.sidechainHpf, static_cast<float>(mSampleRate));

        // Gen2: Update lookahead buffer if size changed
        mLookahead.prepare(mParams.lookahead, mSampleRate);

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
