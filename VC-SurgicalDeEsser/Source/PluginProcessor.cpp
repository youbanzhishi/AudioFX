#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Construction / Destruction
//==============================================================================
VCSurgicalDeEsserProcessor::VCSurgicalDeEsserProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::stereo())
                     .withOutput("Output", AudioChannelSet::stereo()))
    , mAPVTS(*this, nullptr, Identifier("VCSurgicalDeEsserParameters"),
             createParameterLayout())
{
    // Register parameter listeners
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
    mAPVTS.addParameterListener(ParameterIDs::threshold, this);
    mAPVTS.addParameterListener(ParameterIDs::reduction, this);
    mAPVTS.addParameterListener(ParameterIDs::freqLow, this);
    mAPVTS.addParameterListener(ParameterIDs::freqHigh, this);
    mAPVTS.addParameterListener(ParameterIDs::mode, this);
}

VCSurgicalDeEsserProcessor::~VCSurgicalDeEsserProcessor()
{
}

//==============================================================================
// Parameter Layout
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout
VCSurgicalDeEsserProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Bypass
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    // Threshold (-60~0 dBFS)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::threshold, "Threshold",
        NormalisableRange<float>(-60.0f, 0.0f), -30.0f, "dBFS"));

    // Reduction (0~20 dB)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::reduction, "Reduction",
        NormalisableRange<float>(0.0f, 20.0f), 6.0f, "dB"));

    // Frequency Low (2000~8000 Hz)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::freqLow, "Freq Low",
        NormalisableRange<float>(2000.0f, 8000.0f), 5000.0f, "Hz"));

    // Frequency High (5000~14000 Hz)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::freqHigh, "Freq High",
        NormalisableRange<float>(5000.0f, 14000.0f), 9000.0f, "Hz"));

    // Mode (0=Gain, 1=DynEQ)
    params.push_back(std::make_unique<AudioParameterInt>(
        ParameterIDs::mode, "Mode", 0, 1, 0));

    return {params.begin(), params.end()};
}

//==============================================================================
// Prepare to Play
//==============================================================================
void VCSurgicalDeEsserProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    mProcessSpec.numChannels = getMainBusNumOutputChannels();

    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VCSurgicalDeEsserProcessor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// Bus Layout Support Check
// IMPORTANT: In JUCE 8, layouts.inputBuses[] returns by value (not reference)
// Use const auto& or auto to capture, NOT auto&
//==============================================================================
bool VCSurgicalDeEsserProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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
void VCSurgicalDeEsserProcessor::processBlock(AudioBuffer<float>& buffer,
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
void VCSurgicalDeEsserProcessor::parameterChanged(const String& parameterID,
                                                  float newValue)
{
    VCPluginDSP::Params p = mDSP.getParams();

    if (parameterID == ParameterIDs::bypass) {
        mBypass = newValue > 0.5f;
        mDSP.setEnabled(!mBypass);
    } else if (parameterID == ParameterIDs::threshold) {
        p.threshold = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::reduction) {
        p.reduction = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::freqLow) {
        p.freqLow = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::freqHigh) {
        p.freqHigh = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::mode) {
        p.mode = static_cast<int>(newValue);
        mDSP.setParams(p);
    }
}

//==============================================================================
// State Information
//==============================================================================
void VCSurgicalDeEsserProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        MemoryOutputStream mos(destData, true);
        xml->writeTo(mos, {});
    }
}

void VCSurgicalDeEsserProcessor::setStateInformation(const void* data,
                                                      int sizeInBytes)
{
    auto xmlState = parseXML(String(static_cast<const char*>(data), sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Create Editor
//==============================================================================
AudioProcessorEditor* VCSurgicalDeEsserProcessor::createEditor()
{
    return new VCSurgicalDeEsserEditor(*this);
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCSurgicalDeEsserProcessor();
}
