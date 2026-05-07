#pragma once

//==============================================================================
// VC-PitchShift DSP Core Header - High-Quality Pitch Shifting (Phase Vocoder)
// Supports both JUCE and Standalone (no dependency) modes
//==============================================================================

constexpr float VC_PI = 3.14159265358979323846f;

#ifdef VC_STANDALONE
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>

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
// Phase Vocoder constants
//==============================================================================
constexpr int PV_FFT_SIZE   = 2048;
constexpr int PV_HOP_SIZE   = 512;
constexpr int PV_FFT_SIZE_2 = PV_FFT_SIZE / 2 + 1;

//==============================================================================
// Main DSP Class
//==============================================================================
class VCPluginDSP
{
public:
    struct Params
    {
        int   semitones = 0;
        float cents     = 0.0f;
        bool  formant   = false;
        bool  enabled   = true;
    };

    VCPluginDSP();
    ~VCPluginDSP();

    void prepare(double sampleRate, int blockSize);
    void process(float* left, float* right, int numSamples);

#ifndef VC_STANDALONE
    void process(juce::dsp::AudioBlock<float>& block);
#endif

    void reset();

    void setParams(const Params& p);
    Params getParams() const;

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

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
    void processInternal(float* left, float* right, int numSamples);
    void processChannelPV(const float* input, float* output, int numSamples, int channel);
    void fft(float* real, float* imag, int N, bool inverse);
    void computeHannWindow(float* window, int size);
    void updatePitchRatio();

    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;
    float mPitchRatio = 1.0f;

    struct PVState {
        std::vector<float> prevPhase;
        std::vector<float> synthPhase;
    };
    PVState mPVState[2];

    std::vector<float> mHannWindow;
    std::vector<float> mFFTReal;
    std::vector<float> mFFTImag;
    std::vector<float> mSpectralEnvelope[2];

    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    // Input copy buffer (for safe in-place processing)
    std::vector<float> mInputCopy[2];

    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
