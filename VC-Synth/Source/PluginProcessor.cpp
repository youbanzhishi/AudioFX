#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Construction / Destruction
//==============================================================================

VCSynthProcessor::VCSynthProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      mAPVTS(*this, nullptr, "Parameters", createParameterLayout())
{
    mAPVTS.addParameterListener(ParameterIDs::oscType, this);
    mAPVTS.addParameterListener(ParameterIDs::unison, this);
    mAPVTS.addParameterListener(ParameterIDs::detune, this);
    mAPVTS.addParameterListener(ParameterIDs::cutoff, this);
    mAPVTS.addParameterListener(ParameterIDs::resonance, this);
    mAPVTS.addParameterListener(ParameterIDs::filterType, this);
    mAPVTS.addParameterListener(ParameterIDs::attack, this);
    mAPVTS.addParameterListener(ParameterIDs::decay, this);
    mAPVTS.addParameterListener(ParameterIDs::sustain, this);
    mAPVTS.addParameterListener(ParameterIDs::release, this);
    mAPVTS.addParameterListener(ParameterIDs::reverbMix, this);
    mAPVTS.addParameterListener(ParameterIDs::delayMix, this);
    mAPVTS.addParameterListener(ParameterIDs::delayTime, this);
    mAPVTS.addParameterListener(ParameterIDs::volume, this);
}

VCSynthProcessor::~VCSynthProcessor()
{
}

//==============================================================================
// Prepare / Release
//==============================================================================

void VCSynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    mProcessSpec.numChannels = 2;

    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VCSynthProcessor::releaseResources()
{
}

bool VCSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // VSTi: output only (stereo)
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // No input required
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    return true;
}

//==============================================================================
// Process Block — VSTi with MIDI input
//==============================================================================

void VCSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midiBuffer)
{
    juce::ScopedNoDenormals noDenormals;

    // Process MIDI messages
    for (const auto metadata : midiBuffer) {
        auto message = metadata.getMessage();
        if (message.isNoteOn()) {
            mDSP.noteOn(message.getNoteNumber(),
                        message.getVelocity() / 127.0f);
        } else if (message.isNoteOff()) {
            mDSP.noteOff(message.getNoteNumber());
        }
        // Could handle CC, pitch bend, etc. here
    }

    // Render audio
    auto totalNumSamples = buffer.getNumSamples();

    if (totalNumSamples == 0)
        return;

    // Create temp buffers for DSP
    std::vector<float> left(totalNumSamples, 0.0f);
    std::vector<float> right(totalNumSamples, 0.0f);

    mDSP.render(left.data(), right.data(), totalNumSamples);

    // Copy to output buffer
    auto* outL = buffer.getWritePointer(0);
    auto* outR = buffer.getWritePointer(1);

    for (int i = 0; i < totalNumSamples; ++i) {
        outL[i] = left[i];
        outR[i] = right[i];
    }
}

//==============================================================================
// Editor
//==============================================================================

juce::AudioProcessorEditor* VCSynthProcessor::createEditor()
{
    return new VCSynthEditor(*this);
}

//==============================================================================
// Program Support
//==============================================================================

int VCSynthProcessor::getNumPrograms()
{
    return VCPluginDSP::getNumPresets();
}

void VCSynthProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= VCPluginDSP::getNumPresets()) return;
    mCurrentProgram = index;

    VCPluginDSP::Params p;
    if (VCPluginDSP::getPreset(index, p)) {
        mDSP.setParams(p);
    }
}

const juce::String VCSynthProcessor::getProgramName(int index)
{
    const char* name = VCPluginDSP::getPresetName(index);
    return name ? juce::String(name) : juce::String();
}

//==============================================================================
// State Save/Restore
//==============================================================================

void VCSynthProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VCSynthProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr) {
        auto state = juce::ValueTree::fromXml(*xml);
        mAPVTS.replaceState(state);
    }
}

//==============================================================================
// Parameter Changed Callback
//==============================================================================

void VCSynthProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    VCPluginDSP::Params p = mDSP.getParams();

    if (parameterID == ParameterIDs::oscType)
        p.oscType = static_cast<VCOscType>((int)newValue);
    else if (parameterID == ParameterIDs::unison)
        p.unison = (int)newValue;
    else if (parameterID == ParameterIDs::detune)
        p.detune = newValue;
    else if (parameterID == ParameterIDs::cutoff)
        p.cutoff = newValue;
    else if (parameterID == ParameterIDs::resonance)
        p.resonance = newValue;
    else if (parameterID == ParameterIDs::filterType)
        p.filterType = static_cast<VCFilterType>((int)newValue);
    else if (parameterID == ParameterIDs::attack)
        p.attack = newValue;
    else if (parameterID == ParameterIDs::decay)
        p.decay = newValue;
    else if (parameterID == ParameterIDs::sustain)
        p.sustain = newValue;
    else if (parameterID == ParameterIDs::release)
        p.release = newValue;
    else if (parameterID == ParameterIDs::reverbMix)
        p.reverbMix = newValue;
    else if (parameterID == ParameterIDs::delayMix)
        p.delayMix = newValue;
    else if (parameterID == ParameterIDs::delayTime)
        p.delayTime = newValue;
    else if (parameterID == ParameterIDs::volume)
        p.volumeDB = newValue;
    else
        return;

    mDSP.setParams(p);
}

//==============================================================================
// Create Parameter Layout
//==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout VCSynthProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Oscillator
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterIDs::oscType, "Oscillator Type",
        juce::StringArray{"Sine", "Saw", "Square", "Triangle", "Noise"}, 1));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParameterIDs::unison, "Unison", 1, 7, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::detune, "Detune",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 10.0f));

    // Filter
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::cutoff, "Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.5f), 8000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::resonance, "Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterIDs::filterType, "Filter Type",
        juce::StringArray{"LP", "BP", "HP"}, 0));

    // Envelope
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::attack, "Attack",
        juce::NormalisableRange<float>(0.1f, 2000.0f, 0.1f, 0.5f), 10.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::decay, "Decay",
        juce::NormalisableRange<float>(0.1f, 2000.0f, 0.1f, 0.5f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::sustain, "Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.7f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::release, "Release",
        juce::NormalisableRange<float>(0.1f, 5000.0f, 0.1f, 0.5f), 200.0f));

    // Effects
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::reverbMix, "Reverb Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::delayMix, "Delay Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::delayTime, "Delay Time",
        juce::NormalisableRange<float>(1.0f, 2000.0f, 1.0f), 375.0f));

    // Output
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::volume, "Volume",
        juce::NormalisableRange<float>(-60.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCSynthProcessor();
}
