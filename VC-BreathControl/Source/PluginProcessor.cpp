#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/VCPluginDSP.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    mDSP = std::make_unique<VCPluginDSP>();
}

PluginProcessor::~PluginProcessor()
{
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mDSP->prepare(sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);
    int numSamples = buffer.getNumSamples();

    mDSP->process(left, right, numSamples);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto params = mDSP->getParams();
    juce::MemoryOutputStream stream(destData, true);
    stream.writeFloat(params.threshold);
    stream.writeFloat(params.reduction);
    stream.writeFloat(params.attack);
    stream.writeFloat(params.release);
    stream.writeBool(params.autoSmooth);
    stream.writeFloat(params.fadeIn);
    stream.writeFloat(params.fadeOut);
    stream.writeFloat(params.minBreathDuration);
    stream.writeFloat(params.sensitivity);
    stream.writeFloat(params.lookahead);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    VCPluginDSP::Params params;
    params.threshold = stream.readFloat();
    params.reduction = stream.readFloat();
    params.attack = stream.readFloat();
    params.release = stream.readFloat();
    params.autoSmooth = stream.readBool();
    params.fadeIn = stream.readFloat();
    params.fadeOut = stream.readFloat();
    params.minBreathDuration = stream.readFloat();
    params.sensitivity = stream.readFloat();
    params.lookahead = stream.readFloat();
    mDSP->setParams(params);
}

//==============================================================================
// Plugin Entry Point
//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
