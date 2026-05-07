#pragma once

//==============================================================================
// JUCE Audio Processor Header
// Template for VST3 Audio Plugin Development
//==============================================================================

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

#include "DSP/VCPluginDSP.h"

//==============================================================================
// Parameter IDs
// TODO: Replace with your plugin's parameter IDs
//==============================================================================
namespace ParameterIDs
{
    static const juce::String bypass = "bypass";
    static const juce::String gain = "gain";
    static const juce::String mix = "mix";
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
class VCGainProcessor : public juce::AudioProcessor,
                                  public juce::AudioProcessorValueTreeState::Listener
{
public:
    //============================================================================
    // Construction / Destruction
    //============================================================================
    VCGainProcessor();
    ~VCGainProcessor() override;

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
    // Plugin Information
    //============================================================================
    const juce::String getName() const override { return "VC-Gain"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //============================================================================
    // Program (Preset) Support
    //============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
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
    bool mBypass = false;
    float mGainDB = 0.0f;
    float mMix = 100.0f;

    //============================================================================
    // AudioProcessorValueTreeState
    //============================================================================
    juce::AudioProcessorValueTreeState mAPVTS;

    //============================================================================
    // Non-copyable
    //============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCGainProcessor)
};
