#include "VCPluginDSP.h"

//==============================================================================
// VC-Reverb Gen 2 — FDN Implementation
// 8-delay-line Feedback Delay Network with Householder matrix
// Early reflections via simplified image method
// Frequency-dependent decay via 1-pole LP in feedback path
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
    // Initialize default parameters (Gen 1 compatible)
    mParams.roomSize = 50.0f;
    mParams.decay = 50.0f;
    mParams.damping = 50.0f;
    mParams.preDelay = 20.0f;
    mParams.mix = 30.0f;
    mParams.enabled = true;
}

VCPluginDSP::~VCPluginDSP()
{
}

//==============================================================================
// Calculate scaled delay line size based on room size
// Scale factor: 0.5 to 2.0 of base delay lengths
//==============================================================================
int VCPluginDSP::calcFDNDelaySize(int baseDelay)
{
    float scale = 0.5f + mRoomSizeFactor * 1.5f;  // 0.5 to 2.0
    int scaled = static_cast<int>(baseDelay * scale + 0.5f);
    // Ensure minimum delay of 1 sample
    return std::max(scaled, 1);
}

//==============================================================================
// Update internal parameters from Params
//==============================================================================
void VCPluginDSP::updateParameters()
{
    //==========================================================================
    // Room size mapping: (1-100) -> 0.01-1.0 factor -> room meters (2-30m)
    //==========================================================================
    mRoomSizeFactor = VC_JCLAMP(mParams.roomSize, 1.0f, 100.0f) / 100.0f;
    mRoomSizeMeters = 2.0f + mRoomSizeFactor * 28.0f;  // 2m to 30m

    //==========================================================================
    // Decay mapping: (1-100) -> T60 (0.5s~8s), exponential
    // T60 = 0.5 * (8/0.5)^(normalized) = 0.5 * 16^norm
    //==========================================================================
    float decayNorm = VC_JCLAMP(mParams.decay, 1.0f, 100.0f) / 100.0f;
    mT60 = 0.5f * std::pow(16.0f, decayNorm);  // 0.5s to 8s exponential

    //==========================================================================
    // Damping mapping: (1-100) -> feedback LP cutoff (1kHz~16kHz)
    // Lower damping = brighter (higher cutoff), higher damping = darker
    //==========================================================================
    float dampNorm = VC_JCLAMP(mParams.damping, 1.0f, 100.0f) / 100.0f;
    // Invert: low damping param -> high cutoff (bright), high damping param -> low cutoff (dark)
    float lpCutoff = 16000.0f * std::pow(1000.0f / 16000.0f, dampNorm);  // 16kHz -> 1kHz
    mDampingLPF = 1.0f - std::exp(-2.0f * VC_PI * lpCutoff / static_cast<float>(mSampleRate));

    //==========================================================================
    // Pre-delay in samples
    //==========================================================================
    mPreDelaySamples = static_cast<int>(mParams.preDelay / 1000.0f * mSampleRate);
    mPreDelaySamples = VC_JCLAMP(mPreDelaySamples, 0, MAX_PREDELAY_SAMPLES - 1);

    //==========================================================================
    // Update FDN delay lines: T60-based per-line feedback gains
    // g_i = 10^(-3 * d_i / (sr * T60))
    //==========================================================================
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < FDN_NUM_DELAYS; ++i) {
            int d = mFDN[ch][i].length;
            if (d > 0) {
                float gain = std::pow(10.0f, -3.0f * static_cast<float>(d) /
                    (static_cast<float>(mSampleRate) * mT60));
                mFDN[ch][i].feedbackGain = gain;
                mFDN[ch][i].lpCoeff = mDampingLPF;
            }
        }
    }

    //==========================================================================
    // Post-reverb wet signal filter coefficients
    //==========================================================================
    float wetLPCutoff = VC_JCLAMP(mParams.wetLPF, 1000.0f, 16000.0f);
    mWetLPCoeff = 1.0f - std::exp(-2.0f * VC_PI * wetLPCutoff / static_cast<float>(mSampleRate));

    float wetHPCutoff = VC_JCLAMP(mParams.wetHPF, 20.0f, 500.0f);
    mWetHPCoeff = 1.0f - std::exp(-2.0f * VC_PI * wetHPCutoff / static_cast<float>(mSampleRate));
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

    // Resize pre-delay buffers (max size for 200ms)
    mPreDelayBuffer[0].assign(MAX_PREDELAY_SAMPLES, 0.0f);
    mPreDelayBuffer[1].assign(MAX_PREDELAY_SAMPLES, 0.0f);
    mPreDelayWritePos = 0;

    // Update parameters first (needed for calcFDNDelaySize)
    updateParameters();

    // Scale base delays by sample rate ratio (reference: 44100Hz)
    float srRatio = static_cast<float>(sampleRate) / 44100.0f;

    // Initialize FDN delay lines for both channels
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < FDN_NUM_DELAYS; ++i) {
            int baseDelay = FDN_BASE_DELAYS[i];
            // Scale for sample rate and room size
            int delay = calcFDNDelaySize(static_cast<int>(baseDelay * srRatio + 0.5f));
            mFDN[ch][i].init(delay);
        }
    }

    // Initialize early reflections
    mEarlyRef[0].configure(mRoomSizeMeters, sampleRate);
    mEarlyRef[1].configure(mRoomSizeMeters * 0.95f, sampleRate);  // slightly asymmetric for stereo

    // Re-update with correct delay lengths
    updateParameters();
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

    juce::dsp::AudioBlock<float> block(mInternalPtrs.data(), 2, numSamples);
    process(block);

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

    float* left = block.getChannelPointer(0);
    float* right = block.getChannelPointer(1);
    int numSamples = static_cast<int>(block.getNumSamples());

    processInternal(left, right, numSamples);
}
#endif

//==============================================================================
// Internal FDN reverb processing
//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    updateParameters();

    float mixFactor = VC_JCLAMP(mParams.mix, 0.0f, 100.0f) / 100.0f;

    // FDN output scaling: 1/sqrt(N) to normalize energy
    const float fdnScale = 1.0f / std::sqrt(static_cast<float>(FDN_NUM_DELAYS));
    // Householder matrix scaling: 2/N
    const float hhScale = 2.0f / static_cast<float>(FDN_NUM_DELAYS);

    for (int i = 0; i < numSamples; ++i) {
        float inL = left[i];
        float inR = right[i];

        // Store dry signal
        float dryL = inL;
        float dryR = inR;

        //==============================================================
        // Pre-delay
        //==============================================================
        float predelayL = 0.0f, predelayR = 0.0f;
        if (mPreDelaySamples > 0) {
            int readPos = (mPreDelayWritePos - mPreDelaySamples + MAX_PREDELAY_SAMPLES)
                          % MAX_PREDELAY_SAMPLES;
            predelayL = mPreDelayBuffer[0][readPos];
            predelayR = mPreDelayBuffer[1][readPos];
        }
        mPreDelayBuffer[0][mPreDelayWritePos] = inL;
        mPreDelayBuffer[1][mPreDelayWritePos] = inR;
        mPreDelayWritePos = (mPreDelayWritePos + 1) % MAX_PREDELAY_SAMPLES;

        float delayedL = (mPreDelaySamples > 0) ? predelayL : inL;
        float delayedR = (mPreDelaySamples > 0) ? predelayR : inR;

        //==============================================================
        // Early reflections (simplified image method)
        //==============================================================
        float earlyL = mEarlyRef[0].process(delayedL);
        float earlyR = mEarlyRef[1].process(delayedR);

        //==============================================================
        // FDN Core: 8 delay lines + Householder feedback matrix
        //==============================================================
        // Process each channel independently for stereo
        float wetL = 0.0f, wetR = 0.0f;

        for (int ch = 0; ch < 2; ++ch) {
            float input = (ch == 0) ? delayedL : delayedR;
            float& wetOut = (ch == 0) ? wetL : wetR;

            // Step 1: Read from all delay lines
            float delayOut[FDN_NUM_DELAYS];
            for (int d = 0; d < FDN_NUM_DELAYS; ++d) {
                delayOut[d] = mFDN[ch][d].read();
            }

            // Step 2: Sum delay outputs for wet signal
            // y = sum(delayOut) * scale
            float sumOut = 0.0f;
            for (int d = 0; d < FDN_NUM_DELAYS; ++d) {
                sumOut += delayOut[d];
            }
            wetOut = sumOut * fdnScale;

            // Step 3: Apply Householder feedback matrix
            // A = I - (2/N) * 1·1^T
            // A * v = v - (2/N) * sum(v) * 1
            // This requires only 1 multiply + 7 additions for 8x8
            float sum = 0.0f;
            for (int d = 0; d < FDN_NUM_DELAYS; ++d) {
                sum += delayOut[d];
            }
            float hhTerm = hhScale * sum;  // single multiply

            // Step 4: Feed back through delay lines with input injection
            // new_delay_in = input + A[d] * feedback
            for (int d = 0; d < FDN_NUM_DELAYS; ++d) {
                float feedback = delayOut[d] - hhTerm;  // A * delayOut
                mFDN[ch][d].write(input * fdnScale, feedback);
            }
        }

        //==============================================================
        // Mix FDN tail with early reflections
        // Early reflections + FDN late tail
        //==============================================================
        float reverbL = earlyL * 0.3f + wetL;
        float reverbR = earlyR * 0.3f + wetR;

        //==============================================================
        // Wet signal post-filtering
        //==============================================================
        // Lowpass: high-cut removes high-frequency shimmer
        mWetLPFState[0] = mWetLPCoeff * reverbL + (1.0f - mWetLPCoeff) * mWetLPFState[0];
        mWetLPFState[1] = mWetLPCoeff * reverbR + (1.0f - mWetLPCoeff) * mWetLPFState[1];
        float wetFilteredL = mWetLPFState[0];
        float wetFilteredR = mWetLPFState[1];

        // Highpass: low-cut removes low-frequency rumble
        mWetHPFState[0] = mWetHPCoeff * wetFilteredL + (1.0f - mWetHPCoeff) * mWetHPFState[0];
        mWetHPFState[1] = mWetHPCoeff * wetFilteredR + (1.0f - mWetHPCoeff) * mWetHPFState[1];
        wetFilteredL = wetFilteredL - mWetHPFState[0];
        wetFilteredR = wetFilteredR - mWetHPFState[1];

        //==============================================================
        // Dry/Wet mix
        //==============================================================
        float outL = dryL * (1.0f - mixFactor) + wetFilteredL * mixFactor;
        float outR = dryR * (1.0f - mixFactor) + wetFilteredR * mixFactor;

        left[i] = outL;
        right[i] = outR;
    }
}

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
    // Clear all FDN delay lines
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < FDN_NUM_DELAYS; ++i) {
            mFDN[ch][i].clear();
        }
        mEarlyRef[ch].clear();
        std::fill(mPreDelayBuffer[ch].begin(), mPreDelayBuffer[ch].end(), 0.0f);
    }
    mPreDelayWritePos = 0;

    // Reset wet filter states
    mWetLPFState[0] = 0.0f; mWetLPFState[1] = 0.0f;
    mWetHPFState[0] = 0.0f; mWetHPFState[1] = 0.0f;

    // Update parameters to propagate to delay lines
    updateParameters();
}

//==============================================================================
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;
    mEnabled = p.enabled;
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
