#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Construction / Destruction
//==============================================================================
VCDeEsserProcessor::VCDeEsserProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::stereo())
                     .withOutput("Output", AudioChannelSet::stereo()))
    , mAPVTS(*this, nullptr, Identifier("VCDeEsserParameters"),
             createParameterLayout())
{
    // Register parameter listeners
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
    mAPVTS.addParameterListener(ParameterIDs::threshold, this);
    mAPVTS.addParameterListener(ParameterIDs::frequency, this);
    mAPVTS.addParameterListener(ParameterIDs::reduction, this);
    mAPVTS.addParameterListener(ParameterIDs::mix, this);
}

VCDeEsserProcessor::~VCDeEsserProcessor()
{
}

//==============================================================================
// Parameter Layout
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout
VCDeEsserProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Bypass
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    // Threshold
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::threshold, "Threshold",
        NormalisableRange<float>(-40.0f, 0.0f), -20.0f, "dB"));

    // Frequency
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::frequency, "Frequency",
        NormalisableRange<float>(2000.0f, 12000.0f), 6000.0f, "Hz"));

    // Reduction
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::reduction, "Reduction",
        NormalisableRange<float>(-30.0f, 0.0f), -10.0f, "dB"));

    // Mix
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::mix, "Mix",
        NormalisableRange<float>(0.0f, 100.0f), 100.0f, "%"));

    return {params.begin(), params.end()};
}

//==============================================================================
// Prepare to Play
//==============================================================================
void VCDeEsserProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    mProcessSpec.numChannels = getMainBusNumOutputChannels();

    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VCDeEsserProcessor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// Bus Layout Support Check
// IMPORTANT: In JUCE 8, layouts.inputBuses[] returns by value (not reference)
// Use const auto& or auto to capture, NOT auto&
//==============================================================================
bool VCDeEsserProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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
void VCDeEsserProcessor::processBlock(AudioBuffer<float>& buffer,
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
void VCDeEsserProcessor::parameterChanged(const String& parameterID,
                                           float newValue)
{
    if (parameterID == ParameterIDs::bypass) {
        mBypass = newValue > 0.5f;
        mDSP.setEnabled(!mBypass);
    } else if (parameterID == ParameterIDs::threshold) {
        mThreshold = newValue;
        VCDeEsserDSP::Params p = mDSP.getParams();
        p.threshold = mThreshold;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::frequency) {
        mFrequency = newValue;
        VCDeEsserDSP::Params p = mDSP.getParams();
        p.frequency = mFrequency;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::reduction) {
        mReduction = newValue;
        VCDeEsserDSP::Params p = mDSP.getParams();
        p.reduction = mReduction;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::mix) {
        mMix = newValue;
        VCDeEsserDSP::Params p = mDSP.getParams();
        p.mix = mMix;
        mDSP.setParams(p);
    }
}

//==============================================================================
// State Information
//==============================================================================
void VCDeEsserProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        MemoryOutputStream mos(destData, true);
        xml->writeTo(mos, {});
    }
}

void VCDeEsserProcessor::setStateInformation(const void* data,
                                               int sizeInBytes)
{
    auto xmlState = parseXML(String(static_cast<const char*>(data), sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Create Editor
//==============================================================================
AudioProcessorEditor* VCDeEsserProcessor::createEditor()
{
    return new VCDeEsserEditor(*this);
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCDeEsserProcessor();
}
