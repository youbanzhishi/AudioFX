#pragma once

//==============================================================================
// VC-Drum: Drum Synthesizer Plugin
// 4 synthesis engines: Kick, Snare, Hi-hat, Clap
// Pattern sequencer with swing/humanize
// Bus compressor on output
//==============================================================================

constexpr float VC_PI = 3.14159265358979323846f;
constexpr double VC_PI_D = 3.14159265358979323846;

#ifdef VC_STANDALONE
#include <vector>
#include <cmath>
#include <algorithm>
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
// Kick Drum Synthesizer
// Sine wave with frequency sweep: freq(t) = freq_end + (freq_start - freq_end) * exp(-t/tau)
// Amplitude: fast attack + exponential decay
//==============================================================================
class VCKickSynth
{
public:
    struct Params {
        float freqStart = 150.0f;   // Hz - initial sweep frequency
        float freqEnd   = 50.0f;    // Hz - final frequency
        float decay     = 300.0f;   // ms - amplitude decay time
        float drive     = 1.0f;     // 0-2, adds harmonic content
    };

    VCKickSynth() = default;
    void prepare(double sampleRate);
    void noteOn(float velocity);
    float processSample();
    void reset();

    void setParams(const Params& p) { mParams = p; }
    const Params& getParams() const { return mParams; }
    bool isActive() const { return mEnvelope > 0.001f; }

private:
    double mSampleRate = 44100.0;
    Params mParams;
    double mPhase = 0.0;
    float mEnvelope = 0.0f;
    bool  mActive = false;
};

//==============================================================================
// Snare Drum Synthesizer
// Body: triangle wave + frequency sweep (200->100Hz)
// Noise: bandpass filtered white noise (center 3000Hz, Q=1)
// Mix ratio adjustable
//==============================================================================
class VCSnareSynth
{
public:
    struct Params {
        float tone       = 0.5f;    // 0-1, body vs noise mix
        float decay      = 200.0f;  // ms
        float freqStart  = 200.0f;  // Hz
        float freqEnd    = 100.0f;  // Hz
        float noiseFreq  = 3000.0f; // Hz - bandpass center
        float noiseQ     = 1.0f;    // bandpass Q
    };

    VCSnareSynth() = default;
    void prepare(double sampleRate);
    void noteOn(float velocity);
    float processSample();
    void reset();

    void setParams(const Params& p) { mParams = p; }
    const Params& getParams() const { return mParams; }
    bool isActive() const { return mEnvelope > 0.001f; }

private:
    float generateWhiteNoise();

    double mSampleRate = 44100.0;
    Params mParams;
    double mPhase = 0.0;
    float mEnvelope = 0.0f;
    bool  mActive = false;

    // Biquad bandpass filter for noise
    struct BiquadState {
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };
    BiquadState mBPF;
    void updateBPFCoefficients();
};

//==============================================================================
// Hi-Hat Synthesizer
// 6 detuned square waves -> bandpass filter -> amplitude envelope
// Open/Closed modes (different decay times)
//==============================================================================
class VCHiHatSynth
{
public:
    enum class Type { Closed, Open };

    struct Params {
        float decayClosed = 50.0f;   // ms
        float decayOpen   = 300.0f;  // ms
        Type  type        = Type::Closed;
        float bpfFreq     = 6000.0f; // Hz
        float bpfQ        = 0.8f;
    };

    VCHiHatSynth() = default;
    void prepare(double sampleRate);
    void noteOn(float velocity, Type type = Type::Closed);
    float processSample();
    void reset();

    void setParams(const Params& p) { mParams = p; }
    const Params& getParams() const { return mParams; }
    bool isActive() const { return mEnvelope > 0.001f; }

private:
    double mSampleRate = 44100.0;
    Params mParams;
    Type   mCurrentType = Type::Closed;

    // 6 square wave oscillators with detune
    static constexpr int kNumSquares = 6;
    double mPhases[kNumSquares] = {};
    // Metallic inharmonic ratios
    static constexpr float kDetuneRatios[kNumSquares] = {
        1.0f, 1.547f, 2.0f, 2.531f, 3.0f, 3.987f
    };

    float mEnvelope = 0.0f;
    bool  mActive = false;

    // BPF
    struct BiquadState {
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };
    BiquadState mBPF;
    void updateBPFCoefficients();
};

//==============================================================================
// Clap Synthesizer
// Bandpass filtered white noise with multiple short bursts
//==============================================================================
class VCClapSynth
{
public:
    struct Params {
        int   clapCount = 3;       // 3-8 number of bursts
        float spread    = 15.0f;   // ms between bursts
        float decay     = 250.0f;  // ms overall decay
        float bpfFreq   = 1200.0f; // Hz
        float bpfQ      = 0.7f;
    };

    VCClapSynth() = default;
    void prepare(double sampleRate);
    void noteOn(float velocity);
    float processSample();
    void reset();

    void setParams(const Params& p) { mParams = p; }
    const Params& getParams() const { return mParams; }
    bool isActive() const { return mEnvelope > 0.001f; }

private:
    float generateWhiteNoise();

    double mSampleRate = 44100.0;
    Params mParams;
    float mEnvelope = 0.0f;
    bool  mActive = false;

    // Burst sequencer
    int    mCurrentBurst = 0;
    double mBurstTimer = 0.0;   // samples since noteOn
    double mBurstInterval = 0.0; // samples between bursts
    float  mVelocity = 0.0f;

    // BPF
    struct BiquadState {
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };
    BiquadState mBPF;
    void updateBPFCoefficients();
};

//==============================================================================
// Bus Compressor (simple peak compressor)
//==============================================================================
class VCDrumBusCompressor
{
public:
    struct Params {
        bool  enabled    = true;
        float threshold  = -8.0f;  // dB
        float ratio      = 4.0f;   // compression ratio
        float attack     = 5.0f;   // ms
        float release    = 50.0f;  // ms
        float makeupGain = 0.0f;   // dB
    };

    VCDrumBusCompressor() = default;
    void prepare(double sampleRate);
    void process(float* left, float* right, int numSamples);
    void reset();

    void setParams(const Params& p);
    const Params& getParams() const { return mParams; }

private:
    double mSampleRate = 44100.0;
    Params mParams;
    float mEnvelopeFollower = 0.0f;
    float mGainSmooth = 1.0f;
};

//==============================================================================
// Pattern Sequencer
// Defines which drums hit on which 16th note positions
//==============================================================================
class VCPatternSequencer
{
public:
    enum class DrumType { Kick = 0, Snare, HiHat, Clap, NumTypes };

    struct Step {
        bool  active   = false;
        float velocity = 1.0f;
    };

    // A pattern is 16 steps (1 bar of 16th notes at given BPM)
    struct Pattern {
        Step drums[static_cast<int>(DrumType::NumTypes)][16];
    };

    VCPatternSequencer() = default;
    void prepare(double sampleRate, float bpm);
    void setBPM(float bpm);
    void setSwing(float percent);     // 0-100
    void setHumanize(float percent);  // 0-100
    void setPattern(const Pattern& p) { mPattern = p; }
    const Pattern& getPattern() const { return mPattern; }

    // Call once per sample. Returns bitmask of which drums triggered this sample.
    // Bit 0 = Kick, Bit 1 = Snare, Bit 2 = HiHat, Bit 3 = Clap
    // Also fills velocities array
    int processSample(float velocities[static_cast<int>(DrumType::NumTypes)]);

    void reset();

    // Built-in pattern generators
    static Pattern createKickOnlyPattern();
    static Pattern createSnareOnlyPattern();
    static Pattern createHiHatOnlyPattern();
    static Pattern createBasicBeatPattern();
    static Pattern createHousePattern();
    static Pattern createTechnoPattern();
    static Pattern createHipHopPattern();
    static Pattern createTrapPattern();
    static Pattern createDNBPattern();
    static Pattern createFullPattern();

private:
    double mSampleRate = 44100.0;
    float  mBPM = 120.0f;
    float  mSwing = 0.0f;       // 0-1
    float  mHumanize = 0.0f;    // 0-1
    Pattern mPattern;

    // Sequencer state
    int    mCurrentStep = 0;
    double mSampleCounter = 0.0;
    double mStepDuration = 0.0;  // samples per 16th note

    // Humanize: pre-computed offset and velocity for current step
    double mHumanizeOffset = 0.0;
    float  mHumanizeVelocity[static_cast<int>(DrumType::NumTypes)] = {};
    bool   mStepTriggered = false;
};

//==============================================================================
// Main DSP Class: VCDrumEngine
//==============================================================================
class VCPluginDSP
{
public:
    struct Params
    {
        // Pattern
        int   pattern = 0;       // 0=full, 1=kick-only, 2=snare-only, 3=hihat-only, 4=basic-beat
                                // 5=house, 6=techno, 7=hiphop, 8=trap, 9=dnb
        float bpm      = 120.0f;
        float swing    = 0.0f;   // 0-100
        float humanize = 0.0f;   // 0-100

        // Kick params
        VCKickSynth::Params kick;

        // Snare params
        VCSnareSynth::Params snare;

        // Hi-hat params
        VCHiHatSynth::Params hihat;

        // Clap params
        VCClapSynth::Params clap;

        // Bus compressor
        VCDrumBusCompressor::Params compressor;

        // Master
        float masterGain = 0.0f;  // dB
        bool  enabled = true;
    };

    //==============================================================================
    VCPluginDSP();
    ~VCPluginDSP();

    //==============================================================================
    void prepare(double sampleRate, int blockSize);
    void process(float* left, float* right, int numSamples);

#ifndef VC_STANDALONE
    void process(juce::dsp::AudioBlock<float>& block);
#endif

    void reset();

    //==============================================================================
    // MIDI-triggered drum methods (for VST3 instrument mode)
    // Trigger a specific drum engine by type
    // drumType: 0=Kick, 1=Snare, 2=HiHat, 3=Clap
    // openHiHat: only used for HiHat (true=open, false=closed)
    void triggerDrum(int drumType, float velocity, bool openHiHat = false);

    // Render audio from active drum engines (MIDI-triggered, no sequencer)
    void render(float* left, float* right, int numSamples);

    //==============================================================================
    void setParams(const Params& p);
    Params getParams() const;

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

    //==============================================================================
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

    // For CLI: render N bars to buffers
    void renderBars(int bars, std::vector<float>& outLeft, std::vector<float>& outRight);

private:
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;

    // Synth engines
    VCKickSynth   mKick;
    VCSnareSynth  mSnare;
    VCHiHatSynth  mHiHat;
    VCClapSynth   mClap;

    // Sequencer (used in CLI/standalone mode)
    VCPatternSequencer mSequencer;

    // MIDI mode: when true, sequencer is bypassed and drums are triggered via triggerDrum()
    bool mMidiMode = false;

    // Bus compressor
    VCDrumBusCompressor mCompressor;

    // Internal buffer for AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
