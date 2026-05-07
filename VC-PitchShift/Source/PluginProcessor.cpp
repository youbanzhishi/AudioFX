#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Construction / Destruction
//==============================================================================
VC_PitchShiftProcessor::VC_PitchShiftProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::stereo())
                     .withOutput("Output", AudioChannelSet::stereo()))
    , mAPVTS(*this, nullptr, Identifier("VC-PitchShiftParameters"),
             createParameterLayout())
{
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
    mAPVTS.addParameterListener(ParameterIDs::threshold, this);
    mAPVTS.addParameterListener(ParameterIDs::ratio, this);
    mAPVTS.addParameterListener(ParameterIDs::attack, this);
    mAPVTS.addParameterListener(ParameterIDs::hold, this);
    mAPVTS.addParameterListener(ParameterIDs::release, this);
    mAPVTS.addParameterListener(ParameterIDs::range, this);
}

VC_PitchShiftProcessor::~VC_PitchShiftProcessor()
{
}

//==============================================================================
// Parameter Layout
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout
VC_PitchShiftProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::threshold, "Threshold",
        NormalisableRange<float>(-80.0f, 0.0f, 0.1f), -40.0f, "dB"));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::ratio, "Ratio",
        NormalisableRange<float>(1.0f, 20.0f, 0.1f), 10.0f, ":1"));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::attack, "Attack",
        NormalisableRange<float>(0.1f, 50.0f, 0.1f), 1.0f, "ms"));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::hold, "Hold",
        NormalisableRange<float>(0.0f, 500.0f, 1.0f), 50.0f, "ms"));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::release, "Release",
        NormalisableRange<float>(10.0f, 2000.0f, 1.0f), 100.0f, "ms"));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::range, "Range",
        NormalisableRange<float>(-80.0f, 0.0f, 0.1f), -80.0f, "dB"));

    return {params.begin(), params.end()};
}

//==============================================================================
// Prepare to Play
//==============================================================================
void VC_PitchShiftProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    mProcessSpec.numChannels = getMainBusNumOutputChannels();

    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VC_PitchShiftProcessor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// Bus Layout Support Check
// IMPORTANT: In JUCE 8, layouts.inputBuses[] returns by value (not reference)
//==============================================================================
bool VC_PitchShiftProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto inputLayout = layouts.inputBuses[0];
    const auto outputLayout = layouts.outputBuses[0];

    return inputLayout == AudioChannelSet::stereo() &&
           outputLayout == AudioChannelSet::stereo();
}

//==============================================================================
// Process Audio Block
//==============================================================================
void VC_PitchShiftProcessor::processBlock(AudioBuffer<float>& buffer,
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
void VC_PitchShiftProcessor::parameterChanged(const String& parameterID,
                                         float newValue)
{
    VCPluginDSP::Params p = mDSP.getParams();

    if (parameterID == ParameterIDs::bypass) {
        mBypass = newValue > 0.5f;
        mDSP.setEnabled(!mBypass);
    } else if (parameterID == ParameterIDs::threshold) {
        p.threshold = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::ratio) {
        p.ratio = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::attack) {
        p.attack = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::hold) {
        p.hold = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::release) {
        p.release = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::range) {
        p.range = newValue;
        mDSP.setParams(p);
    }
}

//==============================================================================
// State Information
//==============================================================================
void VC_PitchShiftProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        MemoryOutputStream mos(destData, true);
        xml->writeTo(mos, {});
    }
}

void VC_PitchShiftProcessor::setStateInformation(const void* data,
                                            int sizeInBytes)
{
    auto xmlState = parseXML(String(static_cast<const char*>(data), sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Create Editor
//==============================================================================
AudioProcessorEditor* VC_PitchShiftProcessor::createEditor()
{
    return new VC_PitchShiftEditor(*this);
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VC_PitchShiftProcessor();
}
