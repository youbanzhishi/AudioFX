#pragma once

//==============================================================================
// VC-Drum JUCE Audio Processor Header
// Virtual Instrument (VSTi) — Drum Synthesizer
// MIDI Note → Drum Engine mapping (GM Percussion standard)
// Kick=36, Snare=38, HiHat Closed=42, HiHat Open=46, Clap=39
//==============================================================================

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

#include "DSP/VCPluginDSP.h"

//==============================================================================
// Parameter IDs
//==============================================================================
namespace ParameterIDs
{
    // Kick
    static const juce::String kickFreqStart  = "kick_freq_start";
    static const juce::String kickFreqEnd    = "kick_freq_end";
    static const juce::String kickDecay      = "kick_decay";
    static const juce::String kickDrive      = "kick_drive";
    static const juce::String kickGain       = "kick_gain";
    // Snare
    static const juce::String snareTone      = "snare_tone";
    static const juce::String snareDecay     = "snare_decay";
    static const juce::String snareGain      = "snare_gain";
    // HiHat
    static const juce::String hihatDecayClosed = "hihat_decay_closed";
    static const juce::String hihatDecayOpen   = "hihat_decay_open";
    static const juce::String hihatGain        = "hihat_gain";
    // Clap
    static const juce::String clapCount      = "clap_count";
    static const juce::String clapDecay      = "clap_decay";
    static const juce::String clapGain       = "clap_gain";
    // Master
    static const juce::String masterGain     = "master_gain";
    static const juce::String bypass         = "bypass";
}

//==============================================================================
// GM Percussion Note Map
//==============================================================================
namespace DrumMap
{
    // Standard GM percussion note numbers (channel 10)
    static const int Kick        = 36;  // Bass Drum 1
    static const int Snare       = 38;  // Acoustic Snare
    static const int HiHatClosed = 42;  // Closed Hi-Hat
    static const int HiHatOpen   = 46;  // Open Hi-Hat
    static const int Clap        = 39;  // Hand Clap
    // Extended mapping
    static const int Rimshot     = 37;  // Side Stick
    static const int TomLow      = 45;  // Low Tom
    static const int TomMid      = 47;  // Mid Tom
    static const int TomHi       = 50;  // Hi Tom
    static const int Crash       = 49;  // Crash Cymbal
    static const int Ride        = 51;  // Ride Cymbal
}

//==============================================================================
// Drum Engine Types (bitmask for triggered drums)
//==============================================================================
enum DrumEngineType
{
    DrumKick   = 0,
    DrumSnare  = 1,
    DrumHiHat  = 2,
    DrumClap   = 3,
    DrumCount  = 4
};

//==============================================================================
// Main Audio Processor Class
//==============================================================================
class VCDrumProcessor : public juce::AudioProcessor,
                         public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==========================================================================
    // Construction / Destruction
    //==========================================================================
    VCDrumProcessor();
    ~VCDrumProcessor() override;

    //==========================================================================
    // JUCE AudioProcessor Interface
    //==========================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiBuffer) override;

    //==========================================================================
    // Editor
    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    // Plugin Information — VSTi specific
    //==========================================================================
    const juce::String getName() const override { return "VC-Drum"; }
    bool acceptsMidi() const override { return true; }   // VSTi: accepts MIDI
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    //==========================================================================
    // Program (Preset) Support
    //==========================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override { return mCurrentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    //==========================================================================
    // State Save/Restore
    //==========================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==========================================================================
    // Parameter Listener Callback
    //==========================================================================
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    //==========================================================================
    // Accessor for APVTS
    //==========================================================================
    juce::AudioProcessorValueTreeState& getAPVTS() { return mAPVTS; }

    //==========================================================================
    // DSP Instance Access
    //==========================================================================
    VCPluginDSP& getDSP() { return mDSP; }

private:
    //==========================================================================
    // Create Parameter Layout
    //==========================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==========================================================================
    // MIDI Note → Drum mapping
    //==========================================================================
    void handleMidiNoteOn(int noteNumber, float velocity);
    void handleMidiNoteOff(int noteNumber);

    //==========================================================================
    // DSP Processing Class
    //==========================================================================
    VCPluginDSP mDSP;

    //==========================================================================
    // Processing State
    //==========================================================================
    int mCurrentProgram = 0;

    //==========================================================================
    // Per-engine gain (dB)
    //==========================================================================
    float mKickGainDB   = 0.0f;
    float mSnareGainDB  = 0.0f;
    float mHiHatGainDB  = 0.0f;
    float mClapGainDB   = 0.0f;
    float mMasterGainDB = 0.0f;
    bool  mBypass       = false;

    //==========================================================================
    // AudioProcessorValueTreeState
    //==========================================================================
    juce::AudioProcessorValueTreeState mAPVTS;

    //==========================================================================
    // Non-copyable
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCDrumProcessor)
};
