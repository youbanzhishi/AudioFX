#pragma once

//==============================================================================
// VC-Chorus DSP Core Header - Multi-Voice Chorus Effect
// Supports both JUCE and Standalone (no dependency) modes
//
// Algorithm: Multi-voice modulated delay chorus
//   1. Base delay line (5~30ms)
//   2. LFO modulates delay time: delay + depth * sin(2pi * rate * t)
//   3. Multiple voices with different LFO phase offsets (0, 90, 180, 270 degrees)
//   4. Stereo width via different phase per channel
//   5. Feedback for richer texture
//   6. Dry/wet mix control
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
// Maximum configuration constants
//==============================================================================
constexpr int CHORUS_MAX_VOICES = 4;
constexpr float CHORUS_MAX_DELAY_MS = 30.0f;
constexpr float CHORUS_MAX_MOD_DEPTH_MS = 10.0f;  // Max modulation depth in ms

//==============================================================================
// Main DSP Class
//==============================================================================
class VCPluginDSP
{
public:
    //==========================================================================
    // Plugin-specific parameter structure
    //==========================================================================
    struct Params
    {
        float rate = 1.5f;          // Hz (0.1 ~ 10) - LFO modulation rate
        float depth = 50.0f;        // % (0 ~ 100) - LFO modulation depth
        int voices = 2;             // Number of chorus voices (1 ~ 4)
        float mix = 50.0f;          // % (0 ~ 100) - Dry/wet mix
        float delay = 15.0f;        // ms (5 ~ 30) - Base delay time
        float width = 80.0f;        // % (0 ~ 100) - Stereo width
        float feedback = 20.0f;     // % (0 ~ 50) - Feedback amount
        bool enabled = true;        // Bypass flag
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
    // JUCE AudioBlock processing (non-interleaved)
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
    // Member variables
    //==========================================================================
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Delay lines: one per channel (L/R), each voice reads from the same line
    // but with different LFO phases
    std::vector<float> mDelayBuffer[2];  // [channel]
    int mWritePos = 0;
    int mMaxDelaySamples = 0;            // Buffer size in samples

    // LFO phase accumulator (runs continuously)
    float mLFOPhase = 0.0f;

    // LFO phase offsets for each voice (in radians)
    // Voice 0: 0°, Voice 1: 90°, Voice 2: 180°, Voice 3: 270°
    static constexpr float kVoicePhaseOffsets[CHORUS_MAX_VOICES] = {
        0.0f,
        1.5707963268f,     // pi/2
        3.1415926536f,     // pi
        4.7123889804f      // 3*pi/2
    };

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==========================================================================
    // Non-copyable
    //==========================================================================
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
