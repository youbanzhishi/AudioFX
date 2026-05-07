//==============================================================================
// VC-Stereo DSP Core Implementation - Stereo Width / MS Codec / Pan / Mono Bass
//
// Processing chain:
//   Input L/R
//     -> (optional) Mono Bass extraction via LP crossover
//     -> L/R to M/S encode
//     -> Width control (scale Side)
//     -> M/S to L/R decode
//     -> Pan control (cos/sin panning law)
//     -> (optional) Replace low-freq content with mono bass
//   Output L/R
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

    // Resize internal buffer for AudioBlock conversion
    mInternalBuffer.resize(blockSize * 2);
    mInternalPtrs.resize(2);
    mInternalPtrs[0] = mInternalBuffer.data();
    mInternalPtrs[1] = mInternalBuffer.data() + blockSize;

#ifdef VC_STANDALONE
    // Initialize crossover filter states
    for (int ch = 0; ch < 2; ++ch) {
        mCrossover[ch] = CrossoverState();
    }
    updateCrossoverCoefficients();
#endif
}

//==============================================================================
// Process stereo buffer
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
        mInternalBuffer.resize(static_cast<size_t>(numSamples) * 2);

    float* leftBuf = mInternalBuffer.data();
    float* rightBuf = mInternalBuffer.data() + numSamples;

    for (int i = 0; i < numSamples; ++i) {
        leftBuf[i] = left[i];
        rightBuf[i] = right[i];
    }

    mInternalPtrs[0] = leftBuf;
    mInternalPtrs[1] = rightBuf;

    juce::dsp::AudioBlock<float> block(mInternalPtrs.data(), 2, static_cast<size_t>(numSamples));
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

    auto numSamples = static_cast<int>(block.getNumSamples());

    if (numSamples < 1)
        return;

    float* leftBuf = block.getChannelPointer(0);
    float* rightBuf = block.getChannelPointer(1);

    processInternal(leftBuf, rightBuf, numSamples);
}
#endif

//==============================================================================
// Internal stereo processing
//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    // Parameter conversion
    float widthScale = mParams.width / 100.0f;  // 0~2 (0=mono, 1=original, 2=extra-wide)
    float panNormalized = mParams.pan / 100.0f;  // -1~1 (-1=full left, 0=center, 1=full right)
    bool monoBass = mParams.monoBass;
    float bassFreq = mParams.bassFreq;

    // Pan gain using constant-power panning law
    // panNormalized: -1 (left) to +1 (right)
    // Map to angle: 0 (left) to pi/2 (right)
    float panAngle = (panNormalized + 1.0f) * 0.5f * (VC_PI * 0.5f);
    float panGainL = std::cos(panAngle);
    float panGainR = std::sin(panAngle);

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = left[i];
        float inR = right[i];

        // Optional mono bass extraction
        float bassL = 0.0f, bassR = 0.0f;
        float midBass = 0.0f;
        if (monoBass) {
#ifdef VC_STANDALONE
            bassL = processLP(0, inL);
            bassR = processLP(1, inR);
#else
            // JUCE mode: simple 1st-order LP for bass extraction
            bassL = processLP(0, inL);
            bassR = processLP(1, inR);
#endif
            midBass = (bassL + bassR) * 0.5f;
        }

        // ---- Step 1: L/R -> M/S encode ----
        float mid  = (inL + inR) * 0.5f;
        float side = (inL - inR) * 0.5f;

        // ---- Step 2: Width control ----
        side *= widthScale;

        // ---- Step 3: M/S -> L/R decode ----
        float outL = mid + side;
        float outR = mid - side;

        // ---- Step 4: Pan control ----
        outL *= panGainL;
        outR *= panGainR;

        // ---- Step 5: Mono bass replacement ----
        if (monoBass) {
            // Remove original bass content and replace with mono bass
            outL = outL - bassL + midBass;
            outR = outR - bassR + midBass;
        }

        left[i] = outL;
        right[i] = outR;
    }
}

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
#ifdef VC_STANDALONE
    for (int ch = 0; ch < 2; ++ch) {
        mCrossover[ch] = CrossoverState();
    }
#endif
}

//==============================================================================
// Set parameters
//==============================================================================
void VCPluginDSP::setParams(const Params& p)
{
    mParams = p;

#ifdef VC_STANDALONE
    updateCrossoverCoefficients();
#endif
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

//==============================================================================
// Standalone crossover filter implementation
//==============================================================================
#ifdef VC_STANDALONE

void VCPluginDSP::updateCrossoverCoefficients()
{
    // 1st-order Butterworth LP coefficient
    // H(z) = (1 + z^-1) / (2 * (1 + a1 * z^-1))
    // a1 = (1 - tan(wc/2)) / (1 + tan(wc/2))
    // For Linkwitz-Riley, we cascade two identical 1st-order sections
    float wc = 2.0f * VC_PI * mParams.bassFreq / static_cast<float>(mSampleRate);
    float tanHalf = std::tan(wc * 0.5f);
    mLP_a1 = (1.0f - tanHalf) / (1.0f + tanHalf);
}

float VCPluginDSP::processLP(int channel, float input)
{
    CrossoverState& s = mCrossover[channel];

    // Cascaded 1st-order LP: two identical sections
    // Section transfer: y[n] = 0.5*(1+a1)*x[n] + 0.5*(1-a1)*x[n-1] - a1*y[n-1]
    // But for 1st-order Butterworth LP:
    // y[n] = b0*x[n] + b1*x[n-1] - a1*y[n-1]
    // b0 = b1 = (1 - a1) / 2

    float b0 = (1.0f - mLP_a1) * 0.5f;
    float b1 = b0;

    // Section 1
    float y1 = b0 * input + b1 * s.lp_x1[0] - mLP_a1 * s.lp_y1[0];
    s.lp_x1[0] = input;
    s.lp_y1[0] = y1;

    // Section 2
    float y2 = b0 * y1 + b1 * s.lp_x1[1] - mLP_a1 * s.lp_y1[1];
    s.lp_x1[1] = y1;
    s.lp_y1[1] = y2;

    return y2;
}

#endif
