#include "VCPluginDSP.h"

//==============================================================================
// Construction / Destruction
//==============================================================================
VCPluginDSP::VCPluginDSP()
{
    mParams.delayTime = 250.0f;
    mParams.feedback = 30.0f;
    mParams.mix = 50.0f;
    mParams.enabled = true;
    mParams.syncBpm = 0.0f;
    mParams.noteValue = 4;
    mParams.triplet = false;
    mParams.dotted = false;
    mParams.taps = 1;
    mParams.pingPong = false;
    mParams.feedbackHpf = 80.0f;
    mParams.feedbackLpf = 8000.0f;
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

    // Initialize delay buffer: 5 seconds max (Gen2 needs more for multi-tap)
    mMaxDelaySamples = static_cast<int>(5.0 * sampleRate);
    mDelayBufferL.resize(mMaxDelaySamples, 0.0f);
    mDelayBufferR.resize(mMaxDelaySamples, 0.0f);

    // Initialize positions
    mWritePos = 0;

    // Resize internal buffer for stereo processing
    mInternalBuffer.resize(blockSize * 2);
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;

    // Initialize feedback filters
    mFeedbackFilterL.update(sampleRate, mParams.feedbackHpf, mParams.feedbackLpf);
    mFeedbackFilterR.update(sampleRate, mParams.feedbackHpf, mParams.feedbackLpf);
}

//==============================================================================
// Process interleaved stereo buffer
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

    // Convert non-interleaved to interleaved for internal processing
    if ((int)mInternalBuffer.size() < (int)block.getNumSamples() * 2)
        mInternalBuffer.resize((size_t)block.getNumSamples() * 2);

    float* leftBuf = mInternalBuffer.data();
    float* rightBuf = mInternalBuffer.data() + block.getNumSamples();

    for (size_t i = 0; i < block.getNumSamples(); ++i) {
        leftBuf[i] = block.getSample(0, i);
        rightBuf[i] = block.getSample(1, i);
    }

    processInternal(leftBuf, rightBuf, (int)block.getNumSamples());

    for (size_t i = 0; i < block.getNumSamples(); ++i) {
        block.setSample(0, i, leftBuf[i]);
        block.setSample(1, i, rightBuf[i]);
    }
}
#endif

//==============================================================================
// Internal Gen2 multi-tap delay processing with feedback filtering + ping-pong
//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    float wet = mParams.mix / 100.0f;
    float dry = 1.0f - wet;
    float fb = mParams.feedback / 100.0f;
    int numTaps = VC_JCLAMP(mParams.taps, 1, VC_DELAY_MAX_TAPS);
    bool isPingPong = mParams.pingPong;

    // Resolve each tap's delay time in samples
    int tapDelaySamples[VC_DELAY_MAX_TAPS];
    float tapGainLinear[VC_DELAY_MAX_TAPS];
    for (int t = 0; t < numTaps; ++t) {
        float delayMs = mParams.tapTime[t];
        // If BPM sync is on, calculate the first tap from BPM, others scale proportionally
        if (mParams.syncBpm > 0.0f && t == 0) {
            delayMs = calcDelayFromBPM(mParams.syncBpm, mParams.noteValue, mParams.triplet, mParams.dotted);
        }
        // Ensure minimum delay
        delayMs = VC_JMAX(delayMs, 1.0f);
        tapDelaySamples[t] = static_cast<int>(delayMs * 0.001f * mSampleRate);
        tapDelaySamples[t] = VC_JCLAMP(tapDelaySamples[t], 1, mMaxDelaySamples - 1);
        tapGainLinear[t] = VCStandalone::decibelsToGain(mParams.tapGain[t]);
    }

    // Update feedback filter coefficients (cheap to call each block)
    mFeedbackFilterL.update(mSampleRate, mParams.feedbackHpf, mParams.feedbackLpf);
    mFeedbackFilterR.update(mSampleRate, mParams.feedbackHpf, mParams.feedbackLpf);

    for (int i = 0; i < numSamples; ++i) {
        float inL = left[i];
        float inR = right[i];

        // Sum all tap outputs
        float wetL = 0.0f;
        float wetR = 0.0f;

        for (int t = 0; t < numTaps; ++t) {
            int readPos = (mWritePos - tapDelaySamples[t] + mMaxDelaySamples) % mMaxDelaySamples;
            float delL = mDelayBufferL[readPos];
            float delR = mDelayBufferR[readPos];
            wetL += delL * tapGainLinear[t];
            wetR += delR * tapGainLinear[t];
        }

        // Average tap gains for feedback to avoid feedback explosion with many taps
        float avgTapGain = 0.0f;
        for (int t = 0; t < numTaps; ++t) avgTapGain += tapGainLinear[t];
        avgTapGain /= (float)numTaps;

        // Feedback signal: filtered delay output * feedback
        float fbL = mFeedbackFilterL.process(wetL * fb * avgTapGain);
        float fbR = mFeedbackFilterR.process(wetR * fb * avgTapGain);

        // Ping-pong: cross channels in feedback
        if (isPingPong) {
            float tmp = fbL;
            fbL = fbR;
            fbR = tmp;
        }

        // Write to delay buffer: input + feedback
        mDelayBufferL[mWritePos] = inL + fbL;
        mDelayBufferR[mWritePos] = inR + fbR;

        // Ping-pong: alternate the wet signal between L/R
        if (isPingPong) {
            // Ping-pong effect: swap wet L/R for stereo spread
            float tmp = wetL;
            wetL = wetR * 0.7f + wetL * 0.3f;
            wetR = tmp * 0.7f + wetR * 0.3f;
        }

        // Output: dry + wet
        left[i] = dry * inL + wet * wetL;
        right[i] = dry * inR + wet * wetR;

        // Advance write pointer
        mWritePos = (mWritePos + 1) % mMaxDelaySamples;
    }
}

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
    std::fill(mDelayBufferL.begin(), mDelayBufferL.end(), 0.0f);
    std::fill(mDelayBufferR.begin(), mDelayBufferR.end(), 0.0f);
    mWritePos = 0;
    mFeedbackFilterL.reset();
    mFeedbackFilterR.reset();
}

//==============================================================================
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;

    // Update feedback filters if sample rate is set
    if (mSampleRate > 0) {
        mFeedbackFilterL.update(mSampleRate, mParams.feedbackHpf, mParams.feedbackLpf);
        mFeedbackFilterR.update(mSampleRate, mParams.feedbackHpf, mParams.feedbackLpf);
    }

    // If BPM sync is on, update the first tap time from BPM
    if (mParams.syncBpm > 0.0f) {
        mParams.tapTime[0] = calcDelayFromBPM(mParams.syncBpm, mParams.noteValue, mParams.triplet, mParams.dotted);
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
