// ============================================================================
// PluginProcessor.h - VC-SpaceMaker JUCE Plugin Processor Header
// ============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/VCSpaceMakerDSP.h"

//==============================================================================
class VCSpaceMakerProcessor : public juce::AudioProcessor
{
public:
    VCSpaceMakerProcessor();
    ~VCSpaceMakerProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VC-SpaceMaker"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // 获取频谱数据（给编辑器使用）
    const VCSpaceMakerDSP::SpectrumData& getSpectrumData() const { return dsp.getSpectrumData(); }
    const float* getBandFrequencies() const { return dsp.getBandFrequencies(); }

    // 参数访问（给编辑器使用）
    juce::AudioProcessorValueTreeState& getValueTreeState() { return vts; }

private:
    VCSpaceMakerDSP dsp;

    // AudioProcessorValueTreeState - 替代单独的 addParameter
    juce::AudioProcessorValueTreeState vts;

    juce::AudioParameterFloat* amountParam;
    juce::AudioParameterFloat* attackParam;
    juce::AudioParameterFloat* releaseParam;
    juce::AudioParameterFloat* lowCutParam;
    juce::AudioParameterFloat* highCutParam;
    juce::AudioParameterChoice* stereoModeParam;
    juce::AudioParameterBool* freezeParam;
    juce::AudioParameterBool* bypassParam;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCSpaceMakerProcessor)
};
