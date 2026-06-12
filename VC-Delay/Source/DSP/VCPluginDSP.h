#pragma once

//==============================================================================
// VC-Delay DSP Core Header - Gen2
// Multi-tap delay with BPM sync, feedback filtering, and ping-pong
// Supports both JUCE and Standalone (no dependency) modes
//==============================================================================

// Shared constants (available in both JUCE and Standalone modes)
constexpr float VC_PI = 3.14159265358979323846f;
constexpr int VC_DELAY_MAX_TAPS = 4;

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

namespace VCStandalone {
    inline float decibelsToGain(float dB) { return juce::Decibels::decibelsToGain(dB); }
    inline float gainToDecibels(float gain) { return juce::Decibels::gainToDecibels(gain); }
}

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
    // Gen2 Parameter structure
    //==========================================================================
    struct Params
    {
        // Gen1 params (preserved)
        float delayTime = 250.0f;   // Delay time ms (10~2000), used if syncBpm==0
        float feedback = 30.0f;     // Feedback amount % (0~90)
        float mix = 50.0f;          // Dry/Wet mix (0-100%)
        bool enabled = true;        // Bypass flag

        // Gen2 params
        float syncBpm = 0.0f;       // BPM for sync (0=free time mode)
        int noteValue = 4;          // Note denominator: 1,2,4,8,16,32
        bool triplet = false;       // Triplet flag
        bool dotted = false;        // Dotted flag
        int taps = 1;               // Number of taps (1-4)
        float tapTime[VC_DELAY_MAX_TAPS] = {250.0f, 500.0f, 750.0f, 1000.0f};   // ms per tap
        float tapGain[VC_DELAY_MAX_TAPS] = {0.0f, -3.0f, -6.0f, -9.0f};         // dB per tap
        bool pingPong = false;      // Ping-pong mode
        float feedbackHpf = 80.0f;  // Feedback HPF Hz
        float feedbackLpf = 8000.0f; // Feedback LPF Hz

        Params() {
            for (int i = 0; i < VC_DELAY_MAX_TAPS; ++i) {
                tapTime[i] = 250.0f * (i + 1);
                tapGain[i] = 0.0f - 3.0f * i;
            }
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

    //==========================================================================
    // BPM sync helper
    static float calcDelayFromBPM(float bpm, int noteValue, bool triplet, bool dotted) {
        // Base delay for whole note: 60000/BPM
        float baseMs = 60000.0f / bpm;
        float noteMs = baseMs * (4.0f / (float)noteValue);
        if (triplet) noteMs *= 2.0f / 3.0f;
        if (dotted) noteMs *= 1.5f;
        return noteMs;
    }

private:
    //==========================================================================
    // Internal DSP implementation
    void processInternal(float* left, float* right, int numSamples);

    //==========================================================================
    // Feedback filter (one-pole HPF + one-pole LPF)
    struct FeedbackFilter {
        float hpfX1 = 0.0f, hpfY1 = 0.0f;
        float lpfY1 = 0.0f;
        float hpfCoeff = 0.0f;  // coefficient for HPF
        float lpfCoeff = 0.0f;  // coefficient for LPF

        void update(double sampleRate, float hpfHz, float lpfHz) {
            hpfHz = VC_JCLAMP(hpfHz, 10.0f, (float)sampleRate * 0.49f);
            lpfHz = VC_JCLAMP(lpfHz, 10.0f, (float)sampleRate * 0.49f);
            float hpfOmega = 2.0f * VC_PI * hpfHz / (float)sampleRate;
            hpfCoeff = (2.0f - std::cos(hpfOmega)) - std::sqrt((2.0f - std::cos(hpfOmega)) * (2.0f - std::cos(hpfOmega)) - 1.0f);
            float lpfOmega = 2.0f * VC_PI * lpfHz / (float)sampleRate;
            lpfCoeff = 1.0f - std::exp(-lpfOmega);
        }

        void reset() {
            hpfX1 = 0.0f; hpfY1 = 0.0f;
            lpfY1 = 0.0f;
        }

        float process(float x) {
            // HPF: one-pole high-pass
            float hp = x - hpfX1 + (1.0f - hpfCoeff) * hpfY1;
            hpfX1 = x;
            hpfY1 = hp;
            // LPF: one-pole low-pass
            float lp = lpfCoeff * hp + (1.0f - lpfCoeff) * lpfY1;
            lpfY1 = lp;
            return lp;
        }
    };

    //==========================================================================
    // Member variables
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Delay buffer for circular buffer delay line (per-channel)
    std::vector<float> mDelayBufferL;
    std::vector<float> mDelayBufferR;
    int mWritePos = 0;
    int mMaxDelaySamples = 0;  // max delay = 5 seconds worth of samples

    // Feedback filter (applied in feedback path)
    FeedbackFilter mFeedbackFilterL;
    FeedbackFilter mFeedbackFilterR;

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==========================================================================
    // Non-copyable
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
