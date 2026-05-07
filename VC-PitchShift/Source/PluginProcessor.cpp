#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Construction / Destruction
//==============================================================================
VCPitchShiftProcessor::VCPitchShiftProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::stereo())
                     .withOutput("Output", AudioChannelSet::stereo()))
    , mAPVTS(*this, nullptr, Identifier("VCPitchShiftParameters"),
             createParameterLayout())
{
    // Register parameter listeners
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
    mAPVTS.addParameterListener(ParameterIDs::semitones, this);
    mAPVTS.addParameterListener(ParameterIDs::cents, this);
    mAPVTS.addParameterListener(ParameterIDs::formant, this);
}

VCPitchShiftProcessor::~VCPitchShiftProcessor()
{
}

//==============================================================================
// Parameter Layout
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout
VCPitchShiftProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Bypass
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    // Semitones (-12 ~ +12)
    params.push_back(std::make_unique<AudioParameterInt>(
        ParameterIDs::semitones, "Semitones", -12, 12, 0));

    // Cents (-100 ~ +100)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::cents, "Cents",
        NormalisableRange<float>(-100.0f, 100.0f), 0.0f, "ct"));

    // Formant preservation
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::formant, "Formant", false));

    return {params.begin(), params.end()};
}

//==============================================================================
// Prepare to Play
//==============================================================================
void VCPitchShiftProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = static_cast<uint32>(samplesPerBlock);
    mProcessSpec.numChannels = getMainBusNumOutputChannels();

    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VCPitchShiftProcessor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// Bus Layout Support Check
//==============================================================================
bool VCPitchShiftProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto inputLayout = layouts.inputBuses[0];
    const auto outputLayout = layouts.outputBuses[0];

    return inputLayout == AudioChannelSet::stereo() &&
           outputLayout == AudioChannelSet::stereo();
}

//==============================================================================
// Process Audio Block
//==============================================================================
void VCPitchShiftProcessor::processBlock(AudioBuffer<float>& buffer,
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
void VCPitchShiftProcessor::parameterChanged(const String& parameterID,
                                              float newValue)
{
    VCPluginDSP::Params p = mDSP.getParams();

    if (parameterID == ParameterIDs::bypass) {
        mBypass = newValue > 0.5f;
        mDSP.setEnabled(!mBypass);
    } else if (parameterID == ParameterIDs::semitones) {
        p.semitones = static_cast<int>(newValue);
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::cents) {
        p.cents = newValue;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::formant) {
        p.formant = newValue > 0.5f;
        mDSP.setParams(p);
    }
}

//==============================================================================
// State Information
//==============================================================================
void VCPitchShiftProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        MemoryOutputStream mos(destData, true);
        xml->writeTo(mos, {});
    }
}

void VCPitchShiftProcessor::setStateInformation(const void* data,
                                                  int sizeInBytes)
{
    auto xmlState = parseXML(String(static_cast<const char*>(data), sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Create Editor
//==============================================================================
AudioProcessorEditor* VCPitchShiftProcessor::createEditor()
{
    return new VCPitchShiftEditor(*this);
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCPitchShiftProcessor();
}
