#pragma once

//==============================================================================
// JUCE Audio Processor Header
// VC-Hall480 — Lexicon 480L-Class Algorithmic Reverb
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
    static const juce::String bypass      = "bypass";
    static const juce::String algorithm   = "algorithm";
    static const juce::String room        = "room";
    static const juce::String decay       = "decay";
    static const juce::String preDelay    = "predelay";
    static const juce::String diffusion   = "diffusion";
    static const juce::String shape       = "shape";
    static const juce::String spread      = "spread";
    static const juce::String hiDecay     = "hidecay";
    static const juce::String loDecay     = "lodecay";
    static const juce::String chorusRate  = "chorusrate";
    static const juce::String chorusDepth = "chorusdepth";
    static const juce::String mix         = "mix";
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
class VCHall480Processor : public juce::AudioProcessor,
                            public juce::AudioProcessorValueTreeState::Listener
{
public:
    VCHall480Processor();
    ~VCHall480Processor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiBuffer) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VC-Hall480"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

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
    int mAlgorithm = 0;
    float mRoomSize = 50.0f;
    float mDecayTime = 2.0f;
    float mPreDelay = 20.0f;
    float mDiffusion = 70.0f;
    float mShape = 50.0f;
    float mSpread = 80.0f;
    float mHiDecay = 0.5f;
    float mLoDecay = 1.0f;
    float mChorusRate = 1.0f;
    float mChorusDepth = 30.0f;
    float mMix = 30.0f;

    juce::AudioProcessorValueTreeState mAPVTS;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCHall480Processor)
};
