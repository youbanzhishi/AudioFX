#pragma once

//==============================================================================
// VC-Harmonizer JUCE Audio Processor Header
// Intelligent Harmony Generator
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
    static const juce::String bypass = "bypass";
    static const juce::String numVoices = "numVoices";
    static const juce::String interval0 = "interval0";
    static const juce::String interval1 = "interval1";
    static const juce::String interval2 = "interval2";
    static const juce::String interval3 = "interval3";
    static const juce::String voiceGain0 = "voiceGain0";
    static const juce::String voiceGain1 = "voiceGain1";
    static const juce::String voiceGain2 = "voiceGain2";
    static const juce::String voiceGain3 = "voiceGain3";
    static const juce::String voicePan0 = "voicePan0";
    static const juce::String voicePan1 = "voicePan1";
    static const juce::String voicePan2 = "voicePan2";
    static const juce::String voicePan3 = "voicePan3";
    static const juce::String formantPreserve = "formantPreserve";
    static const juce::String autoKey = "autoKey";
    static const juce::String scale = "scale";
    static const juce::String direction = "direction";
    static const juce::String midiTrack = "midiTrack";
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
class VCHarmonizerProcessor : public juce::AudioProcessor,
                               public juce::AudioProcessorValueTreeState::Listener
{
public:
    VCHarmonizerProcessor();
    ~VCHarmonizerProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiBuffer) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VC-Harmonizer"; }
    bool acceptsMidi() const override { return true; }  // MIDI for harmony control
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCHarmonizerProcessor)
};
