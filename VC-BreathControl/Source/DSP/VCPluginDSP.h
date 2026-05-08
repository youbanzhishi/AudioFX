#pragma once

//==============================================================================
// VC-BreathControl DSP Core Header
// Automatic breath detection and gain control plugin
// Supports both JUCE and Standalone modes
//
// Algorithm:
//   1. Bandpass filter (200Hz~4kHz) extracts breath-frequency content
//   2. Envelope follower tracks energy in the breath band
//   3. Simplified spectral flatness estimation (bandpass energy ratio)
//   4. Dual-criteria detection: energy < threshold AND spectral flatness > sfThreshold
//   5. State machine with hysteresis (3dB gap) to prevent chattering
//   6. Minimum duration filtering removes short false positives
//   7. Gain control with auto_smooth or manual fade modes
//   8. Lookahead buffer aligns gain decisions with audio
//==============================================================================

constexpr float VC_PI = 3.14159265358979323846f;

#ifdef VC_STANDALONE
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
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
// Breath region detected
//==============================================================================
struct BreathRegion {
    int startSample;
    int endSample;
    float peakDb;              // Peak envelope dB in region
    float avgDb;               // Average envelope dB in region
    float spectralFlatness;    // Average spectral flatness in region
    float appliedReductionDb;  // Gain applied to this region
};

//==============================================================================
// Envelope Follower (from VC-Gate)
//==============================================================================
class BreathEnvelopeFollower
{
public:
    BreathEnvelopeFollower() = default;

    void reset() { envelope = 0.0f; }

    void setAttackTime(float timeMs, float sampleRate)
    {
        attackCoef = std::exp(-1.0f / (timeMs * 0.001f * sampleRate));
        attackCoefInv = 1.0f - attackCoef;
    }

    void setReleaseTime(float timeMs, float sampleRate)
    {
        releaseCoef = std::exp(-1.0f / (timeMs * 0.001f * sampleRate));
        releaseCoefInv = 1.0f - releaseCoef;
    }

    float processSample(float input)
    {
        float inputAbs = std::abs(input);
        float sq = inputAbs * inputAbs;

        if (sq > envelope)
            envelope = attackCoefInv * sq + attackCoef * envelope;
        else
            envelope = releaseCoefInv * sq + releaseCoef * envelope;

        return envelope;
    }

    float getEnvelope() const { return envelope; }

private:
    float envelope = 0.0f;
    float attackCoef = 0.0f, attackCoefInv = 1.0f;
    float releaseCoef = 0.0f, releaseCoefInv = 1.0f;
};

//==============================================================================
// Lookahead Buffer (from VC-Gate)
//==============================================================================
class BreathLookaheadBuffer
{
public:
    BreathLookaheadBuffer() = default;

    void reset()
    {
        std::fill(bufferL.begin(), bufferL.end(), 0.0f);
        std::fill(bufferR.begin(), bufferR.end(), 0.0f);
        writePos = 0;
    }

    void prepare(float lookaheadMs, double sampleRate)
    {
        int samples = static_cast<int>(lookaheadMs * 0.001f * static_cast<float>(sampleRate));
        size = VC_JMAX(samples, 1);
        bufferL.resize(size, 0.0f);
        bufferR.resize(size, 0.0f);
        writePos = 0;
    }

    void process(float& left, float& right)
    {
        float delayedL = bufferL[writePos];
        float delayedR = bufferR[writePos];
        bufferL[writePos] = left;
        bufferR[writePos] = right;
        writePos = (writePos + 1) % size;
        left = delayedL;
        right = delayedR;
    }

    int getSize() const { return size; }

private:
    std::vector<float> bufferL;
    std::vector<float> bufferR;
    int size = 1;
    int writePos = 0;
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
        float threshold = -40.0f;        // -60~-10 dBFS, breath detection energy threshold
        float reduction = -8.0f;         // -18~+12 dB, gain adjustment for breath regions
        float attack = 10.0f;            // 1~100 ms, gain decrease time (auto_smooth=true)
        float release = 50.0f;           // 10~500 ms, gain recovery time (auto_smooth=true)
        bool  autoSmooth = true;         // true=attack/release smoothing, false=step+fade
        float fadeIn = 2.0f;             // 0~20 ms, micro-fade at breath start (auto_smooth=false)
        float fadeOut = 5.0f;            // 0~20 ms, micro-fade at breath end (auto_smooth=false)
        float minBreathDuration = 50.0f; // 10~500 ms, minimum breath duration
        float sensitivity = 0.5f;        // 0.1~1.0, spectral flatness weight
        float lookahead = 5.0f;          // 0~10 ms, lookahead buffer time
        float freqLow = 200.0f;          // Hz, detection band low cutoff
        float freqHigh = 4000.0f;        // Hz, detection band high cutoff
        float sfThreshold = 0.6f;        // 0.3~0.9, spectral flatness threshold
        bool  enabled = true;
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
    void detectBreaths(float* left, float* right, int numSamples);
    void processBreaths(float* left, float* right, int numSamples);
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
    const std::vector<BreathRegion>& getBreathRegions() const { return mBreathRegions; }
    void clearRegions() { mBreathRegions.clear(); }

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

    // Bandpass filter (Biquad 2nd order Butterworth)
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

    // Full-band envelope for spectral flatness estimation
    BreathEnvelopeFollower mFullBandEnvelope;
    float mFullBandAttackCoeff = 0.0f;
    float mFullBandReleaseCoeff = 0.0f;

    // Lookahead buffer
    BreathLookaheadBuffer mLookahead;

    // Gate gain smoothing
    float mAttackCoef = 0.0f;
    float mAttackCoefInv = 1.0f;
    float mReleaseCoef = 0.0f;
    float mReleaseCoefInv = 1.0f;

    // State machine
    enum class BreathState { IDLE, BREATH };
    BreathState mState = BreathState::IDLE;
    int mBreathStartSample = 0;
    float mCurrentGainDb = 0.0f;

    //==========================================================================
    // Member variables
    //==========================================================================
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Detected breath regions
    std::vector<BreathRegion> mBreathRegions;

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==========================================================================
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
