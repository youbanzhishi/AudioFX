#pragma once

//==============================================================================
// VC-NoiseProfile DSP Core Header (Gen2)
// Noise Profile Analysis + Adaptive Spectral Subtraction + Noise Gate
// Gen1 signal generator features preserved
// Supports both JUCE and Standalone modes
//==============================================================================

constexpr float VC_PI = 3.14159265358979323846f;

#ifdef VC_STANDALONE
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cstring>

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
// Noise Types (Gen1 preserved)
//==============================================================================
enum class NoiseType : int {
    White = 0,
    Pink = 1,
    Brown = 2,
    Sine = 3,
    Sweep = 4,
    Impulse = 5
};

//==============================================================================
// Processing Mode (Gen2)
//==============================================================================
enum class ProcessMode : int {
    Denoise = 0,   // Spectral subtraction only
    Gate = 1,      // Noise gate only (Gen1)
    Both = 2,      // Denoise + Gate
    Analyze = 3    // Analyze & output noise profile only
};

//==============================================================================
// FFT Utilities (Gen2) — Radix-2 Cooley-Tukey, 512-point
//==============================================================================
class VCRadix2FFT
{
public:
    explicit VCRadix2FFT(int fftSize = 512);
    ~VCRadix2FFT();

    void forward(const float* input, float* realOut, float* imagOut) const;
    void inverse(const float* realIn, const float* imagIn, float* output) const;

    int getSize() const { return mSize; }

private:
    int mSize;
    std::vector<float> mSinTable;   // twiddle sine
    std::vector<float> mCosTable;   // twiddle cosine
    std::vector<int>   mBitRev;     // bit-reversal permutation

    void computeTwiddles();
    void computeBitReversal();
};

//==============================================================================
// Noise Profile (Gen2) — 64-band spectral energy
//==============================================================================
class VCNoiseProfile
{
public:
    static constexpr int NUM_BANDS = 64;

    VCNoiseProfile();
    ~VCNoiseProfile();

    // Reset profile
    void reset();

    // Accumulate one FFT frame into the profile
    void accumulate(const float* magnitude, int fftSize);

    // Finalize: average accumulated frames → final spectrum
    void finalize();

    // Get the learned 64-band energy profile
    const float* getBandEnergies() const { return mBandEnergies; }
    int getNumBands() const { return NUM_BANDS; }

    // Map full FFT magnitude spectrum to 64 bands
    void mapSpectrumToBands(const float* magnitude, int fftSize, float sampleRate);

    // Has a valid profile been learned?
    bool isValid() const { return mFrameCount > 0; }
    int getFrameCount() const { return mFrameCount; }

    // Get full-resolution noise power spectrum (interpolated from 64 bands)
    void getFullSpectrum(float* outPower, int fftSize, float sampleRate) const;

private:
    float mBandEnergies[NUM_BANDS];     // 64-band energy (dB-like)
    float mAccumBands[NUM_BANDS];       // accumulator during learning
    int   mFrameCount;

    // Frequency boundaries for 64 bands (computed from sample rate)
    void computeBandEdges(float sampleRate, int fftSize);
    float mBandEdges[NUM_BANDS + 1];    // frequency boundaries
};

//==============================================================================
// Spectral Subtractor (Gen2)
//==============================================================================
class VCSpectralSubtractor
{
public:
    VCSpectralSubtractor();
    ~VCSpectralSubtractor();

    void prepare(double sampleRate, int fftSize);
    void reset();

    // Process one FFT frame: subtract noise profile from signal
    // realIn/imagIn = current frame STFT, realOut/imagOut = cleaned
    void process(const float* realIn, const float* imagIn,
                 float* realOut, float* imagOut,
                 const VCNoiseProfile& profile);

    // Parameters
    void setReduction(float dB);    // 0~30 dB over-subtraction factor
    void setFloor(float ratio);     // 1~20 (spectral floor as ratio * 0.01)

    float getReduction() const { return mReduction; }
    float getFloor() const { return mFloor; }

private:
    double mSampleRate;
    int    mFFTSize;
    float  mReduction;      // dB, 0~30
    float  mFloor;          // spectral floor ratio (0.01~0.20)

    // Working buffers
    std::vector<float> mNoisePower;   // noise power spectrum (full FFT res)
    std::vector<float> mSigPower;     // signal power spectrum
    std::vector<float> mCleanPower;   // cleaned power spectrum
};

//==============================================================================
// Noise Gate (Gen2 — upgraded from Gen1)
//==============================================================================
class VCNoiseGate
{
public:
    VCNoiseGate();
    ~VCNoiseGate();

    void prepare(double sampleRate);
    void reset();

    // Process sample-by-sample (applied after spectral subtraction in time domain)
    void process(float* left, float* right, int numSamples);

    // Parameters
    void setThreshold(float dB) { mThreshold = dB; }
    void setAttack(float ms)    { mAttackMs = ms; }
    void setRelease(float ms)   { mReleaseMs = ms; }

    float getThreshold() const { return mThreshold; }
    float getAttack() const    { return mAttackMs; }
    float getRelease() const   { return mReleaseMs; }

private:
    double mSampleRate;
    float  mThreshold;     // dB
    float  mAttackMs;
    float  mReleaseMs;

    float  mEnvelope;      // current envelope level (linear)
    float  mGain;          // current gate gain (0~1)

    float  mAttackCoeff;
    float  mReleaseCoeff;

    void updateCoefficients();
};

//==============================================================================
// Main DSP Class (Gen2 — upgraded)
//==============================================================================
class VCPluginDSP
{
public:
    //==========================================================================
    // Plugin-specific parameter structure (Gen2)
    //==========================================================================
    struct Params
    {
        // Gen1 signal generator params (preserved)
        int type = 0;               // 0=White, 1=Pink, 2=Brown, 3=Sine, 4=Sweep, 5=Impulse
        float frequency = 1000.0f;  // 20~20000 Hz
        float endFreq = 20000.0f;   // 20~20000 Hz
        float sweepDuration = 5.0f; // 1~60 seconds
        bool sweepLog = true;
        float volume = -6.0f;       // -60~0 dBFS
        int channelMode = 0;        // 0=stereo, 1=left, 2=right, 3=anti-phase
        float pulsePeriod = 0.0f;   // 0~10 seconds
        bool enabled = true;

        // Gen2 noise profile params
        float learnMs = 500.0f;     // 100~5000 ms: learn noise from first N ms
        float reduction = 10.0f;    // 0~30 dB: spectral subtraction amount
        float floor = 5.0f;         // 1~20: spectral floor (ratio * 0.01)
        float threshold = -40.0f;   // -80~0 dB: noise gate threshold
        float attack = 5.0f;        // 0.1~100 ms: gate attack
        float release = 50.0f;      // 1~1000 ms: gate release
        int mode = 2;               // 0=Denoise, 1=Gate, 2=Both, 3=Analyze
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
    // Generator-specific: generate signal (no input needed) — Gen1 preserved
    //==========================================================================
    void generate(float* left, float* right, int numSamples);

    //==========================================================================
    // Gen2: Noise profile processing (for standalone CLI batch mode)
    //==========================================================================
    // Learn noise profile from input audio (first learnMs milliseconds)
    void learnNoiseProfile(const float* input, int numFrames, int channels);

    // Process input audio with learned profile (denoise/gate/both/analyze)
    void processWithProfile(float* left, float* right, int numSamples);

    // Get noise profile for external inspection
    const VCNoiseProfile& getNoiseProfile() const { return mNoiseProfile; }

    // Has noise profile been learned?
    bool hasNoiseProfile() const { return mNoiseProfile.isValid(); }

    //==========================================================================
    // Parameter access
    //==========================================================================
    void setParams(const Params& p);
    Params getParams() const;

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

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
    long long getSamplePosition() const { return mSamplePos; }
    void resetSamplePosition() { mSamplePos = 0; }

private:
    //==========================================================================
    // Gen1 noise generators (preserved)
    //==========================================================================
    float generateWhite();
    float generatePink();
    float generateBrown();
    float generateSine();
    float generateSweep();
    float generateImpulse();
    float randomUniform();

    //==========================================================================
    // Gen2: internal STFT processing
    //==========================================================================
    void processFrameSTFT(float* left, float* right, int numSamples);
    void analyzeOneFrame(const float* frameReal, const float* frameImag, int fftSize);

    //==========================================================================
    // Member variables
    //==========================================================================
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Gen1: Phase accumulator for sine/sweep
    double mPhase = 0.0;
    float mBrownState = 0.0f;
    static constexpr int PINK_NUM_ROWS = 8;
    float mPinkRows[PINK_NUM_ROWS] = {0.0f};
    int mPinkIndex = 0;
    float mPinkRunningSum = 0.0f;
    bool mImpulseFired = false;
    long long mImpulseCounter = 0;
    long long mSamplePos = 0;
    unsigned int mRandState = 12345;

    // Gen2: FFT and analysis
    static constexpr int FFT_SIZE = 512;
    static constexpr int HOP_SIZE = 256;   // 50% overlap

    VCRadix2FFT           mFFT;
    VCNoiseProfile        mNoiseProfile;
    VCSpectralSubtractor  mSpectralSub;
    VCNoiseGate           mNoiseGate;

    // STFT buffers
    std::vector<float> mInputBufferL;
    std::vector<float> mInputBufferR;
    int mInputWritePos;

    std::vector<float> mOverlapL;
    std::vector<float> mOverlapR;

    std::vector<float> mFFTReal;
    std::vector<float> mFFTImag;
    std::vector<float> mWindow;
    std::vector<float> mOutFrameL;
    std::vector<float> mOutFrameR;

    // Learning state
    bool mProfileLearned;
    int  mLearnSamplesRemaining;

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    //==========================================================================
    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
