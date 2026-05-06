#pragma once

#ifdef VC_STANDALONE
// Standalone mode: use standard library math, no JUCE dependency
#include <vector>
#include <cmath>
#include <algorithm>

// Standalone constants
constexpr float VC_PI = 3.14159265358979323846f;

namespace VCStandalone {
    inline float decibelsToGain(float dB) {
        return std::pow(10.0f, dB / 20.0f);
    }
}
#define VC_DECLARE_NON_COPYABLE(x) // No-op in standalone
#else
// JUCE mode
#include <juce_dsp/juce_dsp.h>
#define VC_DECLARE_NON_COPYABLE(x) JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(x)
#endif

class VCEQDSP
{
public:
    enum class FilterType
    {
        LowShelf = 0,
        HighShelf = 1,
        Parametric = 2,
        LowPass = 3,
        HighPass = 4
    };
    
    struct BandParams
    {
        bool enabled = true;
        FilterType type = FilterType::Parametric;
        float frequency = 1000.0f;
        float q = 1.0f;
        float gainDB = 0.0f;
    };
    
    static constexpr int kNumBands = 5;
    static constexpr float kDefaultFrequencies[kNumBands] = { 80.0f, 300.0f, 1000.0f, 3000.0f, 8000.0f };
    static constexpr float kDefaultQ[kNumBands] = { 0.707f, 1.0f, 1.0f, 1.0f, 0.707f };
    static constexpr float kDefaultGains[kNumBands] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    
    VCEQDSP();
    ~VCEQDSP();
    
    void prepare(double sampleRate, int blockSize);
    void process(float* left, float* right, int numSamples);
#ifndef VC_STANDALONE
    void process(juce::dsp::AudioBlock<float>& block);
#endif
    void reset();
    void setBand(int index, const BandParams& params);
    BandParams getBand(int index) const;
    void setAllBands(const BandParams params[kNumBands]);
    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }
    double getSampleRate() const { return mSampleRate; }
    
private:
    void updateFilterCoefficients(int band);
    
    struct BandFilter
    {
#ifndef VC_STANDALONE
        juce::dsp::IIR::Filter<float> filter;
#endif
        bool enabled = true;
        FilterType type = FilterType::Parametric;
        float frequency = 1000.0f;
        float q = 1.0f;
        float gainDB = 0.0f;
    };
    
    struct CachedCoefficients
    {
        float freq = 0.0f;
        float q = 0.0f;
        float gainDB = 0.0f;
        FilterType type = FilterType::Parametric;
        bool enabled = true;
    };
    
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    
    BandFilter mBands[kNumBands];
    CachedCoefficients mCachedParams[kNumBands];
    
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;
    
    // Standalone IIR state (replaces juce::dsp::IIR::Filter)
#ifdef VC_STANDALONE
    struct IIRState {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
    };
    IIRState mIIRStates[kNumBands][2]; // [band][channel]
    void updateIIRCoefficients(int band, double sampleRate);
    void processIIR(float* left, float* right, int numSamples);
#endif
    
    VC_DECLARE_NON_COPYABLE(VCEQDSP)
};
