#pragma once

//==============================================================================
// VC-Stereo DSP Core Header - Stereo Width / MS Codec / Pan / Mono Bass
// Supports both JUCE and Standalone (no dependency) modes
//
// Algorithm:
//   1. L/R -> M/S encode:  Mid  = (L + R) / 2
//                          Side = (L - R) / 2
//   2. Width control:      Side *= (width / 100)
//                          0% = mono, 100% = original, 200% = extra-wide
//   3. M/S -> L/R decode:  L = Mid + Side
//                          R = Mid - Side
//   4. Pan control:        L *= cos(pan * pi/2), R *= sin(pan * pi/2)
//                          pan=0 -> full left, pan=0.5 -> center, pan=1 -> full right
//   5. Mono bass (optional): below bassFreq, sum L+R and assign to both channels
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
        float width     = 100.0f;   // % (0 ~ 200) - Stereo width: 0=mono, 100=original, 200=extra-wide
        float pan       = 0.0f;     // (-100 ~ 100) - Stereo pan: -100=full left, 0=center, 100=full right
        bool  monoBass  = false;    // Whether to collapse bass to mono
        float bassFreq  = 150.0f;   // Hz (50 ~ 300) - Crossover frequency for mono bass
        bool  enabled   = true;     // Bypass flag
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

#ifdef VC_STANDALONE
    // Standalone 2nd-order Linkwitz-Riley crossover (LP for mono bass extraction)
    // Uses two cascaded 1st-order sections for 12dB/oct Linkwitz-Riley
    struct CrossoverState {
        // LP filter state (2 cascaded 1st-order)
        float lp_x1[2] = {0.0f, 0.0f}; // per-section x history
        float lp_y1[2] = {0.0f, 0.0f}; // per-section y history
    };
    CrossoverState mCrossover[2]; // [channel]

    void updateCrossoverCoefficients();
    float processLP(int channel, float input);

    // LP filter coefficients (cascaded 1st-order for Linkwitz-Riley)
    float mLP_a1 = 0.0f;
#else
    // JUCE mode: dsp::StateVariableFilter for mono bass
    // We'll use a simple state variable in processInternal
    struct SVFLPState {
        float z1 = 0.0f;
        float z2 = 0.0f;
    };
    SVFLPState mLPState[2]; // [channel]
    float mLP_c = 0.0f;
    void updateLPCoefficient();
    float processLPJuce(int channel, float input);
#endif

    //==========================================================================
    // Member variables
    //==========================================================================
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==========================================================================
    // Non-copyable
    //==========================================================================
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
