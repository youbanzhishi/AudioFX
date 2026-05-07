#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Construction / Destruction
//==============================================================================
VCDynamicEQProcessor::VCDynamicEQProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::stereo())
                     .withOutput("Output", AudioChannelSet::stereo()))
    , mAPVTS(*this, nullptr, Identifier("VC-DynamicEQParameters"),
             createParameterLayout())
{
    // Register parameter listeners
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
    mAPVTS.addParameterListener(ParameterIDs::frequency, this);
    mAPVTS.addParameterListener(ParameterIDs::gain, this);
    mAPVTS.addParameterListener(ParameterIDs::q, this);
    mAPVTS.addParameterListener(ParameterIDs::threshold, this);
    mAPVTS.addParameterListener(ParameterIDs::range, this);
    mAPVTS.addParameterListener(ParameterIDs::attack, this);
    mAPVTS.addParameterListener(ParameterIDs::release, this);
    mAPVTS.addParameterListener(ParameterIDs::mix, this);
    
    mEnvelope[0] = mEnvelope[1] = 0.0f;
}

VCDynamicEQProcessor::~VCDynamicEQProcessor()
{
}

//==============================================================================
// Parameter Layout
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout
VCDynamicEQProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Bypass
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    // Frequency - logarithmic scale (20~20000 Hz)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::frequency, "Frequency",
        NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.5f), 200.0f, "Hz"));

    // Gain - static EQ gain (-18~+18 dB)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::gain, "Gain",
        NormalisableRange<float>(-18.0f, 18.0f), -6.0f, "dB"));

    // Q value (0.1~10)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::q, "Q",
        NormalisableRange<float>(0.1f, 10.0f), 1.0f));

    // Threshold - dynamic threshold (-48~0 dB)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::threshold, "Threshold",
        NormalisableRange<float>(-48.0f, 0.0f), -12.0f, "dB"));

    // Range - dynamic range (-24~+24 dB)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::range, "Range",
        NormalisableRange<float>(-24.0f, 24.0f), -12.0f, "dB"));

    // Attack time (0.1~50 ms)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::attack, "Attack",
        NormalisableRange<float>(0.1f, 50.0f), 10.0f, "ms"));

    // Release time (10~500 ms)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::release, "Release",
        NormalisableRange<float>(10.0f, 500.0f), 100.0f, "ms"));

    // Mix (0~100%)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::mix, "Mix",
        NormalisableRange<float>(0.0f, 100.0f), 100.0f, "%"));

    return {params.begin(), params.end()};
}

//==============================================================================
// Prepare to Play
//==============================================================================
void VCDynamicEQProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    mProcessSpec.numChannels = getMainBusNumOutputChannels();

    mDSP.prepare(sampleRate, samplesPerBlock);
    mEnvelope[0] = mEnvelope[1] = 0.0f;
}

void VCDynamicEQProcessor::releaseResources()
{
    mDSP.reset();
    mEnvelope[0] = mEnvelope[1] = 0.0f;
}

//==============================================================================
// Bus Layout Support Check
// IMPORTANT: In JUCE 8, layouts.inputBuses[] returns by value (not reference)
// Use const auto& or auto to capture, NOT auto&
//==============================================================================
bool VCDynamicEQProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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
void VCDynamicEQProcessor::processBlock(AudioBuffer<float>& buffer,
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
void VCDynamicEQProcessor::parameterChanged(const String& parameterID,
                                                  float newValue)
{
    VCPluginDSP::Params p = mDSP.getParams();
    
    if (parameterID == ParameterIDs::bypass) {
        mBypass = newValue > 0.5f;
        mDSP.setEnabled(!mBypass);
    } else if (parameterID == ParameterIDs::frequency) {
        p.frequency = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::gain) {
        p.gain = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::q) {
        p.q = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::threshold) {
        p.threshold = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::range) {
        p.range = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::attack) {
        p.attack = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::release) {
        p.release = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::mix) {
        p.mix = newValue;
        mDSP.setParams(p);
    }
}

//==============================================================================
// State Information
//==============================================================================
void VCDynamicEQProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        MemoryOutputStream mos(destData, true);
        xml->writeTo(mos, {});
    }
}

void VCDynamicEQProcessor::setStateInformation(const void* data,
                                                      int sizeInBytes)
{
    auto xmlState = parseXML(String(static_cast<const char*>(data), sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Create Editor
//==============================================================================
AudioProcessorEditor* VCDynamicEQProcessor::createEditor()
{
    return new VCDynamicEQEditor(*this);
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCDynamicEQProcessor();
}
