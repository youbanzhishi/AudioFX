#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Construction / Destruction
//==============================================================================
VC_DelayProcessor::VC_DelayProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::stereo())
                     .withOutput("Output", AudioChannelSet::stereo()))
    , mAPVTS(*this, nullptr, Identifier("VC-DelayParameters"),
             createParameterLayout())
{
    // Register parameter listeners
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
    mAPVTS.addParameterListener(ParameterIDs::delayTime, this);
    mAPVTS.addParameterListener(ParameterIDs::feedback, this);
    mAPVTS.addParameterListener(ParameterIDs::mix, this);
}

VC_DelayProcessor::~VC_DelayProcessor()
{
}

//==============================================================================
// Parameter Layout
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout
VC_DelayProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Bypass
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    // Delay Time
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::delayTime, "Delay Time",
        NormalisableRange<float>(10.0f, 2000.0f, 1.0f), 250.0f, "ms"));

    // Feedback
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::feedback, "Feedback",
        NormalisableRange<float>(0.0f, 90.0f, 0.1f), 30.0f, "%"));

    // Mix
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::mix, "Mix",
        NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f, "%"));

    return {params.begin(), params.end()};
}

//==============================================================================
// Prepare to Play
//==============================================================================
void VC_DelayProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    mProcessSpec.numChannels = getMainBusNumOutputChannels();

    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VC_DelayProcessor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// Bus Layout Support Check
// IMPORTANT: In JUCE 8, layouts.inputBuses[] returns by value (not reference)
// Use const auto or auto to capture, NOT auto&
//==============================================================================
bool VC_DelayProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Only support stereo layout
    const auto inputLayout = layouts.inputBuses[0];
    const auto outputLayout = layouts.outputBuses[0];

    return inputLayout == AudioChannelSet::stereo() &&
           outputLayout == AudioChannelSet::stereo();
}

//==============================================================================
// Process Audio Block
//==============================================================================
void VC_DelayProcessor::processBlock(AudioBuffer<float>& buffer,
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
void VC_DelayProcessor::parameterChanged(const String& parameterID,
                                         float newValue)
{
    VCPluginDSP::Params p = mDSP.getParams();

    if (parameterID == ParameterIDs::bypass) {
        mBypass = newValue > 0.5f;
        mDSP.setEnabled(!mBypass);
    } else if (parameterID == ParameterIDs::delayTime) {
        p.delayTime = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::feedback) {
        p.feedback = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::mix) {
        p.mix = newValue;
        mDSP.setParams(p);
    }
}

//==============================================================================
// State Information
//==============================================================================
void VC_DelayProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        MemoryOutputStream mos(destData, true);
        xml->writeTo(mos, {});
    }
}

void VC_DelayProcessor::setStateInformation(const void* data,
                                            int sizeInBytes)
{
    auto xmlState = parseXML(String(static_cast<const char*>(data), sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Create Editor
//==============================================================================
AudioProcessorEditor* VC_DelayProcessor::createEditor()
{
    return new VC_DelayEditor(*this);
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VC_DelayProcessor();
}
