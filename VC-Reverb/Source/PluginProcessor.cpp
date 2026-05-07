#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Construction / Destruction
//==============================================================================
VCReverbProcessor::VCReverbProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::stereo())
                     .withOutput("Output", AudioChannelSet::stereo()))
    , mAPVTS(*this, nullptr, Identifier("VCReverbParameters"),
             createParameterLayout())
{
    // Register parameter listeners
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
    mAPVTS.addParameterListener(ParameterIDs::room, this);
    mAPVTS.addParameterListener(ParameterIDs::decay, this);
    mAPVTS.addParameterListener(ParameterIDs::damping, this);
    mAPVTS.addParameterListener(ParameterIDs::preDelay, this);
    mAPVTS.addParameterListener(ParameterIDs::mix, this);
}

VCReverbProcessor::~VCReverbProcessor()
{
}

//==============================================================================
// Parameter Layout
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout
VCReverbProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Bypass
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    // Room Size
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::room, "Room Size",
        NormalisableRange<float>(0.0f, 100.0f), 50.0f, "%"));

    // Decay
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::decay, "Decay",
        NormalisableRange<float>(0.0f, 100.0f), 50.0f, "%"));

    // Damping
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::damping, "Damping",
        NormalisableRange<float>(0.0f, 100.0f), 50.0f, "%"));

    // Pre-Delay
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::preDelay, "Pre-Delay",
        NormalisableRange<float>(0.0f, 100.0f), 20.0f, "ms"));

    // Mix
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::mix, "Mix",
        NormalisableRange<float>(0.0f, 100.0f), 30.0f, "%"));

    return {params.begin(), params.end()};
}

//==============================================================================
// Prepare to Play
//==============================================================================
void VCReverbProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    mProcessSpec.numChannels = getMainBusNumOutputChannels();

    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VCReverbProcessor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// Bus Layout Support Check
// IMPORTANT: In JUCE 8, layouts.inputBuses[] returns by value (not reference)
// Use const auto& or auto to capture, NOT auto&
//==============================================================================
bool VCReverbProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Only support stereo layout
    // Note: layouts.inputBuses[0] returns by value in JUCE 8
    const auto inputLayout = layouts.inputBuses[0];
    const auto outputLayout = layouts.outputBuses[0];

    return inputLayout == AudioChannelSet::stereo() &&
           outputLayout == AudioChannelSet::stereo();
}

//==============================================================================
// Process Audio Block
//==============================================================================
void VCReverbProcessor::processBlock(AudioBuffer<float>& buffer,
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
void VCReverbProcessor::parameterChanged(const String& parameterID,
                                          float newValue)
{
    if (parameterID == ParameterIDs::bypass) {
        mBypass = newValue > 0.5f;
        mDSP.setEnabled(!mBypass);
    } else if (parameterID == ParameterIDs::room) {
        mRoomSize = newValue;
        VCPluginDSP::Params p = mDSP.getParams();
        p.roomSize = mRoomSize;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::decay) {
        mDecay = newValue;
        VCPluginDSP::Params p = mDSP.getParams();
        p.decay = mDecay;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::damping) {
        mDamping = newValue;
        VCPluginDSP::Params p = mDSP.getParams();
        p.damping = mDamping;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::preDelay) {
        mPreDelay = newValue;
        VCPluginDSP::Params p = mDSP.getParams();
        p.preDelay = mPreDelay;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::mix) {
        mMix = newValue;
        VCPluginDSP::Params p = mDSP.getParams();
        p.mix = mMix;
        mDSP.setParams(p);
    }
}

//==============================================================================
// State Information
//==============================================================================
void VCReverbProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        MemoryOutputStream mos(destData, true);
        xml->writeTo(mos, {});
    }
}

void VCReverbProcessor::setStateInformation(const void* data,
                                              int sizeInBytes)
{
    auto xmlState = parseXML(String(static_cast<const char*>(data), sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Create Editor
//==============================================================================
AudioProcessorEditor* VCReverbProcessor::createEditor()
{
    return new VCReverbEditor(*this);
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCReverbProcessor();
}
