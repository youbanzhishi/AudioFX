// ============================================================================
// PluginProcessor.cpp - VC-SpaceMaker JUCE Plugin Processor
// Dynamic Frequency Avoidance - 对标 TrackSpacer
// ============================================================================

#include "PluginProcessor.h"
#include "PluginEditor.cpp"

//==============================================================================
VCSpaceMakerProcessor::VCSpaceMakerProcessor()
    : AudioProcessor (juce::AudioProcessor::BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      vts (*this, nullptr, "Parameters", createParameterLayout())
{
    amountParam    = dynamic_cast<juce::AudioParameterFloat*>(vts.getParameter ("amount"));
    attackParam    = dynamic_cast<juce::AudioParameterFloat*>(vts.getParameter ("attack"));
    releaseParam   = dynamic_cast<juce::AudioParameterFloat*>(vts.getParameter ("release"));
    lowCutParam    = dynamic_cast<juce::AudioParameterFloat*>(vts.getParameter ("lowcut"));
    highCutParam   = dynamic_cast<juce::AudioParameterFloat*>(vts.getParameter ("highcut"));
    stereoModeParam = dynamic_cast<juce::AudioParameterChoice*>(vts.getParameter ("stereomode"));
    freezeParam    = dynamic_cast<juce::AudioParameterBool*>(vts.getParameter ("freeze"));
    bypassParam    = dynamic_cast<juce::AudioParameterBool*>(vts.getParameter ("bypass"));
}

VCSpaceMakerProcessor::~VCSpaceMakerProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VCSpaceMakerProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "amount", "Amount", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "attack", "Attack (ms)", juce::NormalisableRange<float> (0.1f, 200.0f, 0.1f), 5.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "release", "Release (ms)", juce::NormalisableRange<float> (1.0f, 2000.0f, 1.0f), 50.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "lowcut", "Low Cut (Hz)", juce::NormalisableRange<float> (20.0f, 2000.0f, 1.0f), 20.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "highcut", "High Cut (Hz)", juce::NormalisableRange<float> (2000.0f, 20000.0f, 1.0f), 20000.0f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "stereomode", "Stereo Mode", juce::StringArray {"Stereo", "Mid", "Side", "Mid/Side"}, 0));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "freeze", "Freeze", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "bypass", "Bypass", false));

    return { params.begin(), params.end() };
}

//==============================================================================
void VCSpaceMakerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dsp.prepareToPlay (sampleRate, samplesPerBlock);
}

void VCSpaceMakerProcessor::releaseResources() {}

bool VCSpaceMakerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        if (layouts.inputBuses[1] != juce::AudioChannelSet::mono() &&
            layouts.inputBuses[1] != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void VCSpaceMakerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

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

    float scL[4096], scR[4096];
    int numSamples = buffer.getNumSamples();

    if (buffer.getNumChannels() > 2)
    {
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
        for (int i = 0; i < numSamples; i++)
        {
            scL[i] = mainBuffer.getSample (0, i);
            scR[i] = mainBuffer.getSample (1, i);
        }
    }

    float* mainL = mainBuffer.getWritePointer (0);
    float* mainR = mainBuffer.getWritePointer (1);
    dsp.processBlock (mainL, mainR, scL, scR, numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* VCSpaceMakerProcessor::createEditor()
{
    return new VCSpaceMakerEditor (*this);
}

void VCSpaceMakerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = vts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void VCSpaceMakerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml != nullptr)
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
            vts.replaceState (state);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCSpaceMakerProcessor();
}
