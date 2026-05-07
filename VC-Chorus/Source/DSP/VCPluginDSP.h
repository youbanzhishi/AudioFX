#pragma once

//==============================================================================
// VC-Chorus DSP Core Header - Multi-Voice Chorus Effect (Gen2)
// Supports both JUCE and Standalone (no dependency) modes
//
// Gen2 Upgrades:
//   1. Multi-voice chorus: 2-8 independent delay lines + LFO
//   2. Stereo expansion: L/R LFO phase offset for natural width
//   3. Adjustable LFO waveform: sine / triangle / random
//   4. Extended feedback range: 0-0.9 for richer texture
//   5. Per-voice LFO with individual phase offsets
//==============================================================================

// Shared constants (available in both JUCE and Standalone modes)
constexpr float VC_PI = 3.14159265358979323846f;

#ifdef VC_STANDALONE
// Standalone mode: no JUCE dependency
#include <vector>
#include <cmath>
#include <algorithm>

namespace VCStandalone {
    inline float decibelsToGain(float dB) { return std::pow(10.0f, dB / 20.0f); }
    inline float gainToDecibels(float gain) { return 20.0f * std::log10(std::max(gain, 1e-10f)); }
}

// Standalone macros (no-op for non-JUCE mode)
#define VC_DECLARE_NON_COPYABLE(x)  // No-op in standalone mode
#define VC_JMIN(a, b) std::min(a, b)
#define VC_JMAX(a, b) std::max(a, b)
#define VC_JCLAMP(a, b, c) std::clamp(a, b, c)
#else
// JUCE mode
#include <juce_dsp/juce_dsp.h>

#define VC_DECLARE_NON_COPYABLE(x) JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(x)
#define VC_JMIN(a, b) juce::jmin(a, b)
#define VC_JMAX(a, b) juce::jmax(a, b)
#define VC_JCLAMP(a, b, c) juce::jlimit(a, b, c)
#endif

//==============================================================================
// Maximum configuration constants (Gen2: expanded to 8 voices)
//==============================================================================
constexpr int CHORUS_MAX_VOICES = 8;
constexpr float CHORUS_MAX_DELAY_MS = 40.0f;
constexpr float CHORUS_MAX_MOD_DEPTH_MS = 15.0f;

//==============================================================================
// LFO Waveform Type (Gen2)
//==============================================================================
enum class LFOWaveform
{
    Sine = 0,
    Triangle = 1,
    Random = 2
};

//==============================================================================
// Random LFO interpolator (Gen2)
// Generates smooth random modulation by interpolating between random targets
//==============================================================================
class RandomLFO
{
public:
    RandomLFO() = default;

    void reset()
    {
        currentTarget = 0.0f;
        previousTarget = 0.0f;
        phase = 0.0f;
    }

    // Returns a smooth random value between -1 and 1
    float process(float phaseIncrement)
    {
        phase += phaseIncrement;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            previousTarget = currentTarget;
            currentTarget = randomTarget();
        }
        // Cosine interpolation between targets
        float t = 0.5f * (1.0f - std::cos(phase * VC_PI));
        return previousTarget + t * (currentTarget - previousTarget);
    }

private:
    float currentTarget = 0.0f;
    float previousTarget = 0.0f;
    float phase = 0.0f;

    // Simple pseudo-random in [-1, 1]
    float randomTarget()
    {
        // Use a simple LCG for reproducibility
        static unsigned int seed = 12345;
        seed = seed * 1103515245u + 12345u;
        return (static_cast<float>((seed >> 16) & 0x7FFF) / 16384.0f) - 1.0f;
    }
};

//==============================================================================
// Main DSP Class (Gen2)
//==============================================================================
class VCPluginDSP
{
public:
    //==========================================================================
    // Plugin-specific parameter structure (Gen2)
    //==========================================================================
    struct Params
    {
        float rate = 1.5f;              // Hz (0.1 ~ 10) - LFO modulation rate
        float depth = 50.0f;            // % (0 ~ 100) - LFO modulation depth
        int voices = 3;                 // Number of chorus voices (2 ~ 8)
        float mix = 50.0f;              // % (0 ~ 100) - Dry/wet mix
        float delay = 15.0f;            // ms (5 ~ 40) - Base delay time
        float width = 80.0f;            // % (0 ~ 100) - Stereo width
        float feedback = 0.2f;          // (0 ~ 0.9) - Feedback amount
        LFOWaveform lfoWaveform = LFOWaveform::Sine;  // LFO waveform type
        float stereoPhase = 90.0f;      // degrees (0 ~ 180) - L/R LFO phase offset
        bool enabled = true;            // Bypass flag
    };

    //==========================================================================
    // Construction / Destruction
    //==========================================================================
    VCPluginDSP();
    ~VCPluginDSP();

    //==========================================================================
    // Processing
    //==========================================================================
    void prepare(double sampleRate, int blockSize);
    void process(float* left, float* right, int numSamples);

#ifndef VC_STANDALONE
    void process(juce::dsp::AudioBlock<float>& block);
#endif

    void reset();

    //==========================================================================
    // Parameter access
    //==========================================================================
    void setParams(const Params& p);
    Params getParams() const;

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

    //==========================================================================
    // Utility functions
    //==========================================================================
    static float dBToLinear(float dB) {
#ifdef VC_STANDALONE
        return VCStandalone::decibelsToGain(dB);
#else
        return juce::Decibels::decibelsToGain(dB);
#endif
    }

    static float linearToDb(float linear) {
#ifdef VC_STANDALONE
        return VCStandalone::gainToDecibels(linear);
#else
        return juce::Decibels::gainToDecibels(linear);
#endif
    }

    double getSampleRate() const { return mSampleRate; }
    int getBlockSize() const { return mBlockSize; }

private:
    //==========================================================================
    // Internal DSP implementation
    //==========================================================================
    void processInternal(float* left, float* right, int numSamples);

    //==========================================================================
    // Fractional delay read with linear interpolation
    //==========================================================================
    float readDelay(int channel, float delaySamples) const;

    //==========================================================================
    // Gen2: LFO waveform generation
    //==========================================================================
    float generateLFO(LFOWaveform waveform, float phase) const;

    //==========================================================================
    // Member variables
    //==========================================================================
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Delay lines: one per channel (L/R), each voice reads from the same line
    // but with different LFO phases
    std::vector<float> mDelayBuffer[2];
    int mWritePos = 0;
    int mMaxDelaySamples = 0;

    // LFO phase accumulator (runs continuously)
    float mLFOPhase = 0.0f;

    // Gen2: Per-voice LFO phase offsets (evenly distributed)
    float mVoicePhaseOffsets[CHORUS_MAX_VOICES] = {};

    // Gen2: Random LFO generators (one per voice)
    RandomLFO mRandomLFO[CHORUS_MAX_VOICES];

    // Gen2: Random LFO for right channel (with stereo phase offset)
    RandomLFO mRandomLFOR[CHORUS_MAX_VOICES];

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==========================================================================
    // Non-copyable
    //==========================================================================
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
