// ============================================================================
// VCCompDSP.h - VC-Compressor DSP Engine
// Gen2: Added 4-band multiband compression with LR4 crossover
// Original Gen1 code preserved in VCCompDSP.h.gen1 / VCCompDSP.cpp.gen1
// ============================================================================

#pragma once

// Shared constants (available in both JUCE and Standalone modes)
constexpr float VC_PI = 3.14159265358979323846f;

#ifdef VC_STANDALONE
// Standalone mode: use standard library math, no JUCE dependency
#include <vector>
#include <cmath>
#include <algorithm>

namespace VCStandalone {
    inline float decibelsToGain(float dB) { return std::pow(10.0f, dB / 20.0f); }
    inline float gainToDecibels(float gain) { return 20.0f * std::log10(std::max(gain, 1e-10f)); }
}

#define VC_DECLARE_NON_COPYABLE(x) // No-op in standalone
#define VC_JMIN(a, b) std::min(a, b)
#define VC_JMAX(a, b) std::max(a, b)
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
#endif

//==============================================================================
// Helper Classes (kept internal for DSP module)
//==============================================================================

class EnvelopeFollower
{
public:
    EnvelopeFollower() = default;
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

class SoftKneeCompressor
{
public:
    float computeGainReduction(float inputLevelDb, float threshold, float ratio, float kneeWidth = 6.0f)
    {
        float x = inputLevelDb;
        float T = threshold;
        float R = ratio;
        float W = kneeWidth;
        float halfW = W / 2.0f;
        
        if (x <= T - halfW)
            return 0.0f;
        else if (x >= T + halfW)
            return (x - T) * (1.0f - 1.0f / R);
        else
        {
            float offset = x - (T - halfW);
            float slope = (1.0f - 1.0f / R) / W;
            return slope * offset * offset / (2.0f * W);
        }
    }
};

class ARCCalculator
{
public:
    float calculateARC(float inputLevelDb, float gainReductionDb, float currentReleaseMs)
    {
        peakHistory.push_back(inputLevelDb);
        if (peakHistory.size() > maxHistorySize)
            peakHistory.erase(peakHistory.begin());
        
        float avgPeak = 0.0f;
        for (float p : peakHistory)
            avgPeak += p;
        avgPeak /= peakHistory.size();
        
        float peakDelta = inputLevelDb - avgPeak;
        float arcFactor = 1.0f;
        
        if (peakDelta > 6.0f) arcFactor = 0.1f;
        else if (peakDelta > 3.0f) arcFactor = 0.3f;
        else if (peakDelta > 0.0f) arcFactor = 0.5f;
        else if (peakDelta > -3.0f) arcFactor = 1.0f;
        else if (peakDelta > -6.0f) arcFactor = 2.0f;
        else arcFactor = 5.0f;
        
        return arcFactor;
    }
    
    void reset() { peakHistory.clear(); }
    
private:
    std::vector<float> peakHistory;
    static constexpr int maxHistorySize = 64;
};

class SCHPF
{
public:
    void setFrequency(float freqHz, float sampleRate)
    {
#ifdef VC_STANDALONE
        float omega = 2.0f * VC_PI * freqHz / sampleRate;
#else
        float omega = 2.0f * juce::MathConstants<float>::pi * freqHz / sampleRate;
#endif
        float sinOmega = std::sin(omega);
        float cosOmega = std::cos(omega);
        float alpha = sinOmega / (2.0f * 1.4142f);
        
        b0 = (1.0f + cosOmega) / 2.0f;
        b1 = -(1.0f + cosOmega);
        b2 = (1.0f + cosOmega) / 2.0f;
        float a0 = 1.0f + alpha;
        a1 = -2.0f * cosOmega;
        a2 = 1.0f - alpha;
        
        b0 /= a0; b1 /= a0; b2 /= a0;
        a1 /= a0; a2 /= a0;
    }
    
    void reset() { x1 = x2 = y1 = y2 = 0.0f; }
    
    float processSample(float x)
    {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
    
private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
};

class WarmCharacter
{
public:
    void processSample(float& leftSample, float& rightSample, float gainReductionDb)
    {
        if (!enabled) return;
        if (gainReductionDb < 0.1f) return;
        
        float grNorm = VC_JMIN(gainReductionDb / 20.0f, 1.0f);
        
        for (int i = 0; i < 2; ++i)
        {
            float* sample = (i == 0) ? &leftSample : &rightSample;
            float x = *sample;
            float sign = (x >= 0.0f) ? 1.0f : -1.0f;
            float absX = std::abs(x);
            float distorted = std::tanh(absX * 3.0f) * sign;
            float harmonicMix = distorted - x;
            *sample += harmonicMix * grNorm * 0.1f;
        }
    }
    
    void setEnabled(bool e) { enabled = e; }
    
private:
    bool enabled = false;
};

class VCLimiter
{
public:
    void reset() { peakHold = 0.0f; limiterActive = false; limiterAmount = 0.0f; }
    
    float processSample(float input)
    {
        float absInput = std::abs(input);
        
        if (absInput > peakHold)
            peakHold = absInput * 0.9f + peakHold * 0.1f;
        else
            peakHold = peakHold * 0.999f;
        
        if (peakHold > 1.0f)
        {
            limiterActive = true;
            limiterAmount = peakHold;
            return 1.0f * (input >= 0.0f ? 1.0f : -1.0f);
        }
        else
        {
            limiterActive = (peakHold > 0.0f);
            limiterAmount = VC_JMAX(0.0f, peakHold - 1.0f);
            return input;
        }
    }
    
    bool isLimiterActive() const { return limiterActive; }
    float getLimiterAmount() const { return limiterAmount; }
    
private:
    float peakHold = 0.0f;
    bool limiterActive = false;
    float limiterAmount = 0.0f;
};

//==============================================================================
// Gen2: LR4 Crossover (4th-order Linkwitz-Riley, 24dB/oct)
// Each LR4 = two identical 2nd-order Butterworth stages cascaded
// LP + HP at same freq sum to allpass (flat magnitude response)
//==============================================================================

struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    // State: direct form II transposed is more stable, but DF1 is fine for audio
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

    void reset() { x1 = x2 = y1 = y2 = 0.0f; }

    float process(float x) {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
};

// LR4 = two identical 2nd-order Butterworth cascaded
struct LR4 {
    Biquad stage1, stage2;

    void reset() { stage1.reset(); stage2.reset(); }
    float process(float x) { return stage2.process(stage1.process(x)); }
};

// One crossover point = LP output (low freq) + HP output (high freq)
struct Crossover {
    LR4 lp, hp;

    void reset() { lp.reset(); hp.reset(); }
};

// Set Butterworth 2nd-order LP coefficients
inline void setButterworthLP(Biquad& bq, float freqHz, float sampleRate) {
    float omega = 2.0f * VC_PI * freqHz / sampleRate;
    float sinO  = std::sin(omega);
    float cosO  = std::cos(omega);
    // Butterworth Q = 0.7071 = 1/sqrt(2)
    float alpha = sinO / (2.0f * 0.7071067811865476f);

    float a0 = 1.0f + alpha;
    bq.b0 = ((1.0f - cosO) / 2.0f) / a0;
    bq.b1 = (1.0f - cosO) / a0;
    bq.b2 = ((1.0f - cosO) / 2.0f) / a0;
    bq.a1 = -2.0f * cosO / a0;
    bq.a2 = (1.0f - alpha) / a0;
}

// Set Butterworth 2nd-order HP coefficients
inline void setButterworthHP(Biquad& bq, float freqHz, float sampleRate) {
    float omega = 2.0f * VC_PI * freqHz / sampleRate;
    float sinO  = std::sin(omega);
    float cosO  = std::cos(omega);
    float alpha = sinO / (2.0f * 0.7071067811865476f);

    float a0 = 1.0f + alpha;
    bq.b0 = ((1.0f + cosO) / 2.0f) / a0;
    bq.b1 = -(1.0f + cosO) / a0;
    bq.b2 = ((1.0f + cosO) / 2.0f) / a0;
    bq.a1 = -2.0f * cosO / a0;
    bq.a2 = (1.0f - alpha) / a0;
}

// Configure an LR4 LP filter
inline void setLR4LP(LR4& lr4, float freqHz, float sampleRate) {
    setButterworthLP(lr4.stage1, freqHz, sampleRate);
    setButterworthLP(lr4.stage2, freqHz, sampleRate);
}

// Configure an LR4 HP filter
inline void setLR4HP(LR4& lr4, float freqHz, float sampleRate) {
    setButterworthHP(lr4.stage1, freqHz, sampleRate);
    setButterworthHP(lr4.stage2, freqHz, sampleRate);
}

//==============================================================================
// Gen2: Per-band compressor state (one per band, per channel)
//==============================================================================

struct BandCompressorState {
    EnvelopeFollower envelopeFollower;
    float gainReduction = 0.0f;
    float currentRelease = 160.0f;

    void reset() {
        envelopeFollower.reset();
        gainReduction = 0.0f;
        currentRelease = 160.0f;
    }
};

//==============================================================================
// VCCompDSP - Main DSP Class (Gen2 with multiband support)
//==============================================================================

class VCCompDSP
{
public:
    static constexpr int NUM_BANDS = 4;

    struct Params
    {
        // Gen1 parameters (fully compatible, used as global defaults in multiband)
        float threshold = 0.0f;     // dB
        float ratio = 1.0f;
        float attack = 16.0f;      // ms
        float release = 160.0f;    // ms
        float gain = 0.0f;         // makeup gain dB
        int releaseMode = 0;        // 0=ARC, 1=Manual
        int compBehavior = 0;       // 0=Electro, 1=Opto
        int kneeMode = 1;           // 0=Hard, 1=Soft
        int character = 1;          // 0=Clean, 1=Warm
        float mix = 100.0f;        // %
        float trim = 0.0f;         // dB

        // Gen2 multiband parameters
        int multiband = 0;          // 0=single-band (Gen1 compatible), 1=4-band multiband
        float xoverFreqs[3] = {120.0f, 1000.0f, 8000.0f};  // 3 crossover points -> 4 bands

        // Per-band parameters (used when multiband=1)
        float bandThreshold[NUM_BANDS] = {-20.0f, -18.0f, -22.0f, -30.0f};
        float bandRatio[NUM_BANDS]     = {4.0f, 2.5f, 3.0f, 2.0f};
        float bandMakeup[NUM_BANDS]    = {0.0f, 0.0f, 0.0f, 0.0f};
        int soloBand = 0;            // 0=all, 1-4=solo that band
    };
    
    VCCompDSP();
    ~VCCompDSP();
    
    void prepare(double sampleRate, int blockSize);
    void process(float* left, float* right, int numSamples);
    void reset();
    
    void setParams(const Params& p);
    Params getParams() const { return mParams; }
    
    float getGainReduction() const { return mGainReductionDB; }
    bool isLimiterYellow() const { return mLimiter.isLimiterActive(); }
    bool isLimiterRed() const { return mLimiter.getLimiterAmount() > 0.5f; }
    
    void setEnabled(bool enabled) { mEnabled = enabled; }
    bool isEnabled() const { return mEnabled; }
    
    // Utility functions
    static float dBToLinear(float dB) { return std::pow(10.0f, dB / 20.0f); }
    static float linearToDb(float linear) { return 20.0f * std::log10(std::max(linear, 1e-10f)); }
    
private:
    float getKneeWidth(int kneeMode, float currentGR);
    void updateCrossoverFilters();
    void processSingleBand(float* left, float* right, int numSamples);
    void processMultiband(float* left, float* right, int numSamples);
    float compressOneBand(float input, BandCompressorState& state,
                          float threshold, float ratio, float attack, float release);
    
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    
    Params mParams;
    
    // Per-channel processing (Gen1 single-band)
    struct ChannelState
    {
        EnvelopeFollower envelopeFollower;
        ARCCalculator arcCalculator;
        WarmCharacter warmCharacter;
        float gainReduction = 0.0f;
        float currentRelease = 160.0f;
        
        void reset()
        {
            envelopeFollower.reset();
            arcCalculator.reset();
            gainReduction = 0.0f;
            currentRelease = 160.0f;
        }
    };
    
    std::vector<ChannelState> mChannels;
    std::vector<SCHPF> mScHPFs;
    SoftKneeCompressor mCompressor;
    VCLimiter mLimiter;
    
    float mGainReductionDB = 0.0f;
    
    const float mHPFFrequencies[5] = {0.0f, 60.0f, 100.0f, 200.0f, 500.0f};
    
    // Gen2: Multiband state
    // 3 crossovers for 4 bands, per channel (stereo = 2 channels)
    // We use separate crossover instances for L and R
    Crossover mCrossover[3][2];   // [crossover_point][channel]
    BandCompressorState mBandState[NUM_BANDS][2];  // [band][channel]
    bool mCrossoverDirty = true;  // Flag to recompute filter coefficients
    
    VC_DECLARE_NON_COPYABLE(VCCompDSP)
};
