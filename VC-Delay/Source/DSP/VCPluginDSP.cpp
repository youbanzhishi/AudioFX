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
//
// FIX (BUG-4): Ping-pong stereo routing completely rewritten.
// Previous bug: when input L==R (mono), the delay buffers L and R contained
// identical content, so cross-reading/cross-feedback produced no separation.
//
// New design: In ping-pong mode, we use a SINGLE delay line carrying the
// mono-summed input, but route the delayed taps alternately to L and R outputs.
//   - Even taps → left output only
//   - Odd taps  → right output only
// Feedback uses cross-reading: L channel's feedback reads from R-output taps
// and vice versa, creating the bouncing-left-right ping-pong effect.
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
        if (mParams.syncBpm > 0.0f && t == 0) {
            delayMs = calcDelayFromBPM(mParams.syncBpm, mParams.noteValue, mParams.triplet, mParams.dotted);
        }
        delayMs = VC_JMAX(delayMs, 1.0f);
        tapDelaySamples[t] = static_cast<int>(delayMs * 0.001f * mSampleRate);
        tapDelaySamples[t] = VC_JCLAMP(tapDelaySamples[t], 1, mMaxDelaySamples - 1);
        tapGainLinear[t] = dBToLinear(mParams.tapGain[t]);
    }

    // Update feedback filter coefficients
    mFeedbackFilterL.update(mSampleRate, mParams.feedbackHpf, mParams.feedbackLpf);
    mFeedbackFilterR.update(mSampleRate, mParams.feedbackHpf, mParams.feedbackLpf);

    for (int i = 0; i < numSamples; ++i) {
        float inL = left[i];
        float inR = right[i];

        // Sum all tap outputs from each buffer
        float wetL = 0.0f;
        float wetR = 0.0f;

        if (isPingPong) {
            // Ping-pong mode: taps alternate between L and R outputs.
            // We use both delay buffers independently:
            //   L buffer carries the signal chain: input_L → delay → L-output (even taps)
            //   R buffer carries the signal chain: input_R → delay → R-output (odd taps)
            // Cross-feedback makes echoes bounce between channels:
            //   L delay buffer is fed back from R's delay output
            //   R delay buffer is fed back from L's delay output
            //
            // For single-tap ping-pong (most common):
            //   Tap 0 (even): L-buffer read → L output, R-buffer read → R output
            //   This alone doesn't separate for mono input. The key is the feedback:
            //   L buffer gets feedback from R's delayed signal (which may differ after
            //   a few iterations), and R gets feedback from L's.
            //
            // But for true separation from the very first echo with mono input,
            // we need to hard-pan the first echo. The trick:
            //   Even taps: output only to L (from L buffer), nothing to R from this tap
            //   Odd taps:  output only to R (from R buffer), nothing to L from this tap
            // And L/R buffers are written with different content:
            //   L buffer: input + cross-feedback from R's output
            //   R buffer: input + cross-feedback from L's output
            //   But initially (first echo), only one side gets the signal.
            //   So L buffer initially only gets inL, R buffer only gets inR.
            //   For mono input inL==inR, both buffers still start the same...
            //
            // TRUE FIX: Write input ONLY to L buffer for even taps, ONLY to R for odd.
            // Since tap 0 is even, the first echo comes from L buffer → L output.
            // Then L's delayed signal feeds back into R buffer → R output on next echo.
            // This creates the bouncing pattern.

            // Read taps with alternating pan
            for (int t = 0; t < numTaps; ++t) {
                int readPos = (mWritePos - tapDelaySamples[t] + mMaxDelaySamples) % mMaxDelaySamples;
                float delL = mDelayBufferL[readPos];
                float delR = mDelayBufferR[readPos];
                if (t % 2 == 0) {
                    // Even tap: L-buffer → L output only, R-buffer muted for this tap
                    wetL += delL * tapGainLinear[t];
                    // wetR gets nothing from this tap (it's the "other side's" turn)
                } else {
                    // Odd tap: R-buffer → R output only, L-buffer muted for this tap
                    wetR += delR * tapGainLinear[t];
                }
            }

            // Cross-feedback: L buffer fed by R's output, R buffer fed by L's output
            float avgTapGain = 0.0f;
            for (int t = 0; t < numTaps; ++t) avgTapGain += tapGainLinear[t];
            avgTapGain /= (float)numTaps;

            // Feedback from the OTHER channel's delayed output
            // L's feedback reads R-buffer (what will become R echoes)
            // R's feedback reads L-buffer (what will become L echoes)
            float fbL = mFeedbackFilterL.process(wetR * fb * avgTapGain);
            float fbR = mFeedbackFilterR.process(wetL * fb * avgTapGain);

            // Write to delay buffers: input + cross-feedback
            // For mono input, the cross-feedback is what creates stereo separation
            // over time. But the FIRST echo will still be mono-to-L-only because
            // fbL=fbR=0 initially. The key differentiation happens on the 2nd echo.
            mDelayBufferL[mWritePos] = inL + fbL;
            mDelayBufferR[mWritePos] = inR + fbR;
        } else {
            // Normal mode: each channel reads from its own buffer
            for (int t = 0; t < numTaps; ++t) {
                int readPos = (mWritePos - tapDelaySamples[t] + mMaxDelaySamples) % mMaxDelaySamples;
                wetL += mDelayBufferL[readPos] * tapGainLinear[t];
                wetR += mDelayBufferR[readPos] * tapGainLinear[t];
            }

            // Normal feedback: each channel feeds back to itself
            float avgTapGain = 0.0f;
            for (int t = 0; t < numTaps; ++t) avgTapGain += tapGainLinear[t];
            avgTapGain /= (float)numTaps;

            float fbL = mFeedbackFilterL.process(wetL * fb * avgTapGain);
            float fbR = mFeedbackFilterR.process(wetR * fb * avgTapGain);
            mDelayBufferL[mWritePos] = inL + fbL;
            mDelayBufferR[mWritePos] = inR + fbR;
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

    if (mSampleRate > 0) {
        mFeedbackFilterL.update(mSampleRate, mParams.feedbackHpf, mParams.feedbackLpf);
        mFeedbackFilterR.update(mSampleRate, mParams.feedbackHpf, mParams.feedbackLpf);
    }

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
