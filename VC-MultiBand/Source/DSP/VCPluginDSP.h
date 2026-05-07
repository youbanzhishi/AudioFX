#pragma once

//==============================================================================
// VC-MultiBand DSP Core Header
// 4-band LR4 Linkwitz-Riley crossover with per-band gain & compression
// Supports both JUCE and Standalone (no dependency) modes
//==============================================================================

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

#define VC_DECLARE_NON_COPYABLE(x)
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
    // Number of frequency bands
    //==========================================================================
    static constexpr int kNumBands = 4;

    //==========================================================================
    // Plugin-specific parameter structure
    //==========================================================================
    struct Params
    {
        // Crossover frequencies (Hz)
        float xover1 = 120.0f;      // Low / Mid-Low boundary
        float xover2 = 1000.0f;     // Mid-Low / Mid-High boundary
        float xover3 = 8000.0f;     // Mid-High / High boundary

        // Per-band gain (dB) - 4 values
        float bandGain[kNumBands] = {0.0f, 0.0f, 0.0f, 0.0f};

        // Per-band compressor threshold (dB) - 4 values
        float bandThreshold[kNumBands] = {0.0f, 0.0f, 0.0f, 0.0f};

        // Per-band compressor ratio - 4 values (>1 = compression)
        float bandRatio[kNumBands] = {1.0f, 1.0f, 1.0f, 1.0f};

        // Solo band (0 = none, 1-4 = solo that band)
        int soloBand = 0;

        // Muted bands (bitmask: bit0=band1, bit1=band2, etc.)
        int muteBands = 0;

        // Master bypass
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
    // LR4 Crossover: 4th-order Linkwitz-Riley
    // = cascade of two identical 2nd-order Butterworth filters
    //==========================================================================
    struct SecondOrderBiquad {
        // Coefficients (normalized: a0=1)
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        // State (Direct Form I)
        float x1 = 0.0f, x2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;

        void reset() {
            x1 = x2 = y1 = y2 = 0.0f;
        }

        inline float processSample(float x) {
            float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
    };

    // Each crossover has LP and HP, each is 2 cascaded biquads = LR4
    struct LR4Crossover {
        SecondOrderBiquad lpStage1;  // LP 1st biquad
        SecondOrderBiquad lpStage2;  // LP 2nd biquad (same coefficients)
        SecondOrderBiquad hpStage1;  // HP 1st biquad
        SecondOrderBiquad hpStage2;  // HP 2nd biquad (same coefficients)

        void reset() {
            lpStage1.reset(); lpStage2.reset();
            hpStage1.reset(); hpStage2.reset();
        }

        // Process one sample: outputs low and high bands
        inline void processSample(float x, float& lowOut, float& highOut) {
            // LP path: two identical 2nd-order LP in series
            float lp = lpStage1.processSample(x);
            lp = lpStage2.processSample(lp);
            lowOut = lp;

            // HP path: two identical 2nd-order HP in series
            float hp = hpStage1.processSample(x);
            hp = hpStage2.processSample(hp);
            highOut = hp;
        }
    };

    // Per-channel crossover state (3 crossovers for 4 bands)
    struct CrossoverChain {
        LR4Crossover xover1;  // Low vs (Mid-Low, Mid-High, High)
        LR4Crossover xover2;  // Mid-Low vs (Mid-High, High)
        LR4Crossover xover3;  // Mid-High vs High
    };

    //==========================================================================
    // Per-band simple compressor
    //==========================================================================
    struct BandCompressor {
        float thresholdDB = 0.0f;
        float ratio = 1.0f;
        // Envelope follower
        float envelope = 0.0f;
        // Attack/release coefficients
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;

        void prepare(double sampleRate) {
            // Attack: 10ms, Release: 100ms (fixed for simplicity)
            float attackMs = 10.0f;
            float releaseMs = 100.0f;
            attackCoeff = std::exp(-1.0f / (attackMs * 0.001f * static_cast<float>(sampleRate)));
            releaseCoeff = std::exp(-1.0f / (releaseMs * 0.001f * static_cast<float>(sampleRate)));
            envelope = 0.0f;
        }

        void reset() {
            envelope = 0.0f;
        }

        inline float processSample(float x) {
            if (ratio <= 1.0f) return x;  // No compression

            float inputLevel = std::fabs(x);
            // Envelope follower
            if (inputLevel > envelope) {
                envelope = attackCoeff * envelope + (1.0f - attackCoeff) * inputLevel;
            } else {
                envelope = releaseCoeff * envelope + (1.0f - releaseCoeff) * inputLevel;
            }

            if (envelope <= 1e-10f) return x;

            float levelDB = 20.0f * std::log10(envelope);
            if (levelDB < thresholdDB) return x;

            // Compute gain reduction
            float overDB = levelDB - thresholdDB;
            float reducedDB = thresholdDB + overDB / ratio;
            float gainReductionDB = reducedDB - levelDB;
            float gain = std::pow(10.0f, gainReductionDB / 20.0f);

            return x * gain;
        }
    };

    //==========================================================================
    // Internal processing
    //==========================================================================
    void processInternal(float* left, float* right, int numSamples);
    void updateCrossoverCoefficients();

    //==========================================================================
    // Member variables
    //==========================================================================
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Per-channel crossover chains
    CrossoverChain mCrossovers[2];  // [left, right]

    // Per-band compressors (per channel)
    BandCompressor mCompressors[2][kNumBands];  // [channel][band]

    // Internal buffer for AudioBlock conversion (JUCE mode)
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    // Per-band output buffers (for split processing)
    std::vector<float> mBandBuffers[kNumBands][2];  // [band][channel]

    //==========================================================================
    // Non-copyable
    //==========================================================================
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
