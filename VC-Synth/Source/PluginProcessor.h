#pragma once

//==============================================================================
// VC-Synth JUCE Audio Processor Header
// Virtual Instrument (VSTi) — Subtractive Synthesizer
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
    static const juce::String oscType     = "osc_type";
    static const juce::String unison      = "unison";
    static const juce::String detune      = "detune";
    static const juce::String cutoff      = "cutoff";
    static const juce::String resonance   = "resonance";
    static const juce::String filterType  = "filter_type";
    static const juce::String attack      = "attack";
    static const juce::String decay       = "decay";
    static const juce::String sustain     = "sustain";
    static const juce::String release     = "release";
    static const juce::String reverbMix   = "reverb_mix";
    static const juce::String delayMix    = "delay_mix";
    static const juce::String delayTime   = "delay_time";
    static const juce::String volume      = "volume";
    static const juce::String bypass      = "bypass";
}

//==============================================================================
// Configuration Constants
//==============================================================================
namespace Config
{
    static const int kBlockSize = 512;
}

//==============================================================================
// Main Audio Processor Class
//==============================================================================
class VCSynthProcessor : public juce::AudioProcessor,
                          public juce::AudioProcessorValueTreeState::Listener
{
public:
    //============================================================================
    // Construction / Destruction
    //============================================================================
    VCSynthProcessor();
    ~VCSynthProcessor() override;

    //============================================================================
    // JUCE AudioProcessor Interface
    //============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiBuffer) override;

    //============================================================================
    // Editor
    //============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //============================================================================
    // Plugin Information — VSTi specific
    //============================================================================
    const juce::String getName() const override { return "VC-Synth"; }
    bool acceptsMidi() const override { return true; }   // VSTi: accepts MIDI
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }  // reverb/delay tail

    //============================================================================
    // Program (Preset) Support
    //============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override { return mCurrentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    //============================================================================
    // State Save/Restore
    //============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //============================================================================
    // Parameter Listener Callback
    //============================================================================
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    //============================================================================
    // Accessor for APVTS
    //============================================================================
    juce::AudioProcessorValueTreeState& getAPVTS() { return mAPVTS; }

    //============================================================================
    // DSP Instance Access
    //============================================================================
    VCPluginDSP& getDSP() { return mDSP; }

private:
    //============================================================================
    // Create Parameter Layout
    //============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //============================================================================
    // DSP Processing Specification
    //============================================================================
    juce::dsp::ProcessSpec mProcessSpec;

    //============================================================================
    // DSP Processing Class
    //============================================================================
    VCPluginDSP mDSP;

    //============================================================================
    // Processing State
    //============================================================================
    int mCurrentProgram = 1;  // default to "init" preset

    //============================================================================
    // AudioProcessorValueTreeState
    //============================================================================
    juce::AudioProcessorValueTreeState mAPVTS;

    //============================================================================
    // Non-copyable
    //============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCSynthProcessor)
};
