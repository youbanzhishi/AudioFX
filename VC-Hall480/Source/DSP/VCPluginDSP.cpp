#include "VCPluginDSP.h"

//==============================================================================
// VC-Hall480 — Lexicon 480L-Class Nested Allpass Reverb Implementation
// Gardner Large Hall + LFO Modulation + Random Hall + Plate
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
    mParams.algorithm = 0;
    mParams.roomSize = 50.0f;
    mParams.decayTime = 2.0f;
    mParams.preDelay = 20.0f;
    mParams.diffusion = 70.0f;
    mParams.shape = 50.0f;
    mParams.spread = 80.0f;
    mParams.hiDecay = 0.5f;
    mParams.loDecay = 1.0f;
    mParams.chorusRate = 1.0f;
    mParams.chorusDepth = 30.0f;
    mParams.mix = 30.0f;
    mParams.wetLPF = 8000.0f;
    mParams.wetHPF = 200.0f;
    mParams.enabled = true;
}

VCPluginDSP::~VCPluginDSP()
{
}

//==============================================================================
// Calculate decay gain from T60 and delay length
// g = 10^(-3 * delaySamples / (sampleRate * T60))
//==============================================================================
float VCPluginDSP::calcDecayGain(float delaySamples)
{
    if (mT60 < 0.01f) return 0.0f;
    return std::pow(10.0f, -3.0f * delaySamples / (static_cast<float>(mSampleRate) * mT60));
}

//==============================================================================
// Update internal parameters from Params
//==============================================================================
void VCPluginDSP::updateParameters()
{
    //==========================================================================
    // Room size: (0-100) → 0.0-1.0 factor
    //==========================================================================
    mRoomSizeFactor = VC_JCLAMP(mParams.roomSize, 0.0f, 100.0f) / 100.0f;

    //==========================================================================
    // Decay time: directly used as T60
    //==========================================================================
    mT60 = VC_JCLAMP(mParams.decayTime, 0.3f, 20.0f);

    //==========================================================================
    // Feedback LP cutoff — controlled by hiDecay parameter
    // hiDecay < 1.0 = darker (lower cutoff), hiDecay > 1.0 = brighter
    //==========================================================================
    // Base cutoff around 5kHz, modulated by hiDecay
    // hiDecay=0.1 → ~800Hz, hiDecay=1.0 → ~5kHz, hiDecay=2.0 → ~12kHz
    float hiDecayNorm = VC_JCLAMP(mParams.hiDecay, 0.1f, 2.0f);
    mFeedbackLPCutoff = 5000.0f * std::pow(2.4f, hiDecayNorm - 1.0f);
    mFeedbackLPCutoff = VC_JCLAMP(mFeedbackLPCutoff, 500.0f, 16000.0f);

    float feedbackLPCoeff = 1.0f - std::exp(-2.0f * VC_PI * mFeedbackLPCutoff / static_cast<float>(mSampleRate));

    //==========================================================================
    // Lo-decay highpass — removes low-frequency buildup when loDecay < 1.0
    //==========================================================================
    float loDecayNorm = VC_JCLAMP(mParams.loDecay, 0.1f, 2.0f);
    // loDecay=0.1 → aggressive HP at ~300Hz, loDecay=1.0 → no HP, loDecay>1 → boost lows
    if (loDecayNorm < 1.0f) {
        float hpCutoff = 300.0f * std::pow(0.33f, loDecayNorm - 0.1f);
        mLoDecayHPCoeff = 1.0f - std::exp(-2.0f * VC_PI * hpCutoff / static_cast<float>(mSampleRate));
    } else {
        mLoDecayHPCoeff = 0.0f; // no highpass when loDecay >= 1.0
    }

    //==========================================================================
    // Pre-delay
    //==========================================================================
    mPreDelaySamples = static_cast<int>(VC_JCLAMP(mParams.preDelay, 0.0f, 200.0f) / 1000.0f * mSampleRate);
    mPreDelaySamples = VC_JCLAMP(mPreDelaySamples, 0, MAX_PREDELAY_SAMPLES - 1);

    //==========================================================================
    // Calculate feedback gains based on T60
    // Total delay through one pass of the chain
    //==========================================================================
    float srRatio = static_cast<float>(mSampleRate) / 44100.0f;
    float sizeScale = 0.5f + mRoomSizeFactor * 1.5f;
    float totalDelay = 0.0f;
    for (int i = 0; i < 3; ++i) {
        totalDelay += static_cast<float>(LateReverbChannel::BASE_AP_DELAYS[i]) * srRatio * sizeScale;
        totalDelay += static_cast<float>(LateReverbChannel::BASE_D_DELAYS[i]) * srRatio * sizeScale;
    }
    mTotalDelaySamples = totalDelay;

    // Feedback gain: after one full loop, gain should be 10^(-3 * totalDelay / (sr * T60))
    float fbGain = calcDecayGain(totalDelay);

    // Apply loDecay adjustment to feedback gain
    // loDecay < 1.0 reduces low-frequency energy → already handled by HPF
    // loDecay > 1.0 increases low-frequency energy → boost feedback slightly
    float loDecayGainComp = loDecayNorm >= 1.0f ? 1.0f + (loDecayNorm - 1.0f) * 0.3f : 1.0f;
    fbGain *= loDecayGainComp;

    mLateL.feedbackGain = fbGain;
    mLateR.feedbackGain = fbGain;
    mLateL.feedbackLPCoeff = feedbackLPCoeff;
    mLateR.feedbackLPCoeff = feedbackLPCoeff;

    //==========================================================================
    // Shape envelope parameters
    // shape=0 → sharp attack, fast decay (gated reverb)
    // shape=50 → balanced
    // shape=100 → slow attack, long sustain (reverse-like)
    //==========================================================================
    float shapeNorm = VC_JCLAMP(mParams.shape, 0.0f, 100.0f) / 100.0f;
    // Attack coefficient: shape=0 → fast (0.99), shape=100 → slow (0.5)
    mShapeAttack = 1.0f - std::pow(1.0f - shapeNorm, 2.0f) * 0.49f + 0.5f;
    // Decay coefficient: shape=0 → fast (0.95), shape=100 → slow (0.999)
    mShapeDecay = 0.95f + shapeNorm * 0.049f;

    //==========================================================================
    // Cross-coupling gain (stereo spread)
    // spread=0 → mono (cross=0.0), spread=100 → wide (cross=0.5)
    //==========================================================================
    float spreadNorm = VC_JCLAMP(mParams.spread, 0.0f, 100.0f) / 100.0f;
    mCrossGain = spreadNorm * 0.5f;

    //==========================================================================
    // Post-reverb wet signal filter coefficients
    //==========================================================================
    float wetLPCutoff = VC_JCLAMP(mParams.wetLPF, 1000.0f, 16000.0f);
    mWetLPCoeff = 1.0f - std::exp(-2.0f * VC_PI * wetLPCutoff / static_cast<float>(mSampleRate));

    float wetHPCutoff = VC_JCLAMP(mParams.wetHPF, 20.0f, 500.0f);
    mWetHPCoeff = 1.0f - std::exp(-2.0f * VC_PI * wetHPCutoff / static_cast<float>(mSampleRate));

    //==========================================================================
    // Random Hall depth
    //==========================================================================
    float randomDepth = (mParams.algorithm == 1) ? 0.8f : 0.0f;
    mLateL.randomDepth = randomDepth;
    mLateR.randomDepth = randomDepth;
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
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;

    // Resize pre-delay buffers
    mPreDelayBuffer[0].assign(MAX_PREDELAY_SAMPLES, 0.0f);
    mPreDelayBuffer[1].assign(MAX_PREDELAY_SAMPLES, 0.0f);
    mPreDelayWritePos = 0;

    // Update parameters first (needed for roomSize, chorusRate, chorusDepth)
    updateParameters();

    // Initialize late reverb engines (stereo pair)
    ReverbAlgorithm algo = static_cast<ReverbAlgorithm>(VC_JCLAMP(mParams.algorithm, 0, 2));
    float chorusRate = VC_JCLAMP(mParams.chorusRate, 0.0f, 5.0f);
    float chorusDepth = VC_JCLAMP(mParams.chorusDepth, 0.0f, 100.0f);
    float randomDepth = (algo == ReverbAlgorithm::RandomHall) ? 0.8f : 0.0f;

    mLateL.init(false, sampleRate, mRoomSizeFactor, chorusRate, chorusDepth, randomDepth);
    mLateR.init(true, sampleRate, mRoomSizeFactor, chorusRate, chorusDepth, randomDepth);

    // Initialize early reflections
    mEarlyRef.configure(algo, mRoomSizeFactor, mParams.diffusion, sampleRate);

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
    if (static_cast<int>(mInternalBuffer.size()) < numSamples * 2)
        mInternalBuffer.resize(numSamples * 2);

    float* leftBuf = mInternalBuffer.data();
    float* rightBuf = mInternalBuffer.data() + numSamples;

    for (int i = 0; i < numSamples; ++i) {
        leftBuf[i] = left[i];
        rightBuf[i] = right[i];
    }

    mInternalPtrs[0] = leftBuf;
    mInternalPtrs[1] = rightBuf;

    juce::dsp::AudioBlock<float> block(mInternalPtrs.data(), 2, static_cast<juce::uint32>(numSamples));
    process(block);

    for (int i = 0; i < numSamples; ++i) {
        left[i] = leftBuf[i];
        right[i] = rightBuf[i];
    }
#endif
}

//==============================================================================
// JUCE AudioBlock processing
//==============================================================================
#ifndef VC_STANDALONE
void VCPluginDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled)
        return;

    float* left = block.getChannelPointer(0);
    float* right = block.getChannelPointer(1);
    int numSamples = static_cast<int>(block.getNumSamples());

    processInternal(left, right, numSamples);
}
#endif

//==============================================================================
// Internal processing — the heart of the 480L-class reverb
//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    updateParameters();

    float mixFactor = VC_JCLAMP(mParams.mix, 0.0f, 100.0f) / 100.0f;
    ReverbAlgorithm algo = static_cast<ReverbAlgorithm>(VC_JCLAMP(mParams.algorithm, 0, 2));

    // Scale factor for late reverb output
    const float lateScale = 0.7f;
    // Early reflections level (reduced for Plate algorithm)
    float earlyLevel = (algo == ReverbAlgorithm::Plate) ? 0.1f : 0.35f;

    for (int i = 0; i < numSamples; ++i) {
        float inL = left[i];
        float inR = right[i];

        // Store dry signal
        float dryL = inL;
        float dryR = inR;

        // Mono sum for reverb input (typical for algorithmic reverbs)
        float monoIn = (inL + inR) * 0.5f;

        //==============================================================
        // Pre-delay
        //==============================================================
        float predelayOut = 0.0f;
        if (mPreDelaySamples > 0) {
            int readPos = (mPreDelayWritePos - mPreDelaySamples + MAX_PREDELAY_SAMPLES)
                          % MAX_PREDELAY_SAMPLES;
            predelayOut = mPreDelayBuffer[0][readPos]; // use single mono buffer
        }
        mPreDelayBuffer[0][mPreDelayWritePos] = monoIn;
        mPreDelayWritePos = (mPreDelayWritePos + 1) % MAX_PREDELAY_SAMPLES;

        float delayedInput = (mPreDelaySamples > 0) ? predelayOut : monoIn;

        //==============================================================
        // Early reflections (480L-style diffused clusters)
        //==============================================================
        float earlyL = 0.0f, earlyR = 0.0f;
        mEarlyRef.process(delayedInput, earlyL, earlyR);

        //==============================================================
        // Late reverb — nested allpass ring with cross-coupling
        //==============================================================
        // Input to late reverb: mix of dry input, early reflections, and feedback
        float lateInL = delayedInput * 0.5f + (earlyL + earlyR) * 0.25f + mFeedbackL;
        float lateInR = delayedInput * 0.5f + (earlyL + earlyR) * 0.25f + mFeedbackR;

        // Process through left and right reverb chains
        float lateOutL = mLateL.process(lateInL, mSampleRate);
        float lateOutR = mLateR.process(lateInR, mSampleRate);

        // Global feedback with cross-coupling (Householder-inspired)
        // Feed back: L output → R input, R output → L input
        float fbL = lateOutL * (1.0f - mCrossGain) + lateOutR * mCrossGain;
        float fbR = lateOutR * (1.0f - mCrossGain) + lateOutL * mCrossGain;

        // Apply feedback damping (1-pole LP in feedback path)
        float fbL_processed = mLateL.processFeedback(fbL);
        float fbR_processed = mLateR.processFeedback(fbR);

        // Apply lo-decay highpass to feedback
        if (mLoDecayHPCoeff > 0.0f) {
            mLoDecayHPState[0] = mLoDecayHPCoeff * fbL_processed + (1.0f - mLoDecayHPCoeff) * mLoDecayHPState[0];
            fbL_processed = fbL_processed - mLoDecayHPState[0];
            mLoDecayHPState[1] = mLoDecayHPCoeff * fbR_processed + (1.0f - mLoDecayHPCoeff) * mLoDecayHPState[1];
            fbR_processed = fbR_processed - mLoDecayHPState[1];
        }

        // Store feedback for next-sample injection (completes the feedback loop)
        mFeedbackL = fbL_processed;
        mFeedbackR = fbR_processed;

        // Late reverb output with scaling
        float wetL = earlyL * earlyLevel + lateOutL * lateScale;
        float wetR = earlyR * earlyLevel + lateOutR * lateScale;

        //==============================================================
        // Shape envelope (dynamics shaping of wet signal)
        //==============================================================
        float wetAbsL = std::abs(wetL);
        float wetAbsR = std::abs(wetR);

        // Envelope follower: attack controls how quickly envelope rises
        if (wetAbsL > mEnvelopeState[0])
            mEnvelopeState[0] += mShapeAttack * (wetAbsL - mEnvelopeState[0]);
        else
            mEnvelopeState[0] += mShapeDecay * (wetAbsL - mEnvelopeState[0]);

        if (wetAbsR > mEnvelopeState[1])
            mEnvelopeState[1] += mShapeAttack * (wetAbsR - mEnvelopeState[1]);
        else
            mEnvelopeState[1] += mShapeDecay * (wetAbsR - mEnvelopeState[1]);

        // Shape modulation: at shape=100 (slow attack), initial transients are attenuated
        // creating a "reverse reverb" effect; at shape=0, transients are emphasized
        float shapeNorm = VC_JCLAMP(mParams.shape, 0.0f, 100.0f) / 100.0f;
        // Modulate wet signal by envelope ratio
        float envRatioL = (mEnvelopeState[0] > 1e-6f) ? wetAbsL / mEnvelopeState[0] : 1.0f;
        float envRatioR = (mEnvelopeState[1] > 1e-6f) ? wetAbsR / mEnvelopeState[1] : 1.0f;
        // Blend between shaped (shape=100) and unshaped (shape=0)
        float shapeBlend = 1.0f - shapeNorm * 0.6f; // mild shaping
        float shapedWetL = wetL * (shapeBlend + (1.0f - shapeBlend) * envRatioL);
        float shapedWetR = wetR * (shapeBlend + (1.0f - shapeBlend) * envRatioR);

        //==============================================================
        // Wet signal post-filtering
        //==============================================================
        // Lowpass: high-cut
        mWetLPFState[0] = mWetLPCoeff * shapedWetL + (1.0f - mWetLPCoeff) * mWetLPFState[0];
        mWetLPFState[1] = mWetLPCoeff * shapedWetR + (1.0f - mWetLPCoeff) * mWetLPFState[1];
        float wetFilteredL = mWetLPFState[0];
        float wetFilteredR = mWetLPFState[1];

        // Highpass: low-cut
        mWetHPFState[0] = mWetHPCoeff * wetFilteredL + (1.0f - mWetHPCoeff) * mWetHPFState[0];
        mWetHPFState[1] = mWetHPCoeff * wetFilteredR + (1.0f - mWetHPCoeff) * mWetHPFState[1];
        wetFilteredL = wetFilteredL - mWetHPFState[0];
        wetFilteredR = wetFilteredR - mWetHPFState[1];

        //==============================================================
        // Dry/Wet mix + soft-clip protection
        //==============================================================
        float outL = dryL * (1.0f - mixFactor) + wetFilteredL * mixFactor;
        float outR = dryR * (1.0f - mixFactor) + wetFilteredR * mixFactor;

        // Soft-clip to prevent digital clipping (tanh saturation)
        outL = std::tanh(outL);
        outR = std::tanh(outR);

        left[i] = outL;
        right[i] = outR;
    }
}

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
    mLateL.clear();
    mLateR.clear();
    mEarlyRef.clear();

    std::fill(mPreDelayBuffer[0].begin(), mPreDelayBuffer[0].end(), 0.0f);
    std::fill(mPreDelayBuffer[1].begin(), mPreDelayBuffer[1].end(), 0.0f);
    mPreDelayWritePos = 0;

    mWetLPFState[0] = 0.0f; mWetLPFState[1] = 0.0f;
    mWetHPFState[0] = 0.0f; mWetHPFState[1] = 0.0f;
    mLoDecayHPState[0] = 0.0f; mLoDecayHPState[1] = 0.0f;
    mEnvelopeState[0] = 0.0f; mEnvelopeState[1] = 0.0f;
    mFeedbackL = 0.0f; mFeedbackR = 0.0f;

    updateParameters();
}

//==============================================================================
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    bool algoChanged = (mParams.algorithm != p.algorithm);
    bool roomChanged = (mParams.roomSize != p.roomSize);
    bool chorusChanged = (mParams.chorusRate != p.chorusRate || mParams.chorusDepth != p.chorusDepth);

    mParams = p;
    mEnabled = p.enabled;

    // Re-initialize late reverb if algorithm, room size, or chorus params changed
    if (algoChanged || roomChanged || chorusChanged) {
        ReverbAlgorithm algo = static_cast<ReverbAlgorithm>(VC_JCLAMP(mParams.algorithm, 0, 2));
        float chorusRate = VC_JCLAMP(mParams.chorusRate, 0.0f, 5.0f);
        float chorusDepth = VC_JCLAMP(mParams.chorusDepth, 0.0f, 100.0f);
        float randomDepth = (algo == ReverbAlgorithm::RandomHall) ? 0.8f : 0.0f;

        mLateL.init(false, mSampleRate, mRoomSizeFactor, chorusRate, chorusDepth, randomDepth);
        mLateR.init(true, mSampleRate, mRoomSizeFactor, chorusRate, chorusDepth, randomDepth);
        mEarlyRef.configure(algo, mRoomSizeFactor, mParams.diffusion, mSampleRate);
    }

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
