#pragma once

//==============================================================================
// VC-PitchShift DSP Core Header - High-Quality Pitch Shifting (Phase Vocoder)
// Supports both JUCE and Standalone (no dependency) modes
//
// Algorithm: Phase Vocoder
//   1. STFT Analysis: windowed FFT with window size 2048, hop size 512
//   2. Phase accumulation/unwrapping: maintain phase coherence
//   3. Resampling in frequency domain: stretch/compress phase increments
//   4. ISTFT Synthesis: overlap-add reconstruction
//   5. Optional formant preservation: separate spectral envelope from pitch
//
// Pitch shift range: -12 to +12 semitones, with cents micro-adjustment
//==============================================================================

// Shared constants (available in both JUCE and Standalone modes)
constexpr float VC_PI = 3.14159265358979323846f;

#ifdef VC_STANDALONE
// Standalone mode: no JUCE dependency
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>

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
// Phase Vocoder constants
//==============================================================================
constexpr int PV_FFT_SIZE   = 2048;   // FFT window size
constexpr int PV_HOP_SIZE   = 512;    // Hop size (75% overlap with Hann)
constexpr int PV_FFT_SIZE_2 = PV_FFT_SIZE / 2 + 1;  // Unique frequency bins

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
        int   semitones = 0;       // -12 ~ +12 semitones
        float cents     = 0.0f;    // -100 ~ +100 cents (micro-tuning)
        bool  formant   = false;   // Formant preservation mode
        bool  enabled   = true;    // Bypass flag
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

    // Phase vocoder processing for one channel
    void processChannelPV(float* input, float* output, int numSamples, int channel);

    // Standalone FFT (Cooley-Tukey radix-2)
    void fft(float* real, float* imag, int N, bool inverse);

    // Compute Hann window
    void computeHannWindow(float* window, int size);

    // Update pitch ratio from params
    void updatePitchRatio();

    //==========================================================================
    // Member variables
    //==========================================================================
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Pitch shift ratio (computed from semitones + cents)
    float mPitchRatio = 1.0f;

    // Phase vocoder state per channel
    struct PVState {
        std::vector<float> prevPhase;        // Previous analysis phase [PV_FFT_SIZE_2]
        std::vector<float> synthPhase;       // Synthesis phase accumulator [PV_FFT_SIZE_2]
    };
    PVState mPVState[2];  // [channel]

    // Hann window
    std::vector<float> mHannWindow;

    // FFT workspace (per-call, non-shared between channels)
    std::vector<float> mFFTReal;
    std::vector<float> mFFTImag;

    // Formant preservation: spectral envelope storage
    std::vector<float> mSpectralEnvelope[2];  // [channel][PV_FFT_SIZE_2]

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==========================================================================
    // Non-copyable
    //==========================================================================
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
