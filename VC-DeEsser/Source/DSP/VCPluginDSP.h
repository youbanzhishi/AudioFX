#pragma once

//==============================================================================
// VC-DeEsser DSP Core Header
// De-esser / Sibilance Reduction
// Supports both JUCE and Standalone (no dependency) modes
//==============================================================================

#ifdef VC_STANDALONE
// Standalone mode: no JUCE dependency
#include <vector>
#include <cmath>
#include <algorithm>

// Standalone constants
constexpr float VC_PI = 3.14159265358979323846f;

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
#define VC_JCLAMP(a, b, c) juce::jclamp(a, b, c)
#endif

//==============================================================================
// Main DSP Class
//==============================================================================
class VCDeEsserDSP
{
public:
    //==============================================================================
    // Plugin-specific parameter structure
    struct Params
    {
        float threshold = -20.0f;   // Trigger threshold dB (-40~0)
        float frequency = 6000.0f;  // Sibilance detection center frequency Hz (2000~12000)
        float reduction = -10.0f;   // Maximum attenuation dB (-30~0)
        float mix = 100.0f;         // Dry/Wet mix (0-100%)
        bool enabled = true;
    };

    //==============================================================================
    // Construction / Destruction
    VCDeEsserDSP();
    ~VCDeEsserDSP();

    //==============================================================================
    // Processing
    void prepare(double sampleRate, int blockSize);
    void process(float* left, float* right, int numSamples);

#ifndef VC_STANDALONE
    // JUCE AudioBlock processing (non-interleaved)
    void process(juce::dsp::AudioBlock<float>& block);
#endif

    void reset();

    //==============================================================================
    // Parameter access
    void setParams(const Params& p);
    Params getParams() const;

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

    //==============================================================================
    // Utility functions
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
    //==============================================================================
    // Internal DSP implementation
    void processInternal(float* left, float* right, int numSamples);

    //==============================================================================
    // Bandpass filter state (used in both JUCE and Standalone)
    struct BPFState {
        float x1 = 0.0f, x2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        
        float process(float x) {
            float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
        
        void reset() { x1 = x2 = y1 = y2 = 0.0f; }
    };
    
    void updateBPFCoefficients();
    BPFState mBpStates[2];
    float mEnvelope[2] = {0.0f, 0.0f};
    float mAttack = 0.999f;  // Envelope follower attack (very fast)

    //==============================================================================
    // Member variables
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==============================================================================
    // Non-copyable
    VC_DECLARE_NON_COPYABLE(VCDeEsserDSP)
};
