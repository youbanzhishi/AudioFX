#pragma once

//==============================================================================
// VC-Delay DSP Core Header
// Supports both JUCE and Standalone (no dependency) modes
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
    //==============================================================================
    // Plugin-specific parameter structure
    //==============================================================================
    struct Params
    {
        float delayTime = 250.0f;   // Delay time ms (10~2000)
        float feedback = 30.0f;    // Feedback amount % (0~90)
        float mix = 50.0f;          // Dry/Wet mix (0-100%)
        bool enabled = true;        // Bypass flag
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
// Shared constants (available in both JUCE and Standalone modes)

#ifdef VC_STANDALONE
        return VCStandalone::decibelsToGain(dB);
#else
        return juce::Decibels::decibelsToGain(dB);
#endif
    }

    static float linearToDb(float linear) {
// Shared constants (available in both JUCE and Standalone modes)

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
    // Member variables
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Delay buffer for circular buffer delay line
    std::vector<float> mDelayBuffer;  // interleaved stereo [L,R,L,R...]
    int mWritePos = 0;
    int mReadPos = 0;
    int mMaxDelaySamples = 0;  // max delay = 2 seconds worth of samples

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==============================================================================
    // Non-copyable
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
