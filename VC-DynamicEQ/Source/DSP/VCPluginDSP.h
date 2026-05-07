#pragma once

//==============================================================================
// VC-DynamicEQ DSP Core Header
// Dynamic Equalizer: EQ + Compressor on specific frequency band
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
class VCPluginDSP
{
public:
    //==============================================================================
    // Dynamic EQ parameter structure
    //==============================================================================
    struct Params
    {
        float frequency = 200.0f;     // Center frequency Hz (20~20000)
        float gain = -6.0f;           // Static gain dB (-18~+18)
        float q = 1.0f;               // Q value (0.1~10)
        float threshold = -12.0f;     // Dynamic threshold dB (-48~0)
        float range = -12.0f;         // Dynamic range dB (-24~+24), negative=attenuate, positive=boost
        float attack = 10.0f;         // Attack time ms (0.1~50)
        float release = 100.0f;       // Release time ms (10~500)
        float mix = 100.0f;           // Dry/Wet mix (0~100%)
        bool enabled = true;          // Bypass flag
    };

    //==============================================================================
    // Construction / Destruction
    VCPluginDSP();
    ~VCPluginDSP();

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

#ifdef VC_STANDALONE
    // Standalone IIR filter state for biquad
    struct IIRState {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
        
        float process(float x) {
            float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
    };
    
    IIRState mEQState[2];    // Peaking EQ for L/R
    IIRState mBPState[2];    // Bandpass for detection L/R
    float mEnvelope[2] = {0.0f, 0.0f};
    
    void updateEQCoefficients();
    void updateBPCoefficients();
    void processIIR(float* left, float* right, int numSamples);
#endif

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
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
