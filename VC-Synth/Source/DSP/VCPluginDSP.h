#pragma once

//==============================================================================
// VC-Synth DSP Core Header — Subtractive Synthesizer (VCMix VSTi)
// Signal Flow: Oscillator(s) → Moog Ladder Filter → ADSR Amplifier → FX
// Features: Polyblep anti-aliased oscillators, Unison, Moog-style filter,
//           ADSR envelope, lightweight reverb + delay
//==============================================================================

// Shared constants (available in both JUCE and Standalone modes)
constexpr float VC_PI = 3.14159265358979323846f;
constexpr double VC_PI_D = 3.14159265358979323846;

#ifdef VC_STANDALONE
// Standalone mode: no JUCE dependency
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdlib>

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
// Oscillator Waveform Types
//==============================================================================
enum VCOscType {
    VC_OSC_SINE = 0,
    VC_OSC_SAW,
    VC_OSC_SQUARE,
    VC_OSC_TRIANGLE,
    VC_OSC_NOISE
};

//==============================================================================
// Filter Types
//==============================================================================
enum VCFilterType {
    VC_FILTER_LP = 0,
    VC_FILTER_BP,
    VC_FILTER_HP
};

//==============================================================================
// ADSR State
//==============================================================================
enum VCADSRState {
    VC_ADSR_IDLE = 0,
    VC_ADSR_ATTACK,
    VC_ADSR_DECAY,
    VC_ADSR_SUSTAIN,
    VC_ADSR_RELEASE
};

//==============================================================================
// VCOscillator — Polyblep anti-aliased oscillator with unison
//==============================================================================
class VCOscillator {
public:
    static constexpr int MAX_UNISON = 7;

    VCOscillator();

    void setSampleRate(double sr);
    void setOscType(VCOscType type);
    void setFrequency(float freq);
    void setUnison(int voices);
    void setDetune(float cents);   // total spread in cents

    // Generate one sample (call per sample)
    float process();

    // Reset phase
    void reset();

private:
    double mSampleRate = 44100.0;
    VCOscType mType = VC_OSC_SAW;
    float mFrequency = 440.0f;
    int mUnison = 1;
    float mDetune = 10.0f;  // cents spread

    // Per-voice state
    struct UnisonVoice {
        double phase = 0.0;         // 0..1
        double phaseInc = 0.0;      // phase increment per sample
        double lastValue = 0.0f;    // for polyblep (previous raw output)
        float detuneRatio = 1.0f;   // frequency ratio from detune
        float panL = 1.0f;          // stereo pan left
        float panR = 1.0f;          // stereo pan right
    };

    UnisonVoice mVoices[MAX_UNISON];

    // Update phase increments after freq/unison/detune change
    void updatePhaseIncrements();

    // Core waveform generation (naive, before polyblep)
    double naiveWaveform(double phase) const;

    // Polyblep correction
    double polyblep(double phase, double phaseInc, double boundaryValue) const;
};

//==============================================================================
// VCMoogFilter — Moog-style ladder filter
// Based on Antti Huovilainen's model with 4 one-pole stages + global feedback
//==============================================================================
class VCMoogFilter {
public:
    VCMoogFilter();

    void setSampleRate(double sr);
    void setCutoff(float hz);       // 20..20000
    void setResonance(float res);   // 0..1
    void setFilterType(VCFilterType type);

    // Process one sample (mono)
    float process(float input);

    // Reset filter state
    void reset();

private:
    double mSampleRate = 44100.0;
    float mCutoff = 8000.0f;
    float mResonance = 0.5f;
    VCFilterType mFilterType = VC_FILTER_LP;

    // Filter state — 4 stages
    float mStage[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    // Coefficients
    float mAlpha = 0.0f;   // per-stage coefficient
    float mK = 0.0f;       // feedback gain

    void updateCoefficients();
};

//==============================================================================
// VCADSR — Attack-Decay-Sustain-Release envelope generator
//==============================================================================
class VCADSR {
public:
    VCADSR();

    void setSampleRate(double sr);
    void setAttack(float ms);
    void setDecay(float ms);
    void setSustain(float level);   // 0..1
    void setRelease(float ms);

    // Note On / Off
    void noteOn();
    void noteOff();

    // Generate one sample
    float process();

    // Current state
    VCADSRState getState() const { return mState; }
    bool isActive() const { return mState != VC_ADSR_IDLE; }

    // Reset
    void reset();

private:
    double mSampleRate = 44100.0;
    float mAttackMs = 10.0f;
    float mDecayMs = 100.0f;
    float mSustainLevel = 0.7f;
    float mReleaseMs = 200.0f;

    VCADSRState mState = VC_ADSR_IDLE;
    float mCurrentLevel = 0.0f;

    // Coefficients (for exponential envelope)
    float mAttackCoeff = 0.0f;
    float mDecayCoeff = 0.0f;
    float mReleaseCoeff = 0.0f;
    float mReleaseLevel = 0.0f;  // level when release started

    void updateCoefficients();

    // Time constant to coefficient for exponential approach
    static float timeToCoeff(float timeMs, double sampleRate);
};

//==============================================================================
// VCSimpleDelay — Lightweight delay effect
//==============================================================================
class VCSimpleDelay {
public:
    VCSimpleDelay();

    void setSampleRate(double sr);
    void setDelayTime(float ms);
    void setMix(float mix);  // 0..1

    float process(float input);
    void reset();

private:
    double mSampleRate = 44100.0;
    float mDelayTimeMs = 375.0f;
    float mMix = 0.0f;

    std::vector<float> mBuffer;
    int mWritePos = 0;
    int mDelaySamples = 0;
    float mFeedback = 0.4f;  // fixed moderate feedback

    void updateDelay();
};

//==============================================================================
// VCSimpleReverb — Lightweight reverb (Schroeder: 4 comb + 2 allpass)
//==============================================================================
class VCSimpleReverb {
public:
    VCSimpleReverb();

    void setSampleRate(double sr);
    void setMix(float mix);  // 0..1

    float process(float input);
    void reset();

private:
    double mSampleRate = 44100.0;
    float mMix = 0.0f;

    // Comb filter delay lines
    static constexpr int NUM_COMBS = 4;
    struct CombFilter {
        std::vector<float> buffer;
        int writePos = 0;
        int length = 0;
        float feedback = 0.0f;
        float lpState = 0.0f;
        float lpCoeff = 0.0f;
    };
    CombFilter mCombs[NUM_COMBS];

    // Allpass delay lines
    static constexpr int NUM_ALLPASS = 2;
    struct AllpassFilter {
        std::vector<float> buffer;
        int writePos = 0;
        int length = 0;
        float feedback = 0.5f;
    };
    AllpassFilter mAllpass[NUM_ALLPASS];

    // Comb delay lengths at 44100Hz reference
    static constexpr int COMB_DELAYS[NUM_COMBS] = { 1557, 1617, 1491, 1422 };
    static constexpr int ALLPASS_DELAYS[NUM_ALLPASS] = { 225, 556 };

    void initDelayLines();
    int scaleDelay(int baseDelay) const;
};

//==============================================================================
// ActiveNote — A single sounding note
//==============================================================================
struct ActiveNote {
    int noteNumber = 60;
    float velocity = 1.0f;
    VCOscillator oscillator;
    VCMoogFilter filter;
    VCADSR adsr;
    bool active = false;

    void resetAll() {
        noteNumber = 60;
        velocity = 1.0f;
        oscillator.reset();
        filter.reset();
        adsr.reset();
        active = false;
    }
};

//==============================================================================
// Main DSP Class — VC-Synth
//==============================================================================
class VCPluginDSP
{
public:
    //==========================================================================
    // Synth Parameters
    //==========================================================================
    struct Params
    {
        VCOscType oscType = VC_OSC_SAW;
        int unison = 1;
        float detune = 10.0f;          // cents
        float cutoff = 8000.0f;        // Hz
        float resonance = 0.5f;        // 0..1
        VCFilterType filterType = VC_FILTER_LP;
        float attack = 10.0f;          // ms
        float decay = 100.0f;          // ms
        float sustain = 0.7f;          // 0..1
        float release = 200.0f;        // ms
        float reverbMix = 0.0f;        // 0..1
        float delayMix = 0.0f;         // 0..1
        float delayTime = 375.0f;      // ms
        float volumeDB = 0.0f;         // dB
        bool enabled = true;
    };

    //==========================================================================
    // Construction / Destruction
    VCPluginDSP();
    ~VCPluginDSP();

    //==========================================================================
    // Processing
    void prepare(double sampleRate, int blockSize);

    // Render audio to stereo output buffers (for standalone CLI)
    void render(float* left, float* right, int numSamples);

#ifndef VC_STANDALONE
    // JUCE AudioBlock processing
    void process(juce::dsp::AudioBlock<float>& block);
#endif

    void reset();

    //==========================================================================
    // MIDI-like note control
    void noteOn(int noteNumber, float velocity);
    void noteOff(int noteNumber);

    // Render a specific note for a duration (CLI convenience)
    void renderNote(int noteNumber, float velocity, float durationSeconds,
                    float* left, float* right, int& numSamplesRendered);

    //==========================================================================
    // Parameter access
    void setParams(const Params& p);
    Params getParams() const;

    void setEnabled(bool enabled);
    bool isEnabled() const { return mEnabled; }

    //==========================================================================
    // Preset management
    static const char* getPresetName(int index);
    static int getNumPresets();
    static bool getPreset(int index, Params& p);
    static bool getPresetByName(const char* name, Params& p);

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

    // MIDI note number to frequency
    static float midiNoteToFreq(int note) {
        return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
    }

    double getSampleRate() const { return mSampleRate; }
    int getBlockSize() const { return mBlockSize; }

private:
    //==========================================================================
    // Internal
    void updateVoiceParams();
    void silenceExpiredNotes();

    //==========================================================================
    // Voice management
    static constexpr int MAX_VOICES = 16;
    ActiveNote mVoices[MAX_VOICES];
    int mVoiceAllocIdx = 0;  // round-robin allocation

    //==========================================================================
    // Effects (stereo: process L and R separately with shared state is OK for mono->stereo)
    VCSimpleReverb mReverbL;
    VCSimpleReverb mReverbR;
    VCSimpleDelay mDelayL;
    VCSimpleDelay mDelayR;

    //==========================================================================
    // Member variables
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;
    float mVolumeLinear = 1.0f;

    // Internal buffer for JUCE AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
