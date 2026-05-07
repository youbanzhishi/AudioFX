#pragma once

//==============================================================================
// VC-Harmonizer DSP Core Header
// Intelligent Harmony Generator for Vocal Chain
// Architecture: YIN Detect → Scale Quantize → Interval Shift → LPC Formant → Mix
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
// YIN Pitch Detector (reused from VC-Tune)
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
// Scale Quantizer (reused from VC-Tune)
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
// Key Detector (Krumhansl-Schmuckler) (reused from VC-Tune)
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
// LPC Formant Extractor (reused from VC-Tune Gen2)
//==============================================================================
class LPCFormantExtractor
{
public:
    static constexpr int LPC_ORDER = 12;

    LPCFormantExtractor();

    struct LPCResult {
        float a[LPC_ORDER + 1];  // LPC filter coefficients (a[0]=1.0)
        float gain;              // Prediction error gain (sigma)
    };

    LPCResult analyze(const float* frame, int frameSize);
    void inverseFilter(const float* input, int numSamples,
                       const float* a, float* excitation);
    void synthesisFilter(const float* excitation, int numSamples,
                         const float* a, float gain, float* output);
    void shiftFormants(const float* aIn, float shiftSemitones, float sampleRate,
                       float* aOut);
    void reset();

private:
    float mAutocorr[LPC_ORDER + 1];
    float mReflection[LPC_ORDER + 1];

    void autocorrelation(const float* frame, int frameSize);
};

//==============================================================================
// Simple Resampler (linear interpolation) for pitch shifting
//==============================================================================
class SimpleResampler
{
public:
    // Resample input to output using linear interpolation
    // pitchRatio > 1 shifts up, < 1 shifts down
    static void resample(const float* input, int inputLen,
                         float* output, int outputLen, float pitchRatio);
};

//==============================================================================
// Harmony Voice - Single harmony voice processor
//==============================================================================
class HarmonyVoice
{
public:
    HarmonyVoice();

    struct Params {
        int intervalSemitones = 3;    // Interval in semitones from lead vocal
        float gainDB = 0.0f;          // Gain in dB
        float pan = 0.0f;             // Pan: -1.0 (left) to +1.0 (right)
        bool enabled = true;
    };

    void prepare(double sampleRate, int blockSize);
    void processVoice(const float* input, int numSamples,
                      float intervalSemitones,
                      float formantPreserve,
                      const LPCFormantExtractor::LPCResult* lpcResult,
                      float* outLeft, float* outRight);

    void reset();

private:
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    Params mParams;

    // Buffers for pitch-shifted signal (mono)
    std::vector<float> mShiftedBuffer;
    std::vector<float> mExcitationBuffer;
    std::vector<float> mSynthesisBuffer;
};

//==============================================================================
// Main DSP Class: VCPluginDSP (VC-Harmonizer)
//==============================================================================
class VCPluginDSP
{
public:
    static constexpr int MAX_VOICES = 4;

    struct Params
    {
        int numVoices = 2;                              // Number of harmony voices (1-4)
        int intervals[MAX_VOICES] = {3, 7, 12, -5};    // Intervals in semitones
        float voiceGain[MAX_VOICES] = {0, 0, 0, 0};    // Gain per voice (dB)
        float voicePan[MAX_VOICES] = {-0.5f, 0.5f, 0.7f, -0.7f}; // Pan per voice (-1 to 1)
        float formantPreserve = 100.0f;                 // Formant preservation 0-100
        bool autoKey = false;                           // Auto key detection
        int scale = 0;                                  // Scale for quantization (0=Chromatic)
        int direction = 0;                              // 0=both, 1=up, 2=down
        bool bypass = false;
        int midiTrack = -1;                             // MIDI track placeholder (-1=off)
    };

    struct FrameInfo {
        float detectedF0 = 0.0f;
        float confidence = 0.0f;
        bool voiced = false;
        int detectedNote = 0;
        float voiceF0[MAX_VOICES] = {};
        int voiceNote[MAX_VOICES] = {};
        int voiceInterval[MAX_VOICES] = {};
    };

    VCPluginDSP();
    ~VCPluginDSP();

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

    // Key detection result
    KeyDetector::Result getKeyResult() const { return mKeyResult; }
    bool hasKeyDetected() const { return mKeyDetected; }

    // Frame info for reporting
    const std::vector<FrameInfo>& getFrameInfo() const { return mFrameInfo; }
    void setReportMode(bool enable) { mReportMode = enable; }

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

    // Subprocessors
    YINPitchDetector mPitchDetector;
    ScaleQuantizer mQuantizer;
    KeyDetector mKeyDetector;
    LPCFormantExtractor mLPC;
    HarmonyVoice mVoices[MAX_VOICES];

    // Key detection state
    bool mKeyDetected = false;
    KeyDetector::Result mKeyResult;
    int mAutoKeyOffset = 0;
    ScaleQuantizer::Scale mAutoScale = ScaleQuantizer::Chromatic;

    // Analysis settings
    int mFrameSize = 2048;
    int mHopSize = 1024;

    // Report mode
    bool mReportMode = false;
    std::vector<FrameInfo> mFrameInfo;

    // Internal buffers
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    // Per-voice output buffers (mono)
    std::vector<float> mVoiceMono[MAX_VOICES];

    // Compute effective interval for a voice given direction setting
    int computeEffectiveInterval(int baseInterval) const;

    // Quantize a target note to the current scale
    int quantizeNoteToScale(int note, ScaleQuantizer::Scale scale, int keyOffset) const;

    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
