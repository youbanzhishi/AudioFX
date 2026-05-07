#pragma once

//==============================================================================
// VC-Tune DSP Core Header
// Pitch Correction / Auto-Tune Plugin
// Supports both JUCE and Standalone (no dependency) modes
//==============================================================================

constexpr float VC_PI = 3.14159265358979323846f;

#ifdef VC_STANDALONE
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
#include <juce_dsp/juce_dsp.h>

#define VC_DECLARE_NON_COPYABLE(x) JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(x)
#define VC_JMIN(a, b) juce::jmin(a, b)
#define VC_JMAX(a, b) juce::jmax(a, b)
#define VC_JCLAMP(a, b, c) juce::jlimit(a, b, c)
#endif

//==============================================================================
// YIN Pitch Detector
//==============================================================================
class YINPitchDetector
{
public:
    struct Result {
        float frequency = 0.0f;
        float confidence = 0.0f;
        bool voiced = false;
    };

    YINPitchDetector(double sampleRate = 44100.0, int bufferSize = 2048);
    Result detect(const float* samples, int numSamples);
    void reset();
    void setThreshold(float t) { mThreshold = t; }
    void setSampleRate(double sr) { mSampleRate = sr; }

private:
    double mSampleRate;
    int mBufferSize;
    float mThreshold = 0.15f;
    std::vector<float> mD;
    std::vector<float> mDPrime;

    void differenceFunction(const float* samples, int numSamples);
    void cumulativeMeanNormalized();
    int absoluteThreshold();
    float parabolicInterpolation(int tau);
};

//==============================================================================
// Scale Quantizer
//==============================================================================
class ScaleQuantizer
{
public:
    enum Scale {
        Chromatic = 0, Major = 1, Minor = 2, Pentatonic = 3, Blues = 4, Custom = 5
    };

    ScaleQuantizer();

    float quantize(float frequency, Scale scale, int keyOffset = 0,
                   float transpose = 0.0f, float cents = 0.0f);

    int quantizeToNote(float frequency, Scale scale, int keyOffset = 0,
                       float transpose = 0.0f);

    static int frequencyToNote(float frequency);
    static float noteToFrequency(int note);
    static float noteToFrequencyFloat(float note);
    static const char* noteName(int note);
    static const char* noteNameWithOctave(int note);
    static const char* scaleName(Scale s);

    void setCustomScale(const bool scale[12]);
    bool isNoteInScale(int note, Scale scale, int keyOffset = 0) const;

private:
    bool mCustomScale[12] = {};
    static const bool mScales[6][12];
};

//==============================================================================
// Pitch Corrector (Phase 1: Resampling + IIR Smoothing)
//==============================================================================
class PitchCorrector
{
public:
    PitchCorrector(double sampleRate = 44100.0);

    void process(float* left, float* right, int numSamples,
                 YINPitchDetector& detector, ScaleQuantizer& quantizer,
                 float speed, ScaleQuantizer::Scale scale, int keyOffset,
                 float transpose, float cents, float formantPreserve);

    void reset();

    struct FrameInfo {
        float detectedF0 = 0.0f;
        float targetF0 = 0.0f;
        float ratio = 1.0f;
        float confidence = 0.0f;
        bool voiced = false;
        int detectedNote = 0;
        int targetNote = 0;
    };

    const std::vector<FrameInfo>& getFrameInfo() const { return mFrameInfo; }
    void setReportMode(bool enable) { mReportMode = enable; }
    void setSampleRate(double sr) { mSampleRate = sr; rebuildWindow(); }

private:
    double mSampleRate;
    int mFrameSize = 2048;
    int mHopSize = 1024;
    float mCurrentRatio = 1.0f;
    bool mReportMode = false;

    std::vector<float> mWindow;
    std::vector<FrameInfo> mFrameInfo;

    void rebuildWindow();
};

//==============================================================================
// Key Detector (Krumhansl-Schmuckler)
//==============================================================================
class KeyDetector
{
public:
    struct Result {
        int key = 0;
        bool isMajor = true;
        float confidence = 0.0f;
        bool detected = false;
    };

    KeyDetector(double sampleRate = 44100.0);

    Result detect(const float* left, const float* right, int numSamples,
                  YINPitchDetector& yin);

    void reset();

    static const char* keyName(int key, bool isMajor);

private:
    double mSampleRate;
    int mAnalysisFrameSize = 2048;
    int mAnalysisHopSize = 1024;
    float mChroma[12] = {};

    static const float KSMajorProfile[12];
    static const float KSMinorProfile[12];

    float pearsonCorrelation(const float* x, const float* y, int n);
    void rotateProfile(const float* in, float* out, int shift);
};

//==============================================================================
// Main DSP Class: VCTuneDSP
//==============================================================================
class VCTuneDSP
{
public:
    struct Params
    {
        float speed = 50.0f;
        int scale = 0;
        float transpose = 0.0f;
        float cents = 0.0f;
        float formantPreserve = 1.0f;
        bool bypass = false;
        bool autoKey = false;
    };

    struct ABState {
        Params paramsA;
        Params paramsB;
        bool activeIsA = true;
    };

    struct PitchReport {
        float timeMs = 0.0f;
        float detectedF0 = 0.0f;
        float targetF0 = 0.0f;
        float confidence = 0.0f;
        bool voiced = false;
        int detectedNote = 0;
        int targetNote = 0;
        float deviationCents = 0.0f;
    };

    VCTuneDSP();
    ~VCTuneDSP();

    void prepare(double sampleRate, int blockSize);
    void process(float* left, float* right, int numSamples);

#ifndef VC_STANDALONE
    void process(juce::dsp::AudioBlock<float>& block);
#endif

    void reset();

    void setParams(const Params& p);
    Params getParams() const;

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

    // A/B comparison
    void setABParams(const Params& a, const Params& b);
    void switchAB();
    Params getActiveParams() const;
    bool isActiveA() const { return mAB.activeIsA; }

    // Report mode
    void setReportMode(bool enable);
    const std::vector<PitchReport>& getReport() const { return mReport; }

    // Key detection result
    KeyDetector::Result getKeyResult() const { return mKeyResult; }
    bool hasKeyDetected() const { return mKeyDetected; }

    // Utility
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
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    YINPitchDetector mPitchDetector;
    ScaleQuantizer mQuantizer;
    PitchCorrector mCorrector;
    KeyDetector mKeyDetector;

    bool mKeyDetected = false;
    KeyDetector::Result mKeyResult;
    int mAutoKeyOffset = 0;
    ScaleQuantizer::Scale mAutoScale = ScaleQuantizer::Chromatic;

    ABState mAB;

    bool mReportMode = false;
    std::vector<PitchReport> mReport;

    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    VC_DECLARE_NON_COPYABLE(VCTuneDSP)
};
