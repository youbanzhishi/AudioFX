#pragma once

//==============================================================================
// VC-Arp DSP Core Header — Arpeggiator Plugin (VCMix)
// Receives MIDI note input, generates arpeggio patterns automatically
// CLI mode: includes built-in simple synth for audio output
// VST3 mode: outputs MIDI notes (future)
//==============================================================================

constexpr float VC_PI = 3.14159265358979323846f;
constexpr double VC_PI_D = 3.14159265358979323846;

#ifdef VC_STANDALONE
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
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
// Arpeggio Mode
//==============================================================================
enum VCArpMode {
    VC_ARP_UP = 0,
    VC_ARP_DOWN,
    VC_ARP_UP_DOWN,
    VC_ARP_DOWN_UP,
    VC_ARP_RANDOM,
    VC_ARP_AS_PLAYED,
    VC_ARP_CHORD
};

//==============================================================================
// Velocity Mode
//==============================================================================
enum VCVelocityMode {
    VC_VEL_ORIGINAL = 0,
    VC_VEL_ASCENDING,
    VC_VEL_DESCENDING,
    VC_VEL_RANDOM
};

//==============================================================================
// Rate values — musical subdivisions
//==============================================================================
enum VCArpRate {
    VC_RATE_1_1  = 0,   // whole note
    VC_RATE_1_2,         // half note
    VC_RATE_1_4,         // quarter note
    VC_RATE_1_8,         // eighth note
    VC_RATE_1_16,        // sixteenth note
    VC_RATE_1_32         // thirty-second note
};

//==============================================================================
// VCArpPattern — Arpeggio pattern generator
// Takes a set of MIDI notes + mode + octave range, outputs ordered note sequence
//==============================================================================
class VCArpPattern
{
public:
    VCArpPattern() = default;

    void setMode(VCArpMode mode) { mMode = mode; rebuildPattern(); }
    void setOctaveRange(int range) { mOctaveRange = std::clamp(range, 1, 4); rebuildPattern(); }
    void setTranspose(int semitones) { mTranspose = semitones; rebuildPattern(); }

    // Set the input notes (MIDI note numbers)
    void setNotes(const std::vector<int>& notes);

    // Get the full arpeggio pattern (one cycle)
    const std::vector<int>& getPattern() const { return mPattern; }

    // Get pattern length
    int getPatternLength() const { return (int)mPattern.size(); }

private:
    VCArpMode mMode = VC_ARP_UP;
    int mOctaveRange = 1;
    int mTranspose = 0;
    std::vector<int> mInputNotes;  // sorted unique input notes
    std::vector<int> mPattern;     // full pattern including octave expansion

    void rebuildPattern();
};

//==============================================================================
// VCArpSequencer — Arpeggio sequencer
// Triggers notes from the pattern at the specified rate
// Handles gate, swing, humanize, velocity
//==============================================================================
class VCArpSequencer
{
public:
    // Event emitted when a note triggers
    struct ArpEvent {
        int   noteNumber = 60;
        float velocity   = 1.0f;
        bool  noteOn     = true;
    };

    VCArpSequencer() = default;

    void prepare(double sampleRate, float bpm);
    void setBPM(float bpm);
    void setRate(VCArpRate rate);
    void setGate(float percent);       // 1-200%, note length as % of step duration
    void setSwing(float percent);      // 0-100%
    void setHumanize(float percent);   // 0-100%
    void setVelocityMode(VCVelocityMode mode);
    void setPattern(const VCArpPattern& pattern);

    // Process one sample. Returns list of events triggered this sample.
    // Typically 0 or 1 events, but chord mode can trigger multiple.
    void triggerCurrentStep(std::vector<ArpEvent>& events);
    void processSample(std::vector<ArpEvent>& events);

    void reset();

private:
    double mSampleRate = 44100.0;
    float  mBPM = 120.0f;
    VCArpRate mRate = VC_RATE_1_8;
    float  mGate = 100.0f;       // percent
    float  mSwing = 0.0f;        // percent
    float  mHumanize = 0.0f;     // percent
    VCVelocityMode mVelocityMode = VC_VEL_ORIGINAL;

    // Pattern data
    std::vector<int> mPatternNotes;
    int mPatternLength = 0;

    // Sequencer state
    int    mCurrentStep = 0;
    double mSampleCounter = 0.0;
    double mCurrentStepDuration = 0.0;  // samples for current step (may vary with swing)
    bool   mNoteActive = false;
    double mNoteOffSample = 0.0;        // when to turn off current note
    int    mCurrentNote = 60;
    float  mCurrentVelocity = 1.0f;

    // For up-down and down-up: track direction
    bool   mForward = true;

    // Humanize pre-computed for current step
    double mHumanizeOffset = 0.0;
    float  mHumanizeVelocityScale = 1.0f;

    // Random state for humanize/random velocity
    float randomFloat();
    int   randomInt(int min, int max);

    // Calculate step duration in samples
    double calcStepDuration(int step) const;

    // Calculate base step duration without swing
    double calcBaseStepDuration() const;

    // Calculate velocity for a step
    float calcVelocity(int step) const;

    // Update step duration for current step (handles swing)
    void updateStepDuration();
};

//==============================================================================
// VCArpSynth — Simple synthesizer for CLI standalone mode
// Sine/Saw/Square oscillator with exponential decay envelope
//==============================================================================
class VCArpSynth
{
public:
    enum Waveform { SINE = 0, SAW, SQUARE };

    VCArpSynth() = default;
    void prepare(double sampleRate);
    void setWaveform(Waveform wf);
    void noteOn(int noteNumber, float velocity);
    void noteOff();
    float processSample();
    bool isActive() const { return mEnvelope > 0.001f; }
    void reset();

private:
    double mSampleRate = 44100.0;
    Waveform mWaveform = SINE;
    double mPhase = 0.0;
    double mPhaseInc = 0.0;
    float  mEnvelope = 0.0f;
    float  mVelocity = 0.0f;
    float  mFreq = 440.0f;
    bool   mActive = false;

    // Simple exponential decay
    float  mDecayCoeff = 0.0f;
};

//==============================================================================
// Main DSP Class — VC-Arp
//==============================================================================
class VCPluginDSP
{
public:
    //==========================================================================
    // Plugin Parameters
    //==========================================================================
    struct Params
    {
        VCArpMode      mode         = VC_ARP_UP;
        VCArpRate      rate         = VC_RATE_1_8;
        int            octaveRange  = 1;
        float          gate         = 100.0f;   // 1-200%
        float          swing        = 0.0f;     // 0-100%
        VCVelocityMode velocityMode = VC_VEL_ORIGINAL;
        float          bpm          = 120.0f;
        int            transpose    = 0;        // semitones
        float          humanize     = 0.0f;     // 0-100%
        VCArpSynth::Waveform waveform = VCArpSynth::SINE;
        float          volumeDB     = -6.0f;    // dB
        bool           enabled      = true;
    };

    //==========================================================================
    // Construction / Destruction
    VCPluginDSP();
    ~VCPluginDSP();

    //==========================================================================
    // Processing
    void prepare(double sampleRate, int blockSize);

    // Render arpeggio audio (CLI standalone mode)
    void render(float* left, float* right, int numSamples);

#ifndef VC_STANDALONE
    // JUCE AudioBlock processing
    void process(juce::dsp::AudioBlock<float>& block);
#endif

    void reset();

    //==========================================================================
    // Note input (for real-time MIDI input)
    void noteOn(int noteNumber, float velocity);
    void noteOff(int noteNumber);

    // Set the chord notes for the arpeggiator
    void setChordNotes(const std::vector<int>& notes);

    //==========================================================================
    // Render N bars to buffers (CLI convenience)
    void renderBars(int bars, std::vector<float>& outLeft, std::vector<float>& outRight);

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

    static float midiNoteToFreq(int note) {
        return 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
    }

    double getSampleRate() const { return mSampleRate; }
    int getBlockSize() const { return mBlockSize; }

private:
    //==========================================================================
    double mSampleRate = 44100.0;
    int mBlockSize = 512;
    bool mEnabled = true;
    Params mParams;
    float mVolumeLinear = 0.5f;

    // Arpeggiator components
    VCArpPattern   mPattern;
    VCArpSequencer mSequencer;
    VCArpSynth     mSynth;

    // Held notes (MIDI input)
    std::vector<int> mHeldNotes;

    // Internal buffer for JUCE AudioBlock conversion
    std::vector<float> mInternalBuffer;
    std::vector<float*> mInternalPtrs;

    VC_DECLARE_NON_COPYABLE(VCPluginDSP)
};
