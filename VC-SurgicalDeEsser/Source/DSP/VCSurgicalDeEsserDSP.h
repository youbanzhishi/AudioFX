#pragma once

//==============================================================================
// VC-SurgicalDeEsser DSP Core Header
// Surgical de-esser: two-pass detection + processing with crossfade
// Supports both JUCE and Standalone modes
//==============================================================================

constexpr float VC_PI = 3.14159265358979323846f;

#ifdef VC_STANDALONE
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <sstream>

namespace VCStandalone {
    inline float decibelsToGain(float dB) { return std::pow(10.0f, dB / 20.0f); }
    inline float gainToDecibels(float gain) { return 20.0f * std::log10(std::max(gain, 1e-10f)); }
}

#define VC_DECLARE_NON_COPYABLE(x)
#define VC_JMIN(a, b) std::min(a, b)
#define VC_JMAX(a, b) std::max(a, b)
#define VC_JCLAMP(a, b, c) std::clamp(a, b, c)
#else
#include <juce_dsp/juce_dsp.h>

#define VC_DECLARE_NON_COPYABLE(x) JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(x)
#define VC_JMIN(a, b) juce::jmin(a, b)
#define VC_JMAX(a, b) juce::jmax(a, b)
#define VC_JCLAMP(a, b, c) juce::jlimit(a, b, c)
#endif

//==============================================================================
// Sibilance region detected
//==============================================================================
struct SibilanceRegion {
    int startSample;
    int endSample;
    float peakDb;
    float actualReductionDb;
};

//==============================================================================
// De-Esser Modes
//==============================================================================
enum class DeEsserMode : int {
    Gain = 0,   // Simple gain reduction (preferred, no phase issues)
    DynEQ = 1   // Dynamic EQ (has potential phase issues)
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
        float threshold = -30.0f;   // -60~0 dBFS, detection threshold
        float reduction = 6.0f;     // 0~20 dB, max attenuation
        float minDuration = 20.0f;  // 5~100 ms, minimum sibilance duration
        float fadeTime = 5.0f;      // 0.5~10 ms, crossfade time
        float freqLow = 5000.0f;    // 2000~8000 Hz, detection band low
        float freqHigh = 9000.0f;   // 5000~14000 Hz, detection band high
        int mode = 0;               // 0=gain, 1=dynEQ
        bool enabled = true;
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
    // Two-pass processing (CLI mode)
    //==========================================================================
    // Pass 1: detect sibilance regions
    void detectSibilance(float* left, float* right, int numSamples);

    // Pass 2: apply processing based on detected regions
    void processSibilance(float* left, float* right, int numSamples);

    // Two-pass convenience: detect then process
    void processTwoPass(float* left, float* right, int numSamples);

    //==========================================================================
    // Parameter access
    //==========================================================================
    void setParams(const Params& p);
    Params getParams() const;

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

    //==========================================================================
    // Detection results
    //==========================================================================
    const std::vector<SibilanceRegion>& getSibilanceRegions() const { return mRegions; }
    void clearRegions() { mRegions.clear(); }

    // Generate report string
    std::string generateReport() const;

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
    // Internal DSP
    //==========================================================================

    // Bandpass filter (for sibilance detection)
    struct BiquadState {
        float x1 = 0.0f, x2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;
    };

    struct BiquadCoeffs {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
    };

    BiquadCoeffs mBPCoeffs;
    BiquadState mBPState[2]; // [channel]

    void updateBPCoefficients();
    float processBiquad(float x, BiquadState& state);

    // Envelope follower
    float mEnvelope = 0.0f;
    float mAttackCoeff = 0.0f;
    float mReleaseCoeff = 0.0f;

    // Real-time processing state (for VST3/streaming mode)
    bool mInSibilance = false;
    int mSibilanceStart = 0;
    float mCurrentGain = 1.0f;
    int mFadeInCount = 0;
    int mFadeOutCount = 0;

    // Lookahead buffer (for VST3 mode)
    int mLookaheadSamples = 0;
    std::vector<float> mLookaheadBuffer[2];
    int mLookaheadWritePos = 0;
    int mLookaheadReadPos = 0;
    bool mLookaheadFilled = false;

    //==========================================================================
    // Member variables
    //==========================================================================
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Detected sibilance regions (from pass 1)
    std::vector<SibilanceRegion> mRegions;

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==========================================================================
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
