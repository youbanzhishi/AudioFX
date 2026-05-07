//==============================================================================
// VC-Chorus DSP Core Implementation - Multi-Voice Chorus Effect (Gen2)
//
// Gen2 Algorithm overview:
//   1. Input signal is written into stereo delay lines
//   2. For each voice, LFO modulates the read position with individual phase
//   3. LFO waveform selectable: sine, triangle, or random interpolation
//   4. Stereo phase offset between L/R creates natural width
//   5. Feedback from delay output back into delay input adds richness
//   6. Up to 8 voices for dense ensemble effects
//   7. Dry signal + wet signal mixed according to mix parameter
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

    // Allocate delay buffer: max delay + max modulation + safety margin
    float maxDelayMs = CHORUS_MAX_DELAY_MS + CHORUS_MAX_MOD_DEPTH_MS + 10.0f;
    mMaxDelaySamples = static_cast<int>(maxDelayMs * 0.001f * sampleRate);
    mMaxDelaySamples = VC_JMAX(mMaxDelaySamples, 512);

    mDelayBuffer[0].resize(mMaxDelaySamples, 0.0f);
    mDelayBuffer[1].resize(mMaxDelaySamples, 0.0f);
    mWritePos = 0;

    // Reset LFO phase
    mLFOPhase = 0.0f;

    // Gen2: Compute per-voice phase offsets (evenly distributed across 360 degrees)
    for (int v = 0; v < CHORUS_MAX_VOICES; ++v)
    {
        mVoicePhaseOffsets[v] = (2.0f * VC_PI * static_cast<float>(v)) /
                                 static_cast<float>(CHORUS_MAX_VOICES);
        mRandomLFO[v].reset();
        mRandomLFOR[v].reset();
    }

    // Resize internal buffer for AudioBlock conversion
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
// Read from delay buffer with fractional sample position (linear interpolation)
//==============================================================================
float VCPluginDSP::readDelay(int channel, float delaySamples) const
{
    float readPos = static_cast<float>(mWritePos) - delaySamples;
    while (readPos < 0.0f)
        readPos += static_cast<float>(mMaxDelaySamples);
    while (readPos >= static_cast<float>(mMaxDelaySamples))
        readPos -= static_cast<float>(mMaxDelaySamples);

    int pos0 = static_cast<int>(readPos);
    int pos1 = (pos0 + 1) % mMaxDelaySamples;
    float frac = readPos - static_cast<float>(pos0);

    return mDelayBuffer[channel][pos0] * (1.0f - frac) +
           mDelayBuffer[channel][pos1] * frac;
}

//==============================================================================
// Gen2: LFO waveform generation
//==============================================================================
float VCPluginDSP::generateLFO(LFOWaveform waveform, float phase) const
{
    switch (waveform)
    {
        case LFOWaveform::Sine:
            return std::sin(phase);

        case LFOWaveform::Triangle:
        {
            // Triangle: period of 2*pi, output -1 to 1
            float p = phase / (2.0f * VC_PI);  // normalize to 0-1
            p = p - std::floor(p);              // keep fractional part
            if (p < 0.25f)
                return 4.0f * p;
            else if (p < 0.75f)
                return 2.0f - 4.0f * p;
            else
                return -4.0f + 4.0f * p;
        }

        case LFOWaveform::Random:
            // Random LFO is handled separately with per-voice RandomLFO objects
            // This return is a fallback, should not be called for Random mode
            return 0.0f;

        default:
            return std::sin(phase);
    }
}

//==============================================================================
// Internal chorus processing (Gen2)
//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    float sampleRateF = static_cast<float>(mSampleRate);

    // Parameter conversion
    float rate = mParams.rate;
    float depthMs = mParams.depth / 100.0f * CHORUS_MAX_MOD_DEPTH_MS;
    int voices = VC_JCLAMP(mParams.voices, 2, CHORUS_MAX_VOICES);
    float wetMix = mParams.mix / 100.0f;
    float dryMix = 1.0f - wetMix;
    float baseDelayMs = mParams.delay;
    float width = mParams.width / 100.0f;
    float feedback = VC_JCLAMP(mParams.feedback, 0.0f, 0.9f);
    LFOWaveform waveform = mParams.lfoWaveform;
    float stereoPhaseRad = mParams.stereoPhase * VC_PI / 180.0f;

    // LFO phase increment per sample
    float phaseIncrement = 2.0f * VC_PI * rate / sampleRateF;
    // Random LFO phase increment (full cycle = 2*PI/rate mapped to 0-1)
    float randomPhaseIncrement = rate / sampleRateF;

    // Base delay in samples
    float baseDelaySamples = baseDelayMs * 0.001f * sampleRateF;

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = left[i];
        float inR = right[i];

        // Write input + feedback into delay lines
        float fbReadL = readDelay(0, baseDelaySamples);
        float fbReadR = readDelay(1, baseDelaySamples);

        mDelayBuffer[0][mWritePos] = inL + feedback * fbReadL;
        mDelayBuffer[1][mWritePos] = inR + feedback * fbReadR;

        // Accumulate wet signal from all voices
        float wetL = 0.0f;
        float wetR = 0.0f;

        for (int v = 0; v < voices; ++v)
        {
            float voicePhase = mLFOPhase + mVoicePhaseOffsets[v];
            float voicePhaseR = mLFOPhase + mVoicePhaseOffsets[v] + stereoPhaseRad;

            float lfoValueL, lfoValueR;

            if (waveform == LFOWaveform::Random)
            {
                // Gen2: Random waveform - use dedicated RandomLFO per voice
                lfoValueL = mRandomLFO[v].process(randomPhaseIncrement);
                lfoValueR = mRandomLFOR[v].process(randomPhaseIncrement);
            }
            else
            {
                // Sine or Triangle
                lfoValueL = generateLFO(waveform, voicePhase);
                lfoValueR = generateLFO(waveform, voicePhaseR);
            }

            // Modulated delay for left channel
            float modDelayL = baseDelaySamples + depthMs * 0.001f * sampleRateF * lfoValueL;
            modDelayL = VC_JCLAMP(modDelayL, 1.0f,
                                   static_cast<float>(mMaxDelaySamples) - 2.0f);

            // Modulated delay for right channel (with stereo phase offset)
            float modDelayR = baseDelaySamples + depthMs * 0.001f * sampleRateF * lfoValueR;
            modDelayR = VC_JCLAMP(modDelayR, 1.0f,
                                   static_cast<float>(mMaxDelaySamples) - 2.0f);

            float voiceOutL = readDelay(0, modDelayL);
            float voiceOutR = readDelay(1, modDelayR);

            // Accumulate
            wetL += voiceOutL;
            wetR += voiceOutR;
        }

        // Normalize by voice count
        if (voices > 0)
        {
            float invVoices = 1.0f / static_cast<float>(voices);
            wetL *= invVoices;
            wetR *= invVoices;
        }

        // Output: dry + wet
        left[i] = dryMix * inL + wetMix * wetL;
        right[i] = dryMix * inR + wetMix * wetR;

        // Advance write position
        mWritePos = (mWritePos + 1) % mMaxDelaySamples;

        // Advance LFO phase
        mLFOPhase += phaseIncrement;
        if (mLFOPhase > 2.0f * VC_PI)
            mLFOPhase -= 2.0f * VC_PI;
    }
}

//==============================================================================
// Reset processing state
//==============================================================================
void VCPluginDSP::reset()
{
    std::fill(mDelayBuffer[0].begin(), mDelayBuffer[0].end(), 0.0f);
    std::fill(mDelayBuffer[1].begin(), mDelayBuffer[1].end(), 0.0f);
    mWritePos = 0;
    mLFOPhase = 0.0f;

    for (int v = 0; v < CHORUS_MAX_VOICES; ++v)
    {
        mRandomLFO[v].reset();
        mRandomLFOR[v].reset();
    }
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
