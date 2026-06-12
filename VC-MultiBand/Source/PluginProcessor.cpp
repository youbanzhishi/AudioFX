#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Construction / Destruction
//==============================================================================
__PLUGIN_NAME__Processor::__PLUGIN_NAME__Processor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::stereo())
                     .withOutput("Output", AudioChannelSet::stereo()))
    , mAPVTS(*this, nullptr, Identifier("__PLUGIN_NAME__Parameters"),
             createParameterLayout())
{
    // Register parameter listeners
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
    mAPVTS.addParameterListener(ParameterIDs::gain, this);
    mAPVTS.addParameterListener(ParameterIDs::mix, this);
}

__PLUGIN_NAME__Processor::~__PLUGIN_NAME__Processor()
{
}

//==============================================================================
// Parameter Layout
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout
__PLUGIN_NAME__Processor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Bypass
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    // Gain
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::gain, "Gain",
        NormalisableRange<float>(-24.0f, 24.0f), 0.0f, "dB"));

    // Mix
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::mix, "Mix",
        NormalisableRange<float>(0.0f, 100.0f), 100.0f, "%"));

    return {params.begin(), params.end()};
}

//==============================================================================
// Prepare to Play
//==============================================================================
void __PLUGIN_NAME__Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    mProcessSpec.numChannels = getMainBusNumOutputChannels();

    mDSP.prepare(sampleRate, samplesPerBlock);
}

void __PLUGIN_NAME__Processor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// Bus Layout Support Check
// IMPORTANT: In JUCE 8, layouts.inputBuses[] returns by value (not reference)
// Use const auto& or auto to capture, NOT auto&
//==============================================================================
bool __PLUGIN_NAME__Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
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
void __PLUGIN_NAME__Processor::processBlock(AudioBuffer<float>& buffer,
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
void __PLUGIN_NAME__Processor::parameterChanged(const String& parameterID,
                                                  float newValue)
{
    if (parameterID == ParameterIDs::bypass) {
        mBypass = newValue > 0.5f;
        mDSP.setEnabled(!mBypass);
    } else if (parameterID == ParameterIDs::gain) {
        mGainDB = newValue;
        VCPluginDSP::Params p = mDSP.getParams();
        // p.gainDB removed - MultiBand has per-band gain
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::mix) {
        mMix = newValue;
        VCPluginDSP::Params p = mDSP.getParams();
        mDSP.setParams(p);
    }
}

//==============================================================================
// State Information
//==============================================================================
void __PLUGIN_NAME__Processor::getStateInformation(MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        MemoryOutputStream mos(destData, true);
        xml->writeTo(mos, {});
    }
}

void __PLUGIN_NAME__Processor::setStateInformation(const void* data,
                                                      int sizeInBytes)
{
    auto xmlState = parseXML(String(static_cast<const char*>(data), sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Create Editor
//==============================================================================
AudioProcessorEditor* __PLUGIN_NAME__Processor::createEditor()
{
    return new __PLUGIN_NAME__Editor(*this);
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new __PLUGIN_NAME__Processor();
}
