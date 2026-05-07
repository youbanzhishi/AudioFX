#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
VCHarmonizerProcessor::VCHarmonizerProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", AudioChannelSet::stereo())
                     .withOutput("Output", AudioChannelSet::stereo()))
    , mAPVTS(*this, nullptr, Identifier("VCHarmonizerParameters"),
             createParameterLayout())
{
    // Register parameter listeners
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
    mAPVTS.addParameterListener(ParameterIDs::numVoices, this);
    mAPVTS.addParameterListener(ParameterIDs::formantPreserve, this);
    mAPVTS.addParameterListener(ParameterIDs::autoKey, this);
    mAPVTS.addParameterListener(ParameterIDs::scale, this);
    mAPVTS.addParameterListener(ParameterIDs::direction, this);
}

VCHarmonizerProcessor::~VCHarmonizerProcessor()
{
}

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout
VCHarmonizerProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Bypass
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    // Number of voices
    params.push_back(std::make_unique<AudioParameterInt>(
        ParameterIDs::numVoices, "Voices", 1, 4, 2));

    // Intervals (semitones for each voice)
    params.push_back(std::make_unique<AudioParameterInt>(
        ParameterIDs::interval0, "Interval 1", -24, 24, 3));
    params.push_back(std::make_unique<AudioParameterInt>(
        ParameterIDs::interval1, "Interval 2", -24, 24, 7));
    params.push_back(std::make_unique<AudioParameterInt>(
        ParameterIDs::interval2, "Interval 3", -24, 24, 12));
    params.push_back(std::make_unique<AudioParameterInt>(
        ParameterIDs::interval3, "Interval 4", -24, 24, -5));

    // Voice gain (dB)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::voiceGain0, "Voice 1 Gain",
        NormalisableRange<float>(-24.0f, 24.0f), 0.0f, "dB"));
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::voiceGain1, "Voice 2 Gain",
        NormalisableRange<float>(-24.0f, 24.0f), 0.0f, "dB"));
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::voiceGain2, "Voice 3 Gain",
        NormalisableRange<float>(-24.0f, 24.0f), 0.0f, "dB"));
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::voiceGain3, "Voice 4 Gain",
        NormalisableRange<float>(-24.0f, 24.0f), 0.0f, "dB"));

    // Voice pan (-1 to 1)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::voicePan0, "Voice 1 Pan",
        NormalisableRange<float>(-1.0f, 1.0f), -0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::voicePan1, "Voice 2 Pan",
        NormalisableRange<float>(-1.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::voicePan2, "Voice 3 Pan",
        NormalisableRange<float>(-1.0f, 1.0f), 0.7f));
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::voicePan3, "Voice 4 Pan",
        NormalisableRange<float>(-1.0f, 1.0f), -0.7f));

    // Formant preserve
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::formantPreserve, "Formant Preserve",
        NormalisableRange<float>(0.0f, 100.0f), 100.0f, "%"));

    // Auto key
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::autoKey, "Auto Key", false));

    // Scale
    params.push_back(std::make_unique<AudioParameterInt>(
        ParameterIDs::scale, "Scale", 0, 5, 0));

    // Direction (0=both, 1=up, 2=down)
    params.push_back(std::make_unique<AudioParameterInt>(
        ParameterIDs::direction, "Direction", 0, 2, 0));

    // MIDI track
    params.push_back(std::make_unique<AudioParameterInt>(
        ParameterIDs::midiTrack, "MIDI Track", -1, 16, -1));

    return {params.begin(), params.end()};
}

//==============================================================================
void VCHarmonizerProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    mProcessSpec.numChannels = getMainBusNumOutputChannels();

    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VCHarmonizerProcessor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
bool VCHarmonizerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto inputLayout = layouts.inputBuses[0];
    const auto outputLayout = layouts.outputBuses[0];

    return inputLayout == AudioChannelSet::stereo() &&
           outputLayout == AudioChannelSet::stereo();
}

//==============================================================================
void VCHarmonizerProcessor::processBlock(AudioBuffer<float>& buffer,
                                           MidiBuffer& midiBuffer)
{
    if (mBypass)
        return;

    // TODO: Process MIDI for harmony control when midiTrack >= 0

    int numSamples = buffer.getNumSamples();
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getWritePointer(1);

    mDSP.process(leftChannel, rightChannel, numSamples);
}

//==============================================================================
void VCHarmonizerProcessor::parameterChanged(const String& parameterID,
                                               float newValue)
{
    auto p = mDSP.getParams();

    if (parameterID == ParameterIDs::bypass) {
        mBypass = newValue > 0.5f;
        mDSP.setEnabled(!mBypass);
    } else if (parameterID == ParameterIDs::numVoices) {
        p.numVoices = static_cast<int>(newValue);
    } else if (parameterID == ParameterIDs::interval0) {
        p.intervals[0] = static_cast<int>(newValue);
    } else if (parameterID == ParameterIDs::interval1) {
        p.intervals[1] = static_cast<int>(newValue);
    } else if (parameterID == ParameterIDs::interval2) {
        p.intervals[2] = static_cast<int>(newValue);
    } else if (parameterID == ParameterIDs::interval3) {
        p.intervals[3] = static_cast<int>(newValue);
    } else if (parameterID == ParameterIDs::voiceGain0) {
        p.voiceGain[0] = newValue;
    } else if (parameterID == ParameterIDs::voiceGain1) {
        p.voiceGain[1] = newValue;
    } else if (parameterID == ParameterIDs::voiceGain2) {
        p.voiceGain[2] = newValue;
    } else if (parameterID == ParameterIDs::voiceGain3) {
        p.voiceGain[3] = newValue;
    } else if (parameterID == ParameterIDs::voicePan0) {
        p.voicePan[0] = newValue;
    } else if (parameterID == ParameterIDs::voicePan1) {
        p.voicePan[1] = newValue;
    } else if (parameterID == ParameterIDs::voicePan2) {
        p.voicePan[2] = newValue;
    } else if (parameterID == ParameterIDs::voicePan3) {
        p.voicePan[3] = newValue;
    } else if (parameterID == ParameterIDs::formantPreserve) {
        p.formantPreserve = newValue;
    } else if (parameterID == ParameterIDs::autoKey) {
        p.autoKey = newValue > 0.5f;
    } else if (parameterID == ParameterIDs::scale) {
        p.scale = static_cast<int>(newValue);
    } else if (parameterID == ParameterIDs::direction) {
        p.direction = static_cast<int>(newValue);
    } else if (parameterID == ParameterIDs::midiTrack) {
        p.midiTrack = static_cast<int>(newValue);
    }

    mDSP.setParams(p);
}

//==============================================================================
void VCHarmonizerProcessor::getStateInformation(MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        MemoryOutputStream mos(destData, true);
        xml->writeTo(mos, {});
    }
}

void VCHarmonizerProcessor::setStateInformation(const void* data,
                                                    int sizeInBytes)
{
    auto xmlState = parseXML(String(static_cast<const char*>(data), sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
AudioProcessorEditor* VCHarmonizerProcessor::createEditor()
{
    return new VCHarmonizerEditor(*this);
}

//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCHarmonizerProcessor();
}
