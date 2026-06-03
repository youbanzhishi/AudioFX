// ============================================================================
// PluginProcessor.cpp - VC-SpaceMaker JUCE Plugin Processor
// Dynamic Frequency Avoidance - 对标 TrackSpacer
// ============================================================================

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/VCSpaceMakerDSP.h"

//==============================================================================
class VCSpaceMakerProcessor : public juce::AudioProcessor
{
public:
    VCSpaceMakerProcessor()
        : AudioProcessor (juce::AudioProcessor::BusesProperties()
            .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
            .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)
            .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
        // 参数：Amount (0-1)
        addParameter (amountParam = new juce::AudioParameterFloat (
            "amount", "Amount",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

        // 参数：Attack (0.1-200ms)
        addParameter (attackParam = new juce::AudioParameterFloat (
            "attack", "Attack (ms)",
            juce::NormalisableRange<float> (0.1f, 200.0f, 0.1f), 5.0f));

        // 参数：Release (1-2000ms)
        addParameter (releaseParam = new juce::AudioParameterFloat (
            "release", "Release (ms)",
            juce::NormalisableRange<float> (1.0f, 2000.0f, 1.0f), 50.0f));

        // 参数：Low-Cut (20-2000Hz)
        addParameter (lowCutParam = new juce::AudioParameterFloat (
            "lowcut", "Low Cut (Hz)",
            juce::NormalisableRange<float> (20.0f, 2000.0f, 1.0f), 20.0f));

        // 参数：High-Cut (2000-20000Hz)
        addParameter (highCutParam = new juce::AudioParameterFloat (
            "highcut", "High Cut (Hz)",
            juce::NormalisableRange<float> (2000.0f, 20000.0f, 1.0f), 20000.0f));

        // 参数：Stereo Mode (0=Stereo, 1=Mid, 2=Side, 3=MidSide)
        addParameter (stereoModeParam = new juce::AudioParameterChoice (
            "stereomode", "Stereo Mode",
            juce::StringArray {"Stereo", "Mid", "Side", "Mid/Side"}, 0));

        // 参数：Freeze
        addParameter (freezeParam = new juce::AudioParameterBool (
            "freeze", "Freeze", false));

        // 参数：Bypass
        addParameter (bypassParam = new juce::AudioParameterBool (
            "bypass", "Bypass", false));
    }

    ~VCSpaceMakerProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        dsp.prepareToPlay (sampleRate, samplesPerBlock);
    }

    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        // Need at least stereo input + sidechain
        if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
            return false;

        if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
            return false;

        // Sidechain can be mono or stereo
        if (layouts.inputBuses.size() > 1)
        {
            if (layouts.inputBuses[1] != juce::AudioChannelSet::mono() &&
                layouts.inputBuses[1] != juce::AudioChannelSet::stereo())
                return false;
        }

        return true;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;

        // 更新参数
        VCSpaceMakerDSP::Parameters params;
        params.amount = amountParam->get();
        params.attackMs = attackParam->get();
        params.releaseMs = releaseParam->get();
        params.lowCutFreq = lowCutParam->get();
        params.highCutFreq = highCutParam->get();
        params.stereoMode = static_cast<VCSpaceMakerDSP::StereoMode>(stereoModeParam->getIndex());
        params.freeze = freezeParam->get();
        params.bypass = bypassParam->get();
        dsp.setParameters (params);

        auto mainBuffer = getBusBuffer (buffer, true, 0);

        // 获取侧链输入
        float scL[4096], scR[4096];
        int numSamples = buffer.getNumSamples();

        if (buffer.getNumChannels() > 2)
        {
            // 有侧链输入
            auto scBuffer = getBusBuffer (buffer, true, 1);
            if (scBuffer.getNumChannels() >= 2)
            {
                for (int i = 0; i < numSamples; i++)
                {
                    scL[i] = scBuffer.getSample (0, i);
                    scR[i] = scBuffer.getSample (1, i);
                }
            }
            else
            {
                for (int i = 0; i < numSamples; i++)
                {
                    scL[i] = scBuffer.getSample (0, i);
                    scR[i] = scL[i];
                }
            }
        }
        else
        {
            // 无外部侧链，使用内部侧链（主信号）
            for (int i = 0; i < numSamples; i++)
            {
                scL[i] = mainBuffer.getSample (0, i);
                scR[i] = mainBuffer.getSample (1, i);
            }
        }

        // 处理
        float* mainL = mainBuffer.getWritePointer (0);
        float* mainR = mainBuffer.getWritePointer (1);
        dsp.processBlock (mainL, mainR, scL, scR, numSamples);
    }

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

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        juce::XmlElement xml ("VC-SpaceMakerState");
        xml.setAttribute ("amount", (double) amountParam->get());
        xml.setAttribute ("attack", (double) attackParam->get());
        xml.setAttribute ("release", (double) releaseParam->get());
        xml.setAttribute ("lowcut", (double) lowCutParam->get());
        xml.setAttribute ("highcut", (double) highCutParam->get());
        xml.setAttribute ("stereomode", stereoModeParam->getIndex());
        xml.setAttribute ("freeze", freezeParam->get());
        copyXmlToBinary (xml, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        auto xml = getXmlFromBinary (data, sizeInBytes);
        if (xml != nullptr)
        {
            if (xml->hasAttribute ("amount"))
                *amountParam = (float) xml->getDoubleAttribute ("amount", 0.5);
            if (xml->hasAttribute ("attack"))
                *attackParam = (float) xml->getDoubleAttribute ("attack", 5.0);
            if (xml->hasAttribute ("release"))
                *releaseParam = (float) xml->getDoubleAttribute ("release", 50.0);
            if (xml->hasAttribute ("lowcut"))
                *lowCutParam = (float) xml->getDoubleAttribute ("lowcut", 20.0);
            if (xml->hasAttribute ("highcut"))
                *highCutParam = (float) xml->getDoubleAttribute ("highcut", 20000.0);
            if (xml->hasAttribute ("stereomode"))
                *stereoModeParam = xml->getIntAttribute ("stereomode", 0);
            if (xml->hasAttribute ("freeze"))
                *freezeParam = xml->getBoolAttribute ("freeze", false);
        }
    }

    // 获取频谱数据（给编辑器使用）
    const VCSpaceMakerDSP::SpectrumData& getSpectrumData() const { return dsp.getSpectrumData(); }
    const float* getBandFrequencies() const { return dsp.getBandFrequencies(); }

private:
    VCSpaceMakerDSP dsp;

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

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCSpaceMakerProcessor();
}
