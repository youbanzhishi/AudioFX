#pragma once

//==============================================================================
// VC-DynamicEQ DSP Core Header - Gen2
// Multi-band Dynamic EQ with sidechain, adjustable attack/release, band types
// Supports both JUCE and Standalone (no dependency) modes
//==============================================================================

constexpr float VC_PI = 3.14159265358979323846f;
constexpr int VC_DYN_EQ_MAX_BANDS = 4;

#ifdef VC_STANDALONE
// Standalone mode: no JUCE dependency
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>

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
// Band type enumeration
//==============================================================================
enum class VCBandType {
    Bell = 0,
    LowShelf = 1,
    HighShelf = 2,
    Notch = 3
};

//==============================================================================
// Main DSP Class
//==============================================================================
class VCPluginDSP
{
public:
    //==========================================================================
    // Gen2 Multi-band Dynamic EQ parameter structure
    //==========================================================================
    struct BandParams {
        float frequency = 200.0f;      // Center/cutoff frequency Hz
        float q = 1.0f;                // Q value
        VCBandType type = VCBandType::Bell;  // Band type
        float threshold = -12.0f;      // Dynamic threshold dB
        float ratio = 3.0f;            // Compression ratio
        float attack = 10.0f;          // Attack time ms
        float release = 100.0f;        // Release time ms
        float gain = -6.0f;            // Static gain dB (makeup/offset)

        BandParams() {}
    };

    struct Params
    {
        // Gen1 compatibility (maps to band 0)
        float frequency = 200.0f;     // Center frequency Hz (20~20000)
        float gain = -6.0f;           // Static gain dB (-18~+18)
        float q = 1.0f;               // Q value (0.1~10)
        float threshold = -12.0f;     // Dynamic threshold dB (-48~0)
        float range = -12.0f;         // Dynamic range dB (-24~+24) [Gen1 compat]
        float attack = 10.0f;         // Attack time ms (0.1~50)
        float release = 100.0f;       // Release time ms (10~500)
        float mix = 100.0f;           // Dry/Wet mix (0~100%)
        bool enabled = true;          // Bypass flag

        // Gen2 params
        int bands = 1;                // Number of active bands (1-4)
        BandParams band[VC_DYN_EQ_MAX_BANDS];  // Per-band params
        int sidechain = 0;            // 0=internal, 1=external (placeholder)

        Params() {
            // Initialize all bands with staggered defaults
            band[0] = BandParams();
            band[1] = BandParams(); band[1].frequency = 800.0f; band[1].gain = -3.0f;
            band[2] = BandParams(); band[2].frequency = 3000.0f; band[2].gain = -2.0f;
            band[3] = BandParams(); band[3].frequency = 8000.0f; band[3].gain = -1.0f;
        }
    };

    //==========================================================================
    // Construction / Destruction
    VCPluginDSP();
    ~VCPluginDSP();

    //==========================================================================
    // Processing
    void prepare(double sampleRate, int blockSize);
    void process(float* left, float* right, int numSamples);

#ifndef VC_STANDALONE
    // JUCE AudioBlock processing (non-interleaved)
    void process(juce::dsp::AudioBlock<float>& block);
#endif

    void reset();

    //==========================================================================
    // Parameter access
    void setParams(const Params& p);
    Params getParams() const;

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

    //==========================================================================
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
    //==========================================================================
    // Internal DSP implementation
    void processInternal(float* left, float* right, int numSamples);

    //==========================================================================
    // Biquad filter state (per band, per channel)
    struct BiquadState {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

        float process(float x) {
            float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }

        void reset() {
            x1 = x2 = y1 = y2 = 0.0f;
        }
    };

    //==========================================================================
    // Per-band processor state
    struct BandProcessor {
        BiquadState eqState[2];      // EQ filter for L/R
        BiquadState bpState[2];      // Bandpass for detection L/R
        float envelope[2] = {0.0f, 0.0f};  // Envelope followers
        float smoothGain[2] = {1.0f, 1.0f}; // Smoothed dynamic gain
    };

    BandProcessor mBandProc[VC_DYN_EQ_MAX_BANDS];

    void updateBandCoefficients(int bandIdx);
    void updateAllBandCoefficients();

    //==========================================================================
    // Member variables
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==========================================================================
    // Non-copyable
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
