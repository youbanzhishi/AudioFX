#pragma once

//==============================================================================
// VC-Reverb DSP Core Header
// Schroeder Algorithmic Reverb
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
// Schroeder Reverb Structures
//==============================================================================

// Comb Filter - parallel processing for room simulation
struct CombFilter {
    std::vector<float> buffer;
    int writePos = 0;
    float feedback = 0.0f;
    float filterState = 0.0f;  // lowpass state in feedback
    float damping = 0.0f;      // damping factor (0-1)
    
    void setBufferSize(int size) {
        buffer.assign(size, 0.0f);
        writePos = 0;
    }
    
    void setFeedback(float fb) { feedback = fb; }
    void setDamping(float damp) { damping = damp; }
    
    float process(float input) {
        float output = buffer[writePos];
        // Lowpass in feedback path for high-frequency damping
        filterState = output * (1.0f - damping) + filterState * damping;
        buffer[writePos] = input + filterState * feedback;
        writePos = (writePos + 1) % static_cast<int>(buffer.size());
        return output;
    }
    
    void clear() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        filterState = 0.0f;
        writePos = 0;
    }
};

// Allpass Filter - series processing for diffusion
struct AllpassFilter {
    std::vector<float> buffer;
    int writePos = 0;
    float feedback = 0.5f;
    
    void setBufferSize(int size) {
        buffer.assign(size, 0.0f);
        writePos = 0;
    }
    
    void setFeedback(float fb) { feedback = fb; }
    
    float process(float input) {
        float buffered = buffer[writePos];
        float output = -input + buffered;
        buffer[writePos] = input + buffered * feedback;
        writePos = (writePos + 1) % static_cast<int>(buffer.size());
        return output;
    }
    
    void clear() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }
};

//==============================================================================
// Main DSP Class
//==============================================================================
class VCPluginDSP
{
public:
    //==============================================================================
    // Plugin-specific parameter structure - VC-Reverb
    //==============================================================================
    struct Params
    {
        float roomSize = 50.0f;     // Room size % (0~100)
        float decay = 50.0f;       // Decay time % (0~100)
        float damping = 50.0f;     // High frequency damping % (0~100)
        float preDelay = 20.0f;    // Pre-delay ms (0~100)
        float mix = 30.0f;         // Dry/Wet mix (0~100)
        float wetLPF = 8000.0f;    // Wet signal high-cut freq (Hz, 1000~16000)
        float wetHPF = 200.0f;     // Wet signal low-cut freq (Hz, 20~500)
        bool enabled = true;      // Bypass flag
    };

    //==============================================================================
    // Construction / Destruction
    VCPluginDSP();
    ~VCPluginDSP();

    //==============================================================================
    // Processing
    void prepare(double sampleRate, int blockSize);
    void process(float* left, float* right, int numSamples);

#ifndef VC_STANDALONE
    // JUCE AudioBlock processing (non-interleaved)
    void process(juce::dsp::AudioBlock<float>& block);
#endif

    void reset();

    //==============================================================================
    // Parameter access
    void setParams(const Params& p);
    Params getParams() const;

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

    //==============================================================================
    // Utility functions
    static float dBToLinear(float dB) {
// Shared constants (available in both JUCE and Standalone modes)

#ifdef VC_STANDALONE
        return VCStandalone::decibelsToGain(dB);
#else
        return juce::Decibels::decibelsToGain(dB);
#endif
    }

    static float linearToDb(float linear) {
// Shared constants (available in both JUCE and Standalone modes)

#ifdef VC_STANDALONE
        return VCStandalone::gainToDecibels(linear);
#else
        return juce::Decibels::gainToDecibels(linear);
#endif
    }

    double getSampleRate() const { return mSampleRate; }
    int getBlockSize() const { return mBlockSize; }

private:
    //==============================================================================
    // Internal DSP implementation
    void processInternal(float* left, float* right, int numSamples);
    void updateParameters();
    int calcCombBufferSize(int baseSize);
    int calcAllpassBufferSize(int baseSize);

    //==============================================================================
    // Comb and Allpass filters
    // mCombs[channel][index], mAllpasses[channel][index]
    CombFilter mCombs[2][4];
    AllpassFilter mAllpasses[2][2];
    
    // Pre-delay buffers
    std::vector<float> mPreDelayBuffer[2];
    int mPreDelayWritePos = 0;
    int mPreDelaySamples = 0;
    
    // Filter parameters
    float mRoomSizeFactor = 0.5f;
    float mDecayFeedback = 0.5f;
    float mDampingFactor = 0.5f;
    
    // Base delay line sizes at 44100Hz
    // Left channel
    static constexpr int BASE_COMB_L0 = 1557;
    static constexpr int BASE_COMB_L1 = 1617;
    static constexpr int BASE_COMB_L2 = 1491;
    static constexpr int BASE_COMB_L3 = 1422;
    // Right channel (slightly offset for stereo spread)
    static constexpr int BASE_COMB_R0 = 1568;
    static constexpr int BASE_COMB_R1 = 1630;
    static constexpr int BASE_COMB_R2 = 1498;
    static constexpr int BASE_COMB_R3 = 1431;
    
    static constexpr int BASE_ALLPASS_L0 = 225;
    static constexpr int BASE_ALLPASS_L1 = 556;
    static constexpr int BASE_ALLPASS_R0 = 231;
    static constexpr int BASE_ALLPASS_R1 = 562;
    
    static constexpr int MAX_PREDELAY_MS = 100;
    static constexpr int MAX_PREDELAY_SAMPLES = 4410;  // 100ms at 44100Hz

    //==============================================================================
    // Member variables
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==============================================================================
    // Non-copyable
    // Post-reverb wet signal filters
    float mWetLPFState[2] = {0.0f, 0.0f};  // lowpass state for high-cut
    float mWetHPFState[2] = {0.0f, 0.0f};  // highpass state for low-cut
    float mWetLPCoeff = 0.0f;   // lowpass coefficient (from wetLPF param)
    float mWetHPCoeff = 0.0f;   // highpass coefficient (from wetHPF param)
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
