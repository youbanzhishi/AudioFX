#pragma once

//==============================================================================
// VC-Gate DSP Core Header - Noise Gate / Expander
// Supports both JUCE and Standalone (no dependency) modes
//
// Algorithm: Downward expander with adjustable ratio
//   - Below threshold: gain = range * (1 - gate_gain)
//   - Envelope detection via RMS-style follower
//   - Attack/Hold/Release smoothing for gate gain
//   - Ratio 1:1 = no processing, higher = more gating
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
// Envelope Follower for Gate Detection
// Uses RMS-style squared envelope with separate attack/release
// NOTE: releaseCoef is initialized in prepare() to avoid the VC-Comp bug
//==============================================================================
class GateEnvelopeFollower
{
public:
    GateEnvelopeFollower() = default;

    void reset() { envelope = 0.0f; }

    void setAttackTime(float timeMs, float sampleRate)
    {
        attackCoef = std::exp(-1.0f / (timeMs * 0.001f * sampleRate));
        attackCoefInv = 1.0f - attackCoef;
    }

    void setReleaseTime(float timeMs, float sampleRate)
    {
        releaseCoef = std::exp(-1.0f / (timeMs * 0.001f * sampleRate));
        releaseCoefInv = 1.0f - releaseCoef;
    }

    float processSample(float input)
    {
        float inputAbs = std::abs(input);
        float sq = inputAbs * inputAbs;

        if (sq > envelope)
            envelope = attackCoefInv * sq + attackCoef * envelope;
        else
            envelope = releaseCoefInv * sq + releaseCoef * envelope;

        return envelope;
    }

    float getEnvelope() const { return envelope; }

private:
    float envelope = 0.0f;
    float attackCoef = 0.0f, attackCoefInv = 1.0f;
    float releaseCoef = 0.0f, releaseCoefInv = 1.0f;
};

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
        float threshold = -40.0f;   // dB (-80 ~ 0)
        float ratio = 10.0f;        // Expansion ratio (1 ~ 20)
        float attack = 1.0f;        // ms (0.1 ~ 50)
        float hold = 50.0f;         // ms (0 ~ 500)
        float release = 100.0f;     // ms (10 ~ 2000)
        float range = -80.0f;       // dB (-80 ~ 0), max attenuation when gate closed
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

    // Get current gate state for metering
    float getGateGain() const { return mGateGain; }

private:
    //==========================================================================
    // Internal DSP implementation
    //==========================================================================
    void processInternal(float* left, float* right, int numSamples);

    //==========================================================================
    // Member variables
    //==========================================================================
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Envelope detection
    GateEnvelopeFollower mEnvelopeFollower;

    // Gate state machine
    float mGateGain = 0.0f;        // Current gate gain (0=closed, 1=open)
    int mHoldCounter = 0;          // Samples remaining in hold phase
    bool mGateOpen = false;        // Current gate state

    // Smoothed gain coefficients
    float mAttackCoef = 0.0f;
    float mAttackCoefInv = 1.0f;
    float mReleaseCoef = 0.0f;
    float mReleaseCoefInv = 1.0f;

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==========================================================================
    // Non-copyable
    //==========================================================================
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
