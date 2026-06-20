#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Construction / Destruction
//==============================================================================
VCHall480Processor::VCHall480Processor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::stereo())
                     .withOutput("Output", AudioChannelSet::stereo()))
    , mAPVTS(*this, nullptr, Identifier("VCHall480Parameters"),
             createParameterLayout())
{
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
    mAPVTS.addParameterListener(ParameterIDs::algorithm, this);
    mAPVTS.addParameterListener(ParameterIDs::room, this);
    mAPVTS.addParameterListener(ParameterIDs::decay, this);
    mAPVTS.addParameterListener(ParameterIDs::preDelay, this);
    mAPVTS.addParameterListener(ParameterIDs::diffusion, this);
    mAPVTS.addParameterListener(ParameterIDs::shape, this);
    mAPVTS.addParameterListener(ParameterIDs::spread, this);
    mAPVTS.addParameterListener(ParameterIDs::hiDecay, this);
    mAPVTS.addParameterListener(ParameterIDs::loDecay, this);
    mAPVTS.addParameterListener(ParameterIDs::chorusRate, this);
    mAPVTS.addParameterListener(ParameterIDs::chorusDepth, this);
    mAPVTS.addParameterListener(ParameterIDs::mix, this);

    // Explicitly initialise DSP state to match the default bypass value.
    // Listeners only fire on *changes*, so this is the only call site on load.
    mDSP.setEnabled(!mBypass);
}

VCHall480Processor::~VCHall480Processor()
{
}

//==============================================================================
// Parameter Layout
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout
VCHall480Processor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Bypass
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    // Algorithm: 0=Hall, 1=Random Hall, 2=Plate
    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterIDs::algorithm, "Algorithm",
        StringArray{"Hall", "Random Hall", "Plate"}, 0));

    // Room Size
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::room, "Room Size",
        NormalisableRange<float>(0.0f, 100.0f), 50.0f, "%"));

    // Decay Time (T60)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::decay, "Decay",
        NormalisableRange<float>(0.3f, 20.0f, 0.1f), 2.0f, "s"));

    // Pre-Delay
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::preDelay, "Pre-Delay",
        NormalisableRange<float>(0.0f, 200.0f), 20.0f, "ms"));

    // Diffusion
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::diffusion, "Diffusion",
        NormalisableRange<float>(0.0f, 100.0f), 70.0f, "%"));

    // Shape
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::shape, "Shape",
        NormalisableRange<float>(0.0f, 100.0f), 50.0f, "%"));

    // Spread
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::spread, "Spread",
        NormalisableRange<float>(0.0f, 100.0f), 80.0f, "%"));

    // Hi Decay
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::hiDecay, "Hi Decay",
        NormalisableRange<float>(0.1f, 2.0f), 0.5f, "x"));

    // Lo Decay
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::loDecay, "Lo Decay",
        NormalisableRange<float>(0.1f, 2.0f), 1.0f, "x"));

    // Chorus Rate
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::chorusRate, "Chorus Rate",
        NormalisableRange<float>(0.0f, 5.0f), 1.0f, "Hz"));

    // Chorus Depth
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::chorusDepth, "Chorus Depth",
        NormalisableRange<float>(0.0f, 100.0f), 30.0f, "%"));

    // Mix
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::mix, "Mix",
        NormalisableRange<float>(0.0f, 100.0f), 30.0f, "%"));

    return {params.begin(), params.end()};
}

//==============================================================================
// Prepare to Play
//==============================================================================
void VCHall480Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = static_cast<uint32>(samplesPerBlock);
    mProcessSpec.numChannels = getMainBusNumOutputChannels();

    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VCHall480Processor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// Bus Layout Support
//==============================================================================
bool VCHall480Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto inputLayout = layouts.inputBuses[0];
    const auto outputLayout = layouts.outputBuses[0];

    return inputLayout == AudioChannelSet::stereo() &&
           outputLayout == AudioChannelSet::stereo();
}

//==============================================================================
// Process Audio Block
//==============================================================================
void VCHall480Processor::processBlock(AudioBuffer<float>& buffer, MidiBuffer&)
{
    // Passthrough when bypassed: the buffer already contains the input audio,
    // so we simply leave it untouched rather than silently discarding it.
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
void VCHall480Processor::parameterChanged(const String& parameterID, float newValue)
{
    VCPluginDSP::Params p = mDSP.getParams();

    if (parameterID == ParameterIDs::bypass) {
        mBypass = newValue > 0.5f;
        mDSP.setEnabled(!mBypass);
    } else if (parameterID == ParameterIDs::algorithm) {
        mAlgorithm = static_cast<int>(newValue);
        p.algorithm = mAlgorithm;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::room) {
        mRoomSize = newValue;
        p.roomSize = mRoomSize;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::decay) {
        mDecayTime = newValue;
        p.decayTime = mDecayTime;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::preDelay) {
        mPreDelay = newValue;
        p.preDelay = mPreDelay;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::diffusion) {
        mDiffusion = newValue;
        p.diffusion = mDiffusion;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::shape) {
        mShape = newValue;
        p.shape = mShape;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::spread) {
        mSpread = newValue;
        p.spread = mSpread;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::hiDecay) {
        mHiDecay = newValue;
        p.hiDecay = mHiDecay;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::loDecay) {
        mLoDecay = newValue;
        p.loDecay = mLoDecay;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::chorusRate) {
        mChorusRate = newValue;
        p.chorusRate = mChorusRate;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::chorusDepth) {
        mChorusDepth = newValue;
        p.chorusDepth = mChorusDepth;
        mDSP.setParams(p);
    } else if (parameterID == ParameterIDs::mix) {
        mMix = newValue;
        p.mix = mMix;
        mDSP.setParams(p);
    }
}

//==============================================================================
// State Information
//==============================================================================
void VCHall480Processor::getStateInformation(MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        MemoryOutputStream mos(destData, true);
        xml->writeTo(mos, {});
    }
}

void VCHall480Processor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xmlState = parseXML(String(static_cast<const char*>(data), sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// Create Editor
//==============================================================================
AudioProcessorEditor* VCHall480Processor::createEditor()
{
    return new VCHall480Editor(*this);
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCHall480Processor();
}
