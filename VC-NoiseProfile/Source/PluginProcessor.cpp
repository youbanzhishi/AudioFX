#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Construction / Destruction
//==============================================================================
VCNoiseProfileProcessor::VCNoiseProfileProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::stereo())
                     .withOutput("Output", AudioChannelSet::stereo()))
    , mAPVTS(*this, nullptr, Identifier("VCNoiseProfileParameters"),
             createParameterLayout())
{
    // Register parameter listeners
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
    mAPVTS.addParameterListener(ParameterIDs::processMode, this);
    mAPVTS.addParameterListener(ParameterIDs::reduction, this);
    mAPVTS.addParameterListener(ParameterIDs::noiseFloor, this);
    mAPVTS.addParameterListener(ParameterIDs::attack, this);
    mAPVTS.addParameterListener(ParameterIDs::release, this);
    mAPVTS.addParameterListener(ParameterIDs::fftSize, this);
    mAPVTS.addParameterListener(ParameterIDs::noiseLearnTime, this);
    mAPVTS.addParameterListener(ParameterIDs::gateThreshold, this);
    mAPVTS.addParameterListener(ParameterIDs::gateDepth, this);
}

VCNoiseProfileProcessor::~VCNoiseProfileProcessor()
{
}

//==============================================================================
// Parameter Layout
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout
VCNoiseProfileProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Bypass
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    // Process Mode: 0=Denoise, 1=Gate, 2=Both, 3=Analyze
    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterIDs::processMode, "Process Mode",
        StringArray{"Denoise", "Gate", "Both", "Analyze"}, 2));

    // Reduction (spectral subtraction amount, 0-30 dB)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::reduction, "Reduction",
        NormalisableRange<float>(0.0f, 30.0f), 10.0f, "dB"));

    // Noise Floor (spectral floor ratio, 1-20)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::noiseFloor, "Noise Floor",
        NormalisableRange<float>(1.0f, 20.0f), 5.0f));

    // Attack (0.1-100 ms)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::attack, "Attack",
        NormalisableRange<float>(0.1f, 100.0f), 5.0f, "ms"));

    // Release (1-1000 ms)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::release, "Release",
        NormalisableRange<float>(1.0f, 1000.0f), 50.0f, "ms"));

    // FFT Size (256/512/1024/2048)
    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterIDs::fftSize, "FFT Size",
        StringArray{"256", "512", "1024", "2048"}, 1));

    // Noise Learn Time (100-5000 ms)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::noiseLearnTime, "Noise Learn Time",
        NormalisableRange<float>(100.0f, 5000.0f), 500.0f, "ms"));

    // Gate Threshold (-80 to 0 dB)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::gateThreshold, "Gate Threshold",
        NormalisableRange<float>(-80.0f, 0.0f), -40.0f, "dB"));

    // Gate Depth (0-100%)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::gateDepth, "Gate Depth",
        NormalisableRange<float>(0.0f, 100.0f), 100.0f, "%"));

    return {params.begin(), params.end()};
}

//==============================================================================
// Prepare to Play
//==============================================================================
void VCNoiseProfileProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    mProcessSpec.numChannels = getMainBusNumOutputChannels();

    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VCNoiseProfileProcessor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// Bus Layout Support Check
// IMPORTANT: In JUCE 8, layouts.inputBuses[] returns by value (not reference)
//==============================================================================
bool VCNoiseProfileProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto inputLayout = layouts.inputBuses[0];
    const auto outputLayout = layouts.outputBuses[0];

    return inputLayout == AudioChannelSet::stereo() &&
           outputLayout == AudioChannelSet::stereo();
}

//==============================================================================
// Process Audio Block
//==============================================================================
void VCNoiseProfileProcessor::processBlock(AudioBuffer<float>& buffer,
                                             MidiBuffer&)
{
    if (mBypass)
        return;

    int numSamples = buffer.getNumSamples();
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getWritePointer(1);

    // In VST3 plugin mode, use process() which handles denoise/gate
    // based on the current mode setting
    mDSP.process(leftChannel, rightChannel, numSamples);
}

//==============================================================================
// Parameter Changed Callback
//==============================================================================
void VCNoiseProfileProcessor::parameterChanged(const String& parameterID,
                                                  float newValue)
{
    auto p = mDSP.getParams();

    if (parameterID == ParameterIDs::bypass) {
        mBypass = newValue > 0.5f;
        mDSP.setEnabled(!mBypass);
    } else if (parameterID == ParameterIDs::processMode) {
        p.mode = static_cast<int>(newValue);
    } else if (parameterID == ParameterIDs::reduction) {
        p.reduction = newValue;
    } else if (parameterID == ParameterIDs::noiseFloor) {
        p.floor = newValue;
    } else if (parameterID == ParameterIDs::attack) {
        p.attack = newValue;
    } else if (parameterID == ParameterIDs::release) {
        p.release = newValue;
    } else if (parameterID == ParameterIDs::fftSize) {
        // FFT size is a compile-time constant in current implementation
        // Future: support dynamic FFT size changes
        // Index 0=256, 1=512, 2=1024, 3=2048
    } else if (parameterID == ParameterIDs::noiseLearnTime) {
        p.learnMs = newValue;
    } else if (parameterID == ParameterIDs::gateThreshold) {
        p.threshold = newValue;
    } else if (parameterID == ParameterIDs::gateDepth) {
        // Gate depth: 100% = full gate, 0% = no gating
        // Future: integrate into VCNoiseGate as depth parameter
    }

    mDSP.setParams(p);
}

//==============================================================================
// State Information
//==============================================================================
void VCNoiseProfileProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        MemoryOutputStream mos(destData, true);
        xml->writeTo(mos, {});
    }
}

void VCNoiseProfileProcessor::setStateInformation(const void* data,
                                                      int sizeInBytes)
{
    auto xmlState = parseXML(String(static_cast<const char*>(data), sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Create Editor
//==============================================================================
AudioProcessorEditor* VCNoiseProfileProcessor::createEditor()
{
    return new VCNoiseProfileEditor(*this);
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCNoiseProfileProcessor();
}
