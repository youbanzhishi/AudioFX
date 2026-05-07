#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Construction / Destruction
//==============================================================================
VCSaturatorProcessor::VCSaturatorProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::stereo())
                     .withOutput("Output", AudioChannelSet::stereo()))
    , mAPVTS(*this, nullptr, Identifier("VCSaturatorParameters"),
             createParameterLayout())
{
    // Register parameter listeners
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
    mAPVTS.addParameterListener(ParameterIDs::drive, this);
    mAPVTS.addParameterListener(ParameterIDs::algorithm, this);
    mAPVTS.addParameterListener(ParameterIDs::mix, this);
}

VCSaturatorProcessor::~VCSaturatorProcessor()
{
}

//==============================================================================
// Parameter Layout
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout
VCSaturatorProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Bypass
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    // Drive
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::drive, "Drive",
        NormalisableRange<float>(0.0f, 24.0f), 0.0f, "dB"));

    // Algorithm (Choice)
    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterIDs::algorithm, "Algorithm",
        StringArray{"Tape", "Tube", "Clip"}, 0));

    // Mix
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::mix, "Mix",
        NormalisableRange<float>(0.0f, 100.0f), 100.0f, "%"));

    return {params.begin(), params.end()};
}

//==============================================================================
// Prepare to Play
//==============================================================================
void VCSaturatorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    mProcessSpec.numChannels = getMainBusNumOutputChannels();

    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VCSaturatorProcessor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// Bus Layout Support Check
// IMPORTANT: In JUCE 8, layouts.inputBuses[] returns by value (not reference)
// Use const auto& or auto to capture, NOT auto&
//==============================================================================
bool VCSaturatorProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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
void VCSaturatorProcessor::processBlock(AudioBuffer<float>& buffer,
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
void VCSaturatorProcessor::parameterChanged(const String& parameterID,
                                              float newValue)
{
    if (parameterID == ParameterIDs::bypass) {
        mBypass = newValue > 0.5f;
        mDSP.setEnabled(!mBypass);
    } else if (parameterID == ParameterIDs::drive) {
        mDrive = newValue;
        VCSaturatorDSP::Params p = mDSP.getParams();
        p.drive = mDrive;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::algorithm) {
        mAlgorithm = static_cast<int>(newValue);
        VCSaturatorDSP::Params p = mDSP.getParams();
        p.algorithm = mAlgorithm;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::mix) {
        mMix = newValue;
        VCSaturatorDSP::Params p = mDSP.getParams();
        p.mix = mMix;
        mDSP.setParams(p);
    }
}

//==============================================================================
// State Information
//==============================================================================
void VCSaturatorProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        MemoryOutputStream mos(destData, true);
        xml->writeTo(mos, {});
    }
}

void VCSaturatorProcessor::setStateInformation(const void* data,
                                                  int sizeInBytes)
{
    auto xmlState = parseXML(String(static_cast<const char*>(data), sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Create Editor
//==============================================================================
AudioProcessorEditor* VCSaturatorProcessor::createEditor()
{
    return new VCSaturatorEditor(*this);
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCSaturatorProcessor();
}
