#pragma once

#include <juce_dsp/juce_dsp.h>

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
    void process(juce::dsp::AudioBlock<float>& block);
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
        juce::dsp::IIR::Filter<float> filter;
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
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCEQDSP)
};
