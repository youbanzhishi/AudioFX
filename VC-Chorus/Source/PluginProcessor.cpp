#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Construction / Destruction
//==============================================================================
VC_ChorusProcessor::VC_ChorusProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::stereo())
                     .withOutput("Output", AudioChannelSet::stereo()))
    , mAPVTS(*this, nullptr, Identifier("VC-ChorusParameters"),
             createParameterLayout())
{
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
    mAPVTS.addParameterListener(ParameterIDs::rate, this);
    mAPVTS.addParameterListener(ParameterIDs::depth, this);
    mAPVTS.addParameterListener(ParameterIDs::voices, this);
    mAPVTS.addParameterListener(ParameterIDs::mix, this);
    mAPVTS.addParameterListener(ParameterIDs::delay, this);
    mAPVTS.addParameterListener(ParameterIDs::width, this);
    mAPVTS.addParameterListener(ParameterIDs::feedback, this);
}

VC_ChorusProcessor::~VC_ChorusProcessor()
{
}

//==============================================================================
// Parameter Layout
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout
VC_ChorusProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::rate, "Rate",
        NormalisableRange<float>(0.1f, 10.0f, 0.01f), 1.5f, "Hz"));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::depth, "Depth",
        NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f, "%"));

    params.push_back(std::make_unique<AudioParameterInt>(
        ParameterIDs::voices, "Voices", 1, 4, 2));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::mix, "Mix",
        NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f, "%"));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::delay, "Delay",
        NormalisableRange<float>(5.0f, 30.0f, 0.1f), 15.0f, "ms"));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::width, "Width",
        NormalisableRange<float>(0.0f, 100.0f, 0.1f), 80.0f, "%"));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::feedback, "Feedback",
        NormalisableRange<float>(0.0f, 50.0f, 0.1f), 20.0f, "%"));

    return {params.begin(), params.end()};
}

//==============================================================================
// Prepare to Play
//==============================================================================
void VC_ChorusProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    mProcessSpec.numChannels = getMainBusNumOutputChannels();

    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VC_ChorusProcessor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// Bus Layout Support Check
// IMPORTANT: In JUCE 8, layouts.inputBuses[] returns by value (not reference)
//==============================================================================
bool VC_ChorusProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto inputLayout = layouts.inputBuses[0];
    const auto outputLayout = layouts.outputBuses[0];

    return inputLayout == AudioChannelSet::stereo() &&
           outputLayout == AudioChannelSet::stereo();
}

//==============================================================================
// Process Audio Block
//==============================================================================
void VC_ChorusProcessor::processBlock(AudioBuffer<float>& buffer,
                                       MidiBuffer&)
{
    if (mBypass)
        return;

    int numSamples = buffer.getNumSamples();
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getWritePointer(1);

    mDSP.process(leftChannel, rightChannel, numSamples);
}

//==============================================================================
// Parameter Changed Callback
//==============================================================================
void VC_ChorusProcessor::parameterChanged(const String& parameterID,
                                           float newValue)
{
    VCPluginDSP::Params p = mDSP.getParams();

    if (parameterID == ParameterIDs::bypass) {
        mBypass = newValue > 0.5f;
        mDSP.setEnabled(!mBypass);
    } else if (parameterID == ParameterIDs::rate) {
        p.rate = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::depth) {
        p.depth = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::voices) {
        p.voices = static_cast<int>(newValue);
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::mix) {
        p.mix = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::delay) {
        p.delay = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::width) {
        p.width = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::feedback) {
        p.feedback = newValue;
        mDSP.setParams(p);
    }
}

//==============================================================================
// State Information
//==============================================================================
void VC_ChorusProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        MemoryOutputStream mos(destData, true);
        xml->writeTo(mos, {});
    }
}

void VC_ChorusProcessor::setStateInformation(const void* data,
                                              int sizeInBytes)
{
    auto xmlState = parseXML(String(static_cast<const char*>(data), sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Create Editor
//==============================================================================
AudioProcessorEditor* VC_ChorusProcessor::createEditor()
{
    return new VC_ChorusEditor(*this);
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VC_ChorusProcessor();
}
