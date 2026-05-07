//==============================================================================
// VC-Chorus DSP Core Implementation - Multi-Voice Chorus Effect
//
// Algorithm overview:
//   1. Input signal is written into stereo delay lines
//   2. For each voice, LFO modulates the read position:
//        mod_delay = base_delay + depth_ms * sin(2pi * rate * t + phase_offset)
//   3. Voices are panned across stereo field using width control
//   4. Feedback from delay output back into delay input adds richness
//   5. Dry signal + wet signal mixed according to mix parameter
//==============================================================================

#include "VCPluginDSP.h"

#ifdef VC_STANDALONE
#include <algorithm>
#include <cmath>
#endif

// Static member initialization
constexpr float VCPluginDSP::kVoicePhaseOffsets[CHORUS_MAX_VOICES];

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
    // Max delay = 30ms, max mod depth = 10ms, add 5ms safety
    float maxDelayMs = CHORUS_MAX_DELAY_MS + CHORUS_MAX_MOD_DEPTH_MS + 5.0f;
    mMaxDelaySamples = static_cast<int>(maxDelayMs * 0.001f * sampleRate);
    mMaxDelaySamples = VC_JMAX(mMaxDelaySamples, 256);  // Minimum buffer size

    mDelayBuffer[0].resize(mMaxDelaySamples, 0.0f);
    mDelayBuffer[1].resize(mMaxDelaySamples, 0.0f);
    mWritePos = 0;

    // Reset LFO phase
    mLFOPhase = 0.0f;

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
    // Convert delay in samples to read position (behind write head)
    float readPos = static_cast<float>(mWritePos) - delaySamples;
    while (readPos < 0.0f)
        readPos += static_cast<float>(mMaxDelaySamples);
    while (readPos >= static_cast<float>(mMaxDelaySamples))
        readPos -= static_cast<float>(mMaxDelaySamples);

    // Linear interpolation
    int pos0 = static_cast<int>(readPos);
    int pos1 = (pos0 + 1) % mMaxDelaySamples;
    float frac = readPos - static_cast<float>(pos0);

    return mDelayBuffer[channel][pos0] * (1.0f - frac) +
           mDelayBuffer[channel][pos1] * frac;
}

//==============================================================================
// Internal chorus processing
//==============================================================================
void VCPluginDSP::processInternal(float* left, float* right, int numSamples)
{
    float sampleRateF = static_cast<float>(mSampleRate);

    // Parameter conversion
    float rate = mParams.rate;                             // Hz
    float depthMs = mParams.depth / 100.0f * CHORUS_MAX_MOD_DEPTH_MS;  // 0~10ms
    int voices = VC_JCLAMP(mParams.voices, 1, CHORUS_MAX_VOICES);
    float wetMix = mParams.mix / 100.0f;                  // 0~1
    float dryMix = 1.0f - wetMix;
    float baseDelayMs = mParams.delay;                    // 5~30ms
    float width = mParams.width / 100.0f;                 // 0~1
    float feedback = mParams.feedback / 100.0f;           // 0~0.5

    // LFO phase increment per sample
    float phaseIncrement = 2.0f * VC_PI * rate / sampleRateF;

    // Base delay in samples
    float baseDelaySamples = baseDelayMs * 0.001f * sampleRateF;

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = left[i];
        float inR = right[i];

        // Write input + feedback into delay lines
        // Read current output for feedback (from voice 0's position as reference)
        float fbReadL = readDelay(0, baseDelaySamples);
        float fbReadR = readDelay(1, baseDelaySamples);

        mDelayBuffer[0][mWritePos] = inL + feedback * fbReadL;
        mDelayBuffer[1][mWritePos] = inR + feedback * fbReadR;

        // Accumulate wet signal from all voices
        float wetL = 0.0f;
        float wetR = 0.0f;

        for (int v = 0; v < voices; ++v)
        {
            // LFO for this voice
            float lfoValue = std::sin(mLFOPhase + kVoicePhaseOffsets[v]);
            float modDelaySamples = baseDelaySamples + depthMs * 0.001f * sampleRateF * lfoValue;

            // Ensure delay doesn't go negative or exceed buffer
            modDelaySamples = VC_JCLAMP(modDelaySamples, 1.0f,
                                        static_cast<float>(mMaxDelaySamples) - 2.0f);

            // Read from delay lines with modulated position
            float voiceOutL = readDelay(0, modDelaySamples);
            float voiceOutR = readDelay(1, modDelaySamples);

            // Stereo width: pan voices across the stereo field
            // Voice 0: center, Voice 1: wide, Voice 2: opposite, Voice 3: very wide
            // Using different LFO phases for L and R creates width
            float lfoValueR = std::sin(mLFOPhase + kVoicePhaseOffsets[v] + width * VC_PI * 0.5f);
            float modDelayR = baseDelaySamples + depthMs * 0.001f * sampleRateF * lfoValueR;
            modDelayR = VC_JCLAMP(modDelayR, 1.0f,
                                   static_cast<float>(mMaxDelaySamples) - 2.0f);

            voiceOutR = readDelay(1, modDelayR);

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
