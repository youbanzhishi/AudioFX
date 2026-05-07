#pragma once

//==============================================================================
// VC-NoiseProfile JUCE Audio Processor Header
// Noise Profile Analysis + Adaptive Spectral Subtraction + Noise Gate
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
    static const juce::String bypass          = "bypass";
    static const juce::String processMode     = "processMode";
    static const juce::String reduction       = "reduction";
    static const juce::String noiseFloor      = "noiseFloor";
    static const juce::String attack          = "attack";
    static const juce::String release         = "release";
    static const juce::String fftSize         = "fftSize";
    static const juce::String noiseLearnTime  = "noiseLearnTime";
    static const juce::String gateThreshold   = "gateThreshold";
    static const juce::String gateDepth       = "gateDepth";
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
class VCNoiseProfileProcessor : public juce::AudioProcessor,
                                 public juce::AudioProcessorValueTreeState::Listener
{
public:
    VCNoiseProfileProcessor();
    ~VCNoiseProfileProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiBuffer) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VC-NoiseProfile"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return mAPVTS; }
    VCPluginDSP& getDSP() { return mDSP; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::dsp::ProcessSpec mProcessSpec;
    VCPluginDSP mDSP;

    bool mBypass = false;

    juce::AudioProcessorValueTreeState mAPVTS;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCNoiseProfileProcessor)
};
