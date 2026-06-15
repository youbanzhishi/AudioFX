#pragma once

//==============================================================================
// VC-Hall480 DSP Core Header — Lexicon 480L-Class Nested Allpass Architecture
// Gardner Large Hall variant with LFO modulation + Random Hall extension
// Supports both JUCE and Standalone (no dependency) modes
//==============================================================================

// Shared constants (available in both JUCE and Standalone modes)
constexpr float VC_PI = 3.14159265358979323846f;

#ifdef VC_STANDALONE
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
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
// Algorithm selection
//==============================================================================
enum class ReverbAlgorithm
{
    Hall = 0,
    RandomHall = 1,
    Plate = 2
};

//==============================================================================
// AllpassFilter — Single allpass with circular buffer
//==============================================================================
struct AllpassFilter {
    std::vector<float> buffer;
    int writePos = 0;
    int length = 0;
    float gain = 0.5f;

    void init(int len) {
        length = len;
        buffer.assign(len, 0.0f);
        writePos = 0;
    }

    float process(float input) {
        float delayed = buffer[writePos];
        float output = -gain * input + delayed;
        buffer[writePos] = input + gain * delayed;
        writePos = (writePos + 1) % length;
        return output;
    }

    void clear() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }
};

//==============================================================================
// ModulatedDelay — Delay line with LFO modulation and linear interpolation
//==============================================================================
struct ModulatedDelay {
    std::vector<float> buffer;
    int writePos = 0;
    int baseLength = 0;       // base delay in samples
    int maxLength = 0;        // allocated buffer size (base + modulation headroom)
    float lfoPhase = 0.0f;    // current LFO phase [0, 2pi)
    float lfoRate = 0.0f;     // LFO frequency (Hz)
    float lfoDepth = 0.0f;    // modulation depth in samples

    void init(int baseLen, float rate, float depth, float phaseOffset) {
        baseLength = baseLen;
        lfoRate = rate;
        lfoDepth = depth;
        lfoPhase = phaseOffset;
        // Extra headroom for modulation (up to 1.5x depth safety margin)
        maxLength = baseLen + static_cast<int>(depth * 2.0f) + 16;
        buffer.assign(maxLength, 0.0f);
        writePos = 0;
    }

    float process(float input, double sampleRate) {
        // Write input
        buffer[writePos] = input;

        // Compute modulated read position
        float mod = std::sin(lfoPhase) * lfoDepth;
        lfoPhase += 2.0f * VC_PI * lfoRate / static_cast<float>(sampleRate);
        if (lfoPhase > 2.0f * VC_PI) lfoPhase -= 2.0f * VC_PI;

        float readPosF = static_cast<float>(writePos) - static_cast<float>(baseLength) + mod;
        // Wrap into buffer range
        while (readPosF < 0.0f) readPosF += static_cast<float>(maxLength);
        while (readPosF >= static_cast<float>(maxLength)) readPosF -= static_cast<float>(maxLength);

        // Linear interpolation
        int idx0 = static_cast<int>(readPosF);
        float frac = readPosF - static_cast<float>(idx0);
        int idx1 = (idx0 + 1) % maxLength;

        float output = buffer[idx0] * (1.0f - frac) + buffer[idx1] * frac;

        writePos = (writePos + 1) % maxLength;
        return output;
    }

    // Read without modulation (for simple delay mode)
    float readDelay() const {
        int readPos = writePos - baseLength;
        if (readPos < 0) readPos += maxLength;
        return buffer[readPos];
    }

    void clear() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }
};

//==============================================================================
// DiffusedCluster — Single early reflection cluster (diffused echo group)
//==============================================================================
struct DiffusedCluster {
    static constexpr int MAX_TAPS = 8;
    int numTaps = 0;
    int tapDelays[MAX_TAPS] = {};
    float tapGains[MAX_TAPS] = {};
    std::vector<float> buffer;
    int writePos = 0;
    int maxDelay = 0;

    void configure(int baseDelayMs, float diffusionFactor, double sampleRate) {
        // Base delay converted to samples
        int baseSamples = static_cast<int>(baseDelayMs * sampleRate / 1000.0f + 0.5f);
        numTaps = 0;

        // Create a cluster of taps around the base delay
        // More diffusion = more taps, more spread
        int numDesired = static_cast<int>(2 + diffusionFactor * 6.0f); // 2-8 taps
        numDesired = VC_JCLAMP(numDesired, 2, MAX_TAPS);

        float spreadMs = 1.0f + diffusionFactor * 15.0f; // 1-16ms spread
        int spreadSamples = static_cast<int>(spreadMs * sampleRate / 1000.0f + 0.5f);

        for (int i = 0; i < numDesired; ++i) {
            // Distribute taps evenly within spread range around base
            float t = static_cast<float>(i) / static_cast<float>(numDesired - 1);
            int offset = static_cast<int>((t - 0.5f) * 2.0f * spreadSamples);
            tapDelays[i] = baseSamples + offset;
            if (tapDelays[i] < 1) tapDelays[i] = 1;
            // Amplitude decreases with distance from center
            float centerDist = std::abs(t - 0.5f) * 2.0f;
            tapGains[i] = 1.0f - centerDist * 0.4f;
            numTaps++;
        }

        maxDelay = 0;
        for (int i = 0; i < numTaps; ++i) {
            if (tapDelays[i] > maxDelay) maxDelay = tapDelays[i];
        }
        maxDelay = VC_JMAX(maxDelay + 1, 64);
        buffer.assign(maxDelay, 0.0f);
        writePos = 0;
    }

    float process(float input) {
        buffer[writePos] = input;
        float output = 0.0f;
        for (int i = 0; i < numTaps; ++i) {
            int readPos = (writePos - tapDelays[i] + maxDelay) % maxDelay;
            output += buffer[readPos] * tapGains[i];
        }
        writePos = (writePos + 1) % maxDelay;
        return output;
    }

    void clear() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }
};

//==============================================================================
// EarlyReflections480 — 480L-style early reflections with diffused clusters
//==============================================================================
struct EarlyReflections480 {
    static constexpr int NUM_VOICES = 6;
    DiffusedCluster voices[NUM_VOICES];
    float voiceGains[NUM_VOICES] = {};
    float voicePans[NUM_VOICES] = {};   // -1 = left, 0 = center, 1 = right
    int numActiveVoices = 0;

    // 480L default pre-echo timing (ms) for Hall algorithm
    static constexpr float HALL_DELAYS[NUM_VOICES] = { 12.5f, 21.3f, 28.7f, 35.1f, 42.8f, 51.3f };
    static constexpr float HALL_GAINS[NUM_VOICES] = { 0.75f, 0.60f, 0.50f, 0.42f, 0.35f, 0.28f };
    static constexpr float HALL_PANS[NUM_VOICES] = { -0.6f, 0.4f, -0.3f, 0.7f, -0.5f, 0.2f };

    // Plate pre-echo timing
    static constexpr float PLATE_DELAYS[NUM_VOICES] = { 5.2f, 9.8f, 14.3f, 18.7f, 22.1f, 26.5f };
    static constexpr float PLATE_GAINS[NUM_VOICES] = { 0.65f, 0.55f, 0.45f, 0.38f, 0.30f, 0.22f };
    static constexpr float PLATE_PANS[NUM_VOICES] = { -0.8f, 0.8f, -0.5f, 0.5f, -0.3f, 0.3f };

    void configure(ReverbAlgorithm algo, float roomSizeFactor, float diffusionFactor, double sampleRate) {
        const float* delays = (algo == ReverbAlgorithm::Plate) ? PLATE_DELAYS : HALL_DELAYS;
        const float* gains = (algo == ReverbAlgorithm::Plate) ? PLATE_GAINS : HALL_GAINS;
        const float* pans = (algo == ReverbAlgorithm::Plate) ? PLATE_PANS : HALL_PANS;

        numActiveVoices = NUM_VOICES;

        for (int i = 0; i < NUM_VOICES; ++i) {
            // Scale delay times by room size (0.5x to 2.0x)
            float scaledDelay = delays[i] * (0.5f + roomSizeFactor * 1.5f);
            voices[i].configure(static_cast<int>(scaledDelay + 0.5f), diffusionFactor / 100.0f, sampleRate);
            voiceGains[i] = gains[i];
            voicePans[i] = pans[i];
        }
    }

    // Process mono input, produce stereo outputs
    void process(float input, float& outL, float& outR) {
        outL = 0.0f;
        outR = 0.0f;
        for (int i = 0; i < numActiveVoices; ++i) {
            float voice = voices[i].process(input) * voiceGains[i];
            float pan = voicePans[i];
            // Equal-power panning
            float angle = (pan + 1.0f) * 0.25f * VC_PI;
            outL += voice * std::cos(angle);
            outR += voice * std::sin(angle);
        }
    }

    void clear() {
        for (int i = 0; i < NUM_VOICES; ++i) {
            voices[i].clear();
        }
    }
};

//==============================================================================
// RandomNoise — Smoothed low-frequency noise for Random Hall
//==============================================================================
struct RandomNoise {
    float state = 0.0f;
    float target = 0.0f;
    float smoothCoeff = 0.0f;
    int stepCounter = 0;
    int stepInterval = 64; // update target every N samples

    void init(double sampleRate, float rate) {
        state = 0.0f;
        target = 0.0f;
        // Smooth coefficient: rate Hz cutoff
        smoothCoeff = 1.0f - std::exp(-2.0f * VC_PI * rate / static_cast<float>(sampleRate));
        stepInterval = static_cast<int>(sampleRate / rate);
        if (stepInterval < 4) stepInterval = 4;
        stepCounter = 0;
    }

    float next() {
        stepCounter++;
        if (stepCounter >= stepInterval) {
            stepCounter = 0;
            // New random target [-1, 1]
#ifdef VC_STANDALONE
            target = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f;
#else
            target = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
#endif
        }
        state += smoothCoeff * (target - state);
        return state;
    }

    void clear() {
        state = 0.0f;
        target = 0.0f;
        stepCounter = 0;
    }
};

//==============================================================================
// LateReverbChannel — Single channel late reverb engine
// Gardner Large Hall: AP → D → AP → D → AP → D → global feedback
//==============================================================================
struct LateReverbChannel {
    // Allpass filters in the chain
    AllpassFilter allpass[3];

    // Modulated delays between allpasses
    ModulatedDelay delay[3];

    // Feedback path damping (1-pole LP)
    float feedbackLPState = 0.0f;
    float feedbackLPCoeff = 0.0f;

    // Feedback gain (derived from T60)
    float feedbackGain = 0.5f;

    // Random noise generator (for Random Hall)
    RandomNoise randomMod;
    float randomDepth = 0.0f;

    // Base delay lengths at 44100Hz (Gardner-style, incommensurate primes)
    // Left channel: slightly different from right for stereo
    static constexpr int BASE_AP_DELAYS[3] = { 309, 791, 271 };   // ~7ms, ~18ms, ~6ms
    static constexpr int BASE_D_DELAYS[3]  = { 1031, 1987, 2953 }; // ~23ms, ~45ms, ~67ms

    // Alternative for right channel (offset primes)
    static constexpr int BASE_AP_DELAYS_R[3] = { 317, 809, 283 };
    static constexpr int BASE_D_DELAYS_R[3]  = { 1061, 2011, 2909 };

    // LFO rates (Hz) per delay — incommensurate
    static constexpr float LFO_RATES[3] = { 0.3f, 0.37f, 0.43f };

    void init(bool isRight, double sampleRate, float roomSizeFactor, float chorusRate, float chorusDepth, float randomDepthParam) {
        float srRatio = static_cast<float>(sampleRate) / 44100.0f;
        float sizeScale = 0.5f + roomSizeFactor * 1.5f;

        const int* apDelays = isRight ? BASE_AP_DELAYS_R : BASE_AP_DELAYS;
        const int* dDelays = isRight ? BASE_D_DELAYS_R : BASE_D_DELAYS;

        // Allpass gains: 0.5 is a good default, can be tuned
        float apGain = 0.5f;

        for (int i = 0; i < 3; ++i) {
            int apLen = static_cast<int>(apDelays[i] * srRatio * sizeScale + 0.5f);
            apLen = VC_JMAX(apLen, 4); // minimum 4 samples
            allpass[i].init(apLen);
            allpass[i].gain = apGain;

            int dLen = static_cast<int>(dDelays[i] * srRatio * sizeScale + 0.5f);
            dLen = VC_JMAX(dLen, 8);

            // LFO modulation depth in samples: 0.5 to 8 samples
            float depth = chorusDepth * 8.0f; // chorusDepth is 0-100
            float phaseOffset = isRight ? VC_PI * 0.7f : 0.0f; // offset for right channel
            delay[i].init(dLen, LFO_RATES[i] * chorusRate, depth, phaseOffset + static_cast<float>(i) * 2.1f);
        }

        feedbackLPState = 0.0f;
        randomDepth = randomDepthParam;
        randomMod.init(sampleRate, 2.0f); // 2Hz random modulation
    }

    float process(float input, double sampleRate) {
        // Apply random modulation if active
        float randomOffset = randomMod.next() * randomDepth;

        // Chain: input → AP0 → D0 → AP1 → D1 → AP2 → D2 → output
        float x = allpass[0].process(input);
        x = delay[0].process(x, sampleRate);

        x = allpass[1].process(x);
        x = delay[1].process(x, sampleRate);

        x = allpass[2].process(x);
        float output = delay[2].process(x, sampleRate);

        // Apply random offset to output for Random Hall
        output += randomOffset * 0.01f; // subtle random perturbation

        return output;
    }

    // Process feedback path: apply LP damping and gain
    float processFeedback(float feedback) {
        feedbackLPState = feedbackLPCoeff * feedback + (1.0f - feedbackLPCoeff) * feedbackLPState;
        return feedbackLPState * feedbackGain;
    }

    void clear() {
        for (int i = 0; i < 3; ++i) {
            allpass[i].clear();
            delay[i].clear();
        }
        feedbackLPState = 0.0f;
        randomMod.clear();
    }
};

//==============================================================================
// Main DSP Class — VC-Hall480
//==============================================================================
class VCPluginDSP
{
public:
    //==========================================================================
    // Plugin-specific parameter structure
    //==========================================================================
    struct Params
    {
        int algorithm = 0;         // 0=Hall, 1=Random Hall, 2=Plate
        float roomSize = 50.0f;    // 0~100
        float decayTime = 2.0f;    // 0.3~20s (T60)
        float preDelay = 20.0f;    // 0~200ms
        float diffusion = 70.0f;   // 0~100
        float shape = 50.0f;       // 0~100
        float spread = 80.0f;      // 0~100
        float hiDecay = 0.5f;      // 0.1~2.0
        float loDecay = 1.0f;      // 0.1~2.0
        float chorusRate = 1.0f;   // 0.0~5.0 Hz
        float chorusDepth = 30.0f; // 0~100
        float mix = 30.0f;         // 0~100
        float wetLPF = 8000.0f;    // 1000~16000 Hz
        float wetHPF = 200.0f;     // 20~500 Hz
        bool enabled = true;
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
    void updateParameters();
    float calcDecayGain(float delaySamples);

    //==========================================================================
    // Late reverb engines (stereo: left + right)
    LateReverbChannel mLateL;  // left channel reverb
    LateReverbChannel mLateR;  // right channel reverb

    // Cross-coupling gain between L/R feedback paths
    float mCrossGain = 0.3f;

    // Early reflections
    EarlyReflections480 mEarlyRef;

    // Pre-delay buffers
    std::vector<float> mPreDelayBuffer[2];
    int mPreDelayWritePos = 0;
    int mPreDelaySamples = 0;
    static constexpr int MAX_PREDELAY_MS = 200;
    static constexpr int MAX_PREDELAY_SAMPLES = 19200; // 200ms at 96000Hz

    // Shape envelope state
    float mShapeAttack = 0.0f;    // attack coefficient
    float mShapeDecay = 0.0f;     // decay coefficient
    float mEnvelopeState[2] = {}; // envelope follower state per channel

    // FDN parameter state
    float mRoomSizeFactor = 0.5f;
    float mT60 = 2.0f;
    float mFeedbackLPCutoff = 5000.0f;
    float mTotalDelaySamples = 0.0f; // sum of all delay lengths for T60 calc

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
    // Post-reverb wet signal filters
    float mWetLPFState[2] = {0.0f, 0.0f};
    float mWetHPFState[2] = {0.0f, 0.0f};
    float mWetLPCoeff = 0.0f;
    float mWetHPCoeff = 0.0f;

    // Lo-decay highpass in feedback (removes low-frequency buildup)
    float mLoDecayHPState[2] = {0.0f, 0.0f};
    float mLoDecayHPCoeff = 0.0f;

    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
