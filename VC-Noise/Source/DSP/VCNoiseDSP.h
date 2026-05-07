#pragma once

//==============================================================================
// VC-Noise DSP Core Header
// Noise/Signal Generator: White, Pink, Brown, Sine, Sweep, Impulse
// Supports both JUCE and Standalone modes
// NOTE: This is a GENERATOR (not a processor). In standalone mode,
//       it generates signal without requiring input.
//==============================================================================

constexpr float VC_PI = 3.14159265358979323846f;

#ifdef VC_STANDALONE
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>

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
// Noise Types
//==============================================================================
enum class NoiseType : int {
    White = 0,
    Pink = 1,
    Brown = 2,
    Sine = 3,
    Sweep = 4,
    Impulse = 5
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
        int type = 0;               // 0=White, 1=Pink, 2=Brown, 3=Sine, 4=Sweep, 5=Impulse
        float frequency = 1000.0f;  // 20~20000 Hz (sine freq / sweep start)
        float endFreq = 20000.0f;   // 20~20000 Hz (sweep end)
        float sweepDuration = 5.0f;  // 1~60 seconds
        bool sweepLog = true;        // true=log sweep, false=linear
        float volume = -6.0f;        // -60~0 dBFS
        int channelMode = 0;         // 0=stereo, 1=left, 2=right, 3=anti-phase
        float pulsePeriod = 0.0f;    // 0~10 seconds (0=single impulse)
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
    // Generator-specific: generate signal (no input needed)
    //==========================================================================
    void generate(float* left, float* right, int numSamples);

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

    // Get current sample position (for tracking progress)
    long long getSamplePosition() const { return mSamplePos; }

    // Reset sample position (e.g., for new generation)
    void resetSamplePosition() { mSamplePos = 0; }

private:
    //==========================================================================
    // Noise generators
    //==========================================================================
    float generateWhite();
    float generatePink();
    float generateBrown();
    float generateSine();
    float generateSweep();
    float generateImpulse();

    // Simple uniform random [-1, 1]
    float randomUniform();

    //==========================================================================
    // Member variables
    //==========================================================================
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Phase accumulator for sine/sweep
    double mPhase = 0.0;

    // Brown noise integrator
    float mBrownState = 0.0f;

    // Pink noise: Voss-McCartney algorithm (8 rows)
    static constexpr int PINK_NUM_ROWS = 8;
    float mPinkRows[PINK_NUM_ROWS] = {0.0f};
    int mPinkIndex = 0;
    float mPinkRunningSum = 0.0f;

    // Impulse tracking
    bool mImpulseFired = false;
    long long mImpulseCounter = 0;

    // Sample position counter
    long long mSamplePos = 0;

    // Random state (simple LCG for reproducibility)
    unsigned int mRandState = 12345;

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==========================================================================
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
