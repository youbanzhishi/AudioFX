#pragma once

//==============================================================================
// VC-Distortion DSP Core Header
// Distortion effects: Tube, Tape, Transistor, Fuzz, BitCrush
// Supports both JUCE and Standalone (no dependency) modes
//==============================================================================

constexpr float VC_PI = 3.14159265358979323846f;

#ifdef VC_STANDALONE
#include <vector>
#include <cmath>
#include <algorithm>

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
// Distortion Types
//==============================================================================
enum class DistortionType : int {
    Tube = 0,
    Tape = 1,
    Transistor = 2,
    Fuzz = 3,
    BitCrush = 4
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
        int type = 0;           // 0=Tube, 1=Tape, 2=Transistor, 3=Fuzz, 4=BitCrush
        float drive = 0.5f;     // 0~1.0 drive amount
        float mix = 1.0f;       // 0~1.0 dry/wet
        float tone = 0.5f;      // 0~1.0 tone filter (0=dark, 1=bright)
        float makeup = 0.0f;    // -30~+30 dB gain compensation
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

    // Distortion algorithms
    float processTube(float x, float driveGain);
    float processTape(float x, float driveGain);
    float processTransistor(float x, float driveGain);
    float processFuzz(float x, float driveGain);
    float processBitCrush(float x, float driveGain, int channel);

#ifdef VC_STANDALONE
    // One-pole lowpass filter for tone control
    struct OnePoleState {
        float y1 = 0.0f;
    };
    OnePoleState mToneFilter[2]; // [channel]

    // One-pole lowpass for tape saturation
    OnePoleState mTapeLP[2];

    void updateToneFilter();
    float applyToneFilter(float x, int channel);
    float applyTapeLP(float x, int channel);
#endif

    //==========================================================================
    // Member variables
    //==========================================================================
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // BitCrush downsampler state
    float mBitCrushHold[2] = {0.0f, 0.0f};  // held sample per channel
    int mBitCrushCounter = 0;

    // Smoothed drive for zipper-free automation
    float mSmoothDrive = 0.5f;

    // Tone filter coefficient
    float mToneCoeff = 0.5f;
    // Tape LP coefficient
    float mTapeLPCoeff = 0.5f;

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==========================================================================
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
