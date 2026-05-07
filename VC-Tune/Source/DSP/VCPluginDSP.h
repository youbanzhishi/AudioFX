#pragma once

//==============================================================================
// VC-Tune DSP Core Header - Gen2
// Pitch Correction / Auto-Tune Plugin
// Gen2: Full PSOLA + LPC Formant Preservation
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

    // Gen2: expose period for PSOLA marking
    float getLastRefinedTau() const { return mLastRefinedTau; }

private:
    double mSampleRate;
    int mBufferSize;
    float mThreshold = 0.15f;
    float mLastRefinedTau = 0.0f;
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
// Gen2: LPC Formant Extractor (12th-order Linear Predictive Coding)
//==============================================================================
class LPCFormantExtractor
{
public:
    static constexpr int LPC_ORDER = 12;

    LPCFormantExtractor();

    // Analyze one frame and return LPC coefficients (a[0..LPC_ORDER])
    // a[0] is always 1.0 (convention: A(z) = 1 + a[1]z^-1 + ... + a[P]z^-P)
    struct LPCResult {
        float a[LPC_ORDER + 1];  // LPC filter coefficients (a[0]=1.0)
        float gain;              // Prediction error gain (sigma)
    };

    LPCResult analyze(const float* frame, int frameSize);

    // Apply inverse filter A(z) to extract excitation signal
    // excitation[n] = x[n] + a[1]*x[n-1] + ... + a[P]*x[n-P]
    void inverseFilter(const float* input, int numSamples,
                       const float* a, float* excitation);

    // Apply synthesis filter 1/A(z) to restore formants
    // y[n] = excitation[n] - a[1]*y[n-1] - ... - a[P]*y[n-P]
    void synthesisFilter(const float* excitation, int numSamples,
                         const float* a, float gain, float* output);

    // Frequency-scale LPC coefficients for formant shifting
    // shiftSemitones > 0 shifts formants up, < 0 shifts down
    void shiftFormants(const float* aIn, float shiftSemitones, float sampleRate,
                       float* aOut);

    void reset();

private:
    float mAutocorr[LPC_ORDER + 1];
    float mReflection[LPC_ORDER + 1];

    void autocorrelation(const float* frame, int frameSize);
};

//==============================================================================
// Gen2: PSOLA Engine (Pitch Synchronous Overlap and Add)
//==============================================================================
class PSOLAEngine
{
public:
    PSOLAEngine(double sampleRate = 44100.0);

    // Main processing: pitch-shift with optional time-stretch
    struct ProcessParams {
        const float* input;          // Input signal
        int numSamples;              // Number of input samples
        const float* pitchRatios;    // Per-frame pitch ratios
        int numFrames;               // Number of analysis frames
        int analysisHop;             // Hop size between frames
        int frameSize;               // Analysis frame size
        float speed;                 // Correction speed 0-100
        float formantPreserve;       // 0-100: formant preservation amount
        float formantShift;          // -12 to +12 semitones formant shift
        float vibratoPreserve;       // 0-100: vibrato preservation amount
        float transitionSmooth;      // 0-100: transition smoothing
        bool useLPC;                 // Whether to use LPC formant processing
        const LPCFormantExtractor::LPCResult* lpcResults; // Per-frame LPC (or nullptr)
    };

    void process(const ProcessParams& params, float* output);

    // Detect pitch marks from YIN analysis
    std::vector<int> detectPitchMarks(const float* input, int numSamples,
                                       YINPitchDetector& detector,
                                       int frameSize, int hopSize);

    void setSampleRate(double sr) { mSampleRate = sr; }
    void reset();

private:
    double mSampleRate;
    LPCFormantExtractor mLPC;  // LPC instance for formant processing within PSOLA

    // Hanning window for PSOLA analysis frames
    std::vector<float> mHannWindow;
    int mLastWindowSize = 0;

    void buildHannWindow(int size);
};

//==============================================================================
// Pitch Corrector (Gen2: Full PSOLA + Formant Preservation)
//==============================================================================
class PitchCorrector
{
public:
    PitchCorrector(double sampleRate = 44100.0);

    // Gen2: extended process with new formant/vibrato/smoothing params
    void process(float* left, float* right, int numSamples,
                 YINPitchDetector& detector, ScaleQuantizer& quantizer,
                 float speed, ScaleQuantizer::Scale scale, int keyOffset,
                 float transpose, float cents, float formantPreserve,
                 float formantShift, float vibratoPreserve, float transitionSmooth);

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

    // Gen2: PSOLA engine and LPC extractor
    PSOLAEngine mPSOLA;
    LPCFormantExtractor mLPC;

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
// Main DSP Class: VCTuneDSP (Gen2)
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
        float formantPreserve = 100.0f;     // Gen2: 0-100 (was 0-1)
        float formantShift = 0.0f;          // Gen2: -12 to +12 semitones
        float vibratoPreserve = 0.0f;       // Gen2: 0-100
        float transitionSmooth = 50.0f;     // Gen2: 0-100
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
