#pragma once

//==============================================================================
// VC-Reverb DSP Core Header — Gen 2 FDN Architecture
// Feedback Delay Network with Householder Matrix + Early Reflections
// Upgraded from Gen 1 Schroeder (4 Comb + 2 Allpass) to 8-delay-line FDN
// Supports both JUCE and Standalone (no dependency) modes
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
// FDN Constants
//==============================================================================
constexpr int FDN_NUM_DELAYS = 8;

// Prime delay line lengths (at 44100Hz reference) — avoids common periods
constexpr int FDN_BASE_DELAYS[FDN_NUM_DELAYS] = { 997, 1061, 1151, 1223, 1301, 1399, 1499, 1559 };

//==============================================================================
// DelayLine — Circular buffer for FDN
//==============================================================================
struct FDNDelayLine {
    std::vector<float> buffer;
    int writePos = 0;
    int length = 0;             // active length (may be scaled)
    float feedbackGain = 0.0f;  // per-line T60-based decay gain
    float lpState = 0.0f;       // 1-pole lowpass state in feedback path
    float lpCoeff = 0.0f;       // 1-pole lowpass coefficient (from damping)

    void init(int len) {
        length = len;
        buffer.assign(len, 0.0f);
        writePos = 0;
        lpState = 0.0f;
    }

    // Read from delay line at current writePos (oldest sample = writePos)
    float read() const {
        return buffer[writePos];
    }

    // Write with feedback path: input + LP(filtered feedback) * gain
    void write(float input, float feedbackSample) {
        // 1-pole LP in feedback path for frequency-dependent decay
        lpState = lpCoeff * feedbackSample + (1.0f - lpCoeff) * lpState;
        buffer[writePos] = input + lpState * feedbackGain;
        writePos = (writePos + 1) % length;
    }

    void clear() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        lpState = 0.0f;
    }
};

//==============================================================================
// EarlyReflections — Simplified image method for first reflections
//==============================================================================
struct EarlyReflections {
    static constexpr int MAX_REFLECTIONS = 16;
    int numReflections = 0;
    int delaySamples[MAX_REFLECTIONS] = {};    // delay in samples
    float gains[MAX_REFLECTIONS] = {};          // gain per reflection
    std::vector<float> delayBuffer;             // single circular buffer
    int delayBufWritePos = 0;
    int maxDelaySamples = 0;

    // Generate early reflections from room dimensions
    // roomSizeM: room dimension in meters (2..30)
    // sr: sample rate
    void configure(float roomSizeM, double sr) {
        // Simplified image method: 1st + 2nd order reflections
        // Source at (0.25*W, 0, 0.25*D), Listener at (0.75*W, 0, 0.75*D)
        float W = roomSizeM;
        float D = roomSizeM * 0.8f;  // depth slightly less than width
        float H = roomSizeM * 0.4f;  // height
        float c = 343.0f;            // speed of sound m/s

        // Source and listener positions
        float sx = 0.25f * W, sz = 0.25f * D;
        float lx = 0.75f * W, lz = 0.75f * D;

        numReflections = 0;

        // 1st order reflections (6 surfaces: +/-x, +/-y, +/-z)
        // Image sources for 1st order
        float img1[][3] = {
            {-sx, 0, sz},       // -x wall
            {2*W - sx, 0, sz},  // +x wall
            {sx, 2*H, sz},      // +y (ceiling)
            {sx, -0, sz},       // -y (floor) — direct floor reflection
            {sx, 0, -sz},       // -z wall
            {sx, 0, 2*D - sz},  // +z wall
        };

        for (auto& img : img1) {
            if (numReflections >= MAX_REFLECTIONS) break;
            float dx = img[0] - lx;
            float dy = img[1];
            float dz = img[2] - lz;
            float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (dist < 0.1f) continue;

            delaySamples[numReflections] = static_cast<int>(dist / c * sr + 0.5f);
            gains[numReflections] = 1.0f / (1.0f + dist * 0.1f);  // distance attenuation + air absorption
            numReflections++;
        }

        // 2nd order reflections (selected corner/edge reflections)
        float img2[][3] = {
            {-sx, 0, -sz},               // -x, -z corner
            {2*W - sx, 0, -sz},           // +x, -z corner
            {-sx, 0, 2*D - sz},           // -x, +z corner
            {2*W - sx, 0, 2*D - sz},      // +x, +z corner
            {-sx, 2*H, sz},               // -x, ceiling
            {2*W - sx, 2*H, sz},          // +x, ceiling
        };

        for (auto& img : img2) {
            if (numReflections >= MAX_REFLECTIONS) break;
            float dx = img[0] - lx;
            float dy = img[1];
            float dz = img[2] - lz;
            float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (dist < 0.1f) continue;

            delaySamples[numReflections] = static_cast<int>(dist / c * sr + 0.5f);
            gains[numReflections] = 0.7f / (1.0f + dist * 0.12f);  // extra attenuation for 2nd order
            numReflections++;
        }

        // Find max delay for buffer sizing
        maxDelaySamples = 0;
        for (int i = 0; i < numReflections; ++i) {
            if (delaySamples[i] > maxDelaySamples)
                maxDelaySamples = delaySamples[i];
        }
        maxDelaySamples = std::max(maxDelaySamples + 1, 64);

        delayBuffer.assign(maxDelaySamples, 0.0f);
        delayBufWritePos = 0;
    }

    // Process a single sample through early reflections
    float process(float input) {
        // Write input to delay buffer
        delayBuffer[delayBufWritePos] = input;

        // Sum reflections
        float output = 0.0f;
        for (int i = 0; i < numReflections; ++i) {
            int readPos = (delayBufWritePos - delaySamples[i] + maxDelaySamples) % maxDelaySamples;
            output += delayBuffer[readPos] * gains[i];
        }

        delayBufWritePos = (delayBufWritePos + 1) % maxDelaySamples;
        return output;
    }

    void clear() {
        std::fill(delayBuffer.begin(), delayBuffer.end(), 0.0f);
        delayBufWritePos = 0;
    }
};

//==============================================================================
// Main DSP Class
//==============================================================================
class VCPluginDSP
{
public:
    //==========================================================================
    // Plugin-specific parameter structure - VC-Reverb (Gen 2 FDN)
    //==========================================================================
    struct Params
    {
        float roomSize = 50.0f;     // Room size % (0~100)
        float decay = 50.0f;       // Decay time % (0~100)
        float damping = 50.0f;     // High frequency damping % (0~100)
        float preDelay = 20.0f;    // Pre-delay ms (0~200)
        float mix = 30.0f;         // Dry/Wet mix (0~100)
        float wetLPF = 8000.0f;    // Wet signal high-cut freq (Hz, 1000~16000)
        float wetHPF = 200.0f;     // Wet signal low-cut freq (Hz, 20~500)
        bool enabled = true;      // Bypass flag
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
    void updateParameters();
    int calcFDNDelaySize(int baseDelay);

    //==========================================================================
    // FDN delay lines: [channel][delay_index]
    FDNDelayLine mFDN[2][FDN_NUM_DELAYS];

    // Householder matrix output temp buffers
    float mHouseholderTmp[FDN_NUM_DELAYS];

    // Early reflections (per channel)
    EarlyReflections mEarlyRef[2];

    // Pre-delay buffers
    std::vector<float> mPreDelayBuffer[2];
    int mPreDelayWritePos = 0;
    int mPreDelaySamples = 0;
    static constexpr int MAX_PREDELAY_MS = 200;
    static constexpr int MAX_PREDELAY_SAMPLES = 8820;  // 200ms at 44100Hz

    // FDN parameter state
    float mRoomSizeFactor = 0.5f;
    float mT60 = 2.0f;           // T60 decay time in seconds
    float mDampingLPF = 0.5f;    // feedback LP coefficient
    float mRoomSizeMeters = 10.0f;

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
    float mWetLPFState[2] = {0.0f, 0.0f};  // lowpass state for high-cut
    float mWetHPFState[2] = {0.0f, 0.0f};  // highpass state for low-cut
    float mWetLPCoeff = 0.0f;   // lowpass coefficient (from wetLPF param)
    float mWetHPCoeff = 0.0f;   // highpass coefficient (from wetHPF param)
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
