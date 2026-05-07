#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Construction / Destruction
//==============================================================================
VCStereoProcessor::VCStereoProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::stereo())
                     .withOutput("Output", AudioChannelSet::stereo()))
    , mAPVTS(*this, nullptr, Identifier("VCStereoParameters"),
             createParameterLayout())
{
    // Register parameter listeners
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
    mAPVTS.addParameterListener(ParameterIDs::width, this);
    mAPVTS.addParameterListener(ParameterIDs::pan, this);
    mAPVTS.addParameterListener(ParameterIDs::monoBass, this);
    mAPVTS.addParameterListener(ParameterIDs::bassFreq, this);
}

VCStereoProcessor::~VCStereoProcessor()
{
}

//==============================================================================
// Parameter Layout
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout
VCStereoProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Bypass
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    // Width (0 ~ 200 %)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::width, "Width",
        NormalisableRange<float>(0.0f, 200.0f), 100.0f, "%"));

    // Pan (-100 ~ 100)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::pan, "Pan",
        NormalisableRange<float>(-100.0f, 100.0f), 0.0f));

    // Mono Bass
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::monoBass, "Mono Bass", false));

    // Bass Frequency (50 ~ 300 Hz)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::bassFreq, "Bass Freq",
        NormalisableRange<float>(50.0f, 300.0f), 150.0f, "Hz"));

    return {params.begin(), params.end()};
}

//==============================================================================
// Prepare to Play
//==============================================================================
void VCStereoProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = static_cast<uint32>(samplesPerBlock);
    mProcessSpec.numChannels = getMainBusNumOutputChannels();

    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VCStereoProcessor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// Bus Layout Support Check
//==============================================================================
bool VCStereoProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto inputLayout = layouts.inputBuses[0];
    const auto outputLayout = layouts.outputBuses[0];

    return inputLayout == AudioChannelSet::stereo() &&
           outputLayout == AudioChannelSet::stereo();
}

//==============================================================================
// Process Audio Block
//==============================================================================
void VCStereoProcessor::processBlock(AudioBuffer<float>& buffer,
                                      MidiBuffer&)
{
    if (mBypass)
        return;

    // Get audio data pointers
    int numSamples = buffer.getNumSamples();
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getWritePointer(1);

    // Process through DSP
    mDSP.process(leftChannel, rightChannel, numSamples);
}

//==============================================================================
// Parameter Changed Callback
//==============================================================================
void VCStereoProcessor::parameterChanged(const String& parameterID,
                                          float newValue)
{
    VCPluginDSP::Params p = mDSP.getParams();

    if (parameterID == ParameterIDs::bypass) {
        mBypass = newValue > 0.5f;
        mDSP.setEnabled(!mBypass);
    } else if (parameterID == ParameterIDs::width) {
        p.width = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::pan) {
        p.pan = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::monoBass) {
        p.monoBass = newValue > 0.5f;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::bassFreq) {
        p.bassFreq = newValue;
        mDSP.setParams(p);
    }
}

//==============================================================================
// State Information
//==============================================================================
void VCStereoProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        MemoryOutputStream mos(destData, true);
        xml->writeTo(mos, {});
    }
}

void VCStereoProcessor::setStateInformation(const void* data,
                                              int sizeInBytes)
{
    auto xmlState = parseXML(String(static_cast<const char*>(data), sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Create Editor
//==============================================================================
AudioProcessorEditor* VCStereoProcessor::createEditor()
{
    return new VCStereoEditor(*this);
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCStereoProcessor();
}
