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
    mParams.xover1 = 120.0f;
    mParams.xover2 = 1000.0f;
    mParams.xover3 = 8000.0f;
    for (int i = 0; i < kNumBands; ++i) {
        mParams.bandGain[i] = 0.0f;
        mParams.bandThreshold[i] = 0.0f;
        mParams.bandRatio[i] = 1.0f;
    }
    mParams.soloBand = 0;
    mParams.muteBands = 0;
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
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;

    // Resize per-band output buffers
    for (int b = 0; b < kNumBands; ++b) {
        for (int ch = 0; ch < 2; ++ch) {
            mBandBuffers[b][ch].resize(blockSize);
        }
    }

    // Initialize compressors
    for (int ch = 0; ch < 2; ++ch) {
        for (int b = 0; b < kNumBands; ++b) {
            mCompressors[ch][b].prepare(sampleRate);
        }
    }

    // Initialize crossover coefficients
    updateCrossoverCoefficients();

    // Reset all filter states
    reset();
}

//==============================================================================
// Update crossover filter coefficients
// LR4 = two cascaded 2nd-order Butterworth sections
//==============================================================================
void VCPluginDSP::updateCrossoverCoefficients()
{
    float sr = static_cast<float>(mSampleRate);

    // Crossover frequencies
    float fc[3] = {mParams.xover1, mParams.xover2, mParams.xover3};

    for (int x = 0; x < 3; ++x) {
        // Clamp crossover frequency
        float freq = VC_JCLAMP(fc[x], 20.0f, sr * 0.49f);

        float omega = 2.0f * VC_PI * freq / sr;
        float cosW = std::cos(omega);
        float sinW = std::sin(omega);

        // Butterworth Q = 0.7071
        float Q = 0.7071067811865476f;
        float alpha = sinW / (2.0f * Q);

        // 2nd-order Butterworth Lowpass coefficients
        float lp_b0 = (1.0f - cosW) / 2.0f;
        float lp_b1 = 1.0f - cosW;
        float lp_b2 = (1.0f - cosW) / 2.0f;
        float lp_a0 = 1.0f + alpha;
        float lp_a1 = -2.0f * cosW;
        float lp_a2 = 1.0f - alpha;

        // 2nd-order Butterworth Highpass coefficients
        float hp_b0 = (1.0f + cosW) / 2.0f;
        float hp_b1 = -(1.0f + cosW);
        float hp_b2 = (1.0f + cosW) / 2.0f;
        float hp_a0 = 1.0f + alpha;
        float hp_a1 = -2.0f * cosW;
        float hp_a2 = 1.0f - alpha;

        // Normalize by a0
        float lp_norm = 1.0f / lp_a0;
        float hp_norm = 1.0f / hp_a0;

        // Update both channels
        for (int ch = 0; ch < 2; ++ch) {
            LR4Crossover& xo = mCrossovers[ch].xover1;  // temp ref
            // Point to the correct crossover
            LR4Crossover* pXover;
            if (x == 0) pXover = &mCrossovers[ch].xover1;
            else if (x == 1) pXover = &mCrossovers[ch].xover2;
            else pXover = &mCrossovers[ch].xover3;

            // LP stage 1 & 2 (same coefficients for LR4)
            pXover->lpStage1.b0 = lp_b0 * lp_norm;
            pXover->lpStage1.b1 = lp_b1 * lp_norm;
            pXover->lpStage1.b2 = lp_b2 * lp_norm;
            pXover->lpStage1.a1 = lp_a1 * lp_norm;
            pXover->lpStage1.a2 = lp_a2 * lp_norm;

            pXover->lpStage2.b0 = lp_b0 * lp_norm;
            pXover->lpStage2.b1 = lp_b1 * lp_norm;
            pXover->lpStage2.b2 = lp_b2 * lp_norm;
            pXover->lpStage2.a1 = lp_a1 * lp_norm;
            pXover->lpStage2.a2 = lp_a2 * lp_norm;

            // HP stage 1 & 2 (same coefficients for LR4)
            pXover->hpStage1.b0 = hp_b0 * hp_norm;
            pXover->hpStage1.b1 = hp_b1 * hp_norm;
            pXover->hpStage1.b2 = hp_b2 * hp_norm;
            pXover->hpStage1.a1 = hp_a1 * hp_norm;
            pXover->hpStage1.a2 = hp_a2 * hp_norm;

            pXover->hpStage2.b0 = hp_b0 * hp_norm;
            pXover->hpStage2.b1 = hp_b1 * hp_norm;
            pXover->hpStage2.b2 = hp_b2 * hp_norm;
            pXover->hpStage2.a1 = hp_a1 * hp_norm;
            pXover->hpStage2.a2 = hp_a2 * hp_norm;
        }
    }

    // Update compressor parameters
    for (int ch = 0; ch < 2; ++ch) {
        for (int b = 0; b < kNumBands; ++b) {
            mCompressors[ch][b].thresholdDB = mParams.bandThreshold[b];
            mCompressors[ch][b].ratio = mParams.bandRatio[b];
        }
    }
}

//==============================================================================
// Process interleaved stereo buffer
//==============================================================================
void VCPluginDSP::process(float* left, float* right, int numSamples)
{
    if (!mEnabled)
        return;

    // Resize band buffers if needed
    for (int b = 0; b < kNumBands; ++b) {
        for (int ch = 0; ch < 2; ++ch) {
            if ((int)mBandBuffers[b][ch].size() < numSamples)
                mBandBuffers[b][ch].resize(numSamples);
        }
    }

#ifdef VC_STANDALONE
    processInternal(left, right, numSamples);
#else
    // JUCE: use AudioBlock
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
// Internal processing - LR4 crossover split, per-band gain+comp, sum
//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    float* channels[2] = { left, right };

    // Per-sample processing for crossover + gain + compression
    for (int i = 0; i < numSamples; ++i) {
        for (int ch = 0; ch < 2; ++ch) {
            float input = channels[ch][i];

            // === Crossover split ===
            // Stage 1: xover1 splits Low vs rest
            float low, rest1;
            mCrossovers[ch].xover1.processSample(input, low, rest1);

            // Stage 2: xover2 splits Mid-Low vs rest
            float midLow, rest2;
            mCrossovers[ch].xover2.processSample(rest1, midLow, rest2);

            // Stage 3: xover3 splits Mid-High vs High
            float midHigh, high;
            mCrossovers[ch].xover3.processSample(rest2, midHigh, high);

            // Band outputs before gain/comp
            float bands[kNumBands] = { low, midLow, midHigh, high };

            // === Per-band gain + compression ===
            float output = 0.0f;

            for (int b = 0; b < kNumBands; ++b) {
                // Check mute
                if (mParams.muteBands & (1 << b))
                    continue;

                // Check solo (if any band is solo'd, only that band passes)
                if (mParams.soloBand > 0 && mParams.soloBand != (b + 1))
                    continue;

                // Apply gain
                float gainLinear = VCStandalone::decibelsToGain(mParams.bandGain[b]);
                float sample = bands[b] * gainLinear;

                // Apply compression
                sample = mCompressors[ch][b].processSample(sample);

                output += sample;
            }

            channels[ch][i] = output;
        }
    }
}

//==============================================================================
// JUCE AudioBlock processing
//==============================================================================
#ifndef VC_STANDALONE
void VCPluginDSP::process(juce::dsp::AudioBlock<float>& block)
{
    if (!mEnabled)
        return;

    int numSamples = static_cast<int>(block.getNumSamples());

    // Resize band buffers if needed
    for (int b = 0; b < kNumBands; ++b) {
        for (int ch = 0; ch < 2; ++ch) {
            if ((int)mBandBuffers[b][ch].size() < numSamples)
                mBandBuffers[b][ch].resize(numSamples);
        }
    }

    for (size_t ch = 0; ch < block.getNumChannels() && ch < 2; ++ch) {
        auto* data = block.getChannelPointer(ch);

        for (int i = 0; i < numSamples; ++i) {
            float input = data[i];

            // Crossover split
            float low, rest1;
            mCrossovers[ch].xover1.processSample(input, low, rest1);

            float midLow, rest2;
            mCrossovers[ch].xover2.processSample(rest1, midLow, rest2);

            float midHigh, high;
            mCrossovers[ch].xover3.processSample(rest2, midHigh, high);

            float bands[kNumBands] = { low, midLow, midHigh, high };

            float output = 0.0f;
            for (int b = 0; b < kNumBands; ++b) {
                if (mParams.muteBands & (1 << b))
                    continue;
                if (mParams.soloBand > 0 && mParams.soloBand != (b + 1))
                    continue;

                float gainLinear = juce::Decibels::decibelsToGain(mParams.bandGain[b]);
                float sample = bands[b] * gainLinear;
                sample = mCompressors[ch][b].processSample(sample);
                output += sample;
            }

            data[i] = output;
        }
    }
}
#endif

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
    for (int ch = 0; ch < 2; ++ch) {
        mCrossovers[ch].xover1.reset();
        mCrossovers[ch].xover2.reset();
        mCrossovers[ch].xover3.reset();
        for (int b = 0; b < kNumBands; ++b) {
            mCompressors[ch][b].reset();
        }
    }
}

//==============================================================================
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;
    updateCrossoverCoefficients();
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
