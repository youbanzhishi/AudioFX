#pragma once

//==============================================================================
// VC-Arp JUCE Audio Processor Header
// Virtual Instrument (VSTi) — Arpeggiator
// Receives MIDI chords → generates arpeggio patterns → outputs audio
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
    static const juce::String mode         = "mode";
    static const juce::String rate         = "rate";
    static const juce::String octaveRange  = "octave_range";
    static const juce::String gate         = "gate";
    static const juce::String swing        = "swing";
    static const juce::String humanize     = "humanize";
    static const juce::String velocityMode = "velocity_mode";
    static const juce::String bpm          = "bpm";
    static const juce::String transpose    = "transpose";
    static const juce::String waveform     = "waveform";
    static const juce::String volume       = "volume";
    static const juce::String bypass       = "bypass";
}

//==============================================================================
// Main Audio Processor Class
//==============================================================================
class VCArpProcessor : public juce::AudioProcessor,
                        public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==========================================================================
    // Construction / Destruction
    //==========================================================================
    VCArpProcessor();
    ~VCArpProcessor() override;

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
    const juce::String getName() const override { return "VC-Arp"; }
    bool acceptsMidi() const override { return true; }   // VSTi: accepts MIDI
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

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
    // DSP Processing Class
    //==========================================================================
    VCPluginDSP mDSP;

    //==========================================================================
    // Processing State
    //==========================================================================
    int mCurrentProgram = 0;
    bool mBypass = false;

    //==========================================================================
    // Held MIDI notes (for chord tracking)
    //==========================================================================
    std::vector<int> mHeldNotes;

    //==========================================================================
    // AudioProcessorValueTreeState
    //==========================================================================
    juce::AudioProcessorValueTreeState mAPVTS;

    //==========================================================================
    // Non-copyable
    //==========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCArpProcessor)
};
