#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Construction / Destruction
//==============================================================================

VCDrumProcessor::VCDrumProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      mAPVTS(*this, nullptr, "Parameters", createParameterLayout())
{
    // Register parameter listeners
    mAPVTS.addParameterListener(ParameterIDs::kickFreqStart, this);
    mAPVTS.addParameterListener(ParameterIDs::kickFreqEnd, this);
    mAPVTS.addParameterListener(ParameterIDs::kickDecay, this);
    mAPVTS.addParameterListener(ParameterIDs::kickDrive, this);
    mAPVTS.addParameterListener(ParameterIDs::kickGain, this);
    mAPVTS.addParameterListener(ParameterIDs::snareTone, this);
    mAPVTS.addParameterListener(ParameterIDs::snareDecay, this);
    mAPVTS.addParameterListener(ParameterIDs::snareGain, this);
    mAPVTS.addParameterListener(ParameterIDs::hihatDecayClosed, this);
    mAPVTS.addParameterListener(ParameterIDs::hihatDecayOpen, this);
    mAPVTS.addParameterListener(ParameterIDs::hihatGain, this);
    mAPVTS.addParameterListener(ParameterIDs::clapCount, this);
    mAPVTS.addParameterListener(ParameterIDs::clapDecay, this);
    mAPVTS.addParameterListener(ParameterIDs::clapGain, this);
    mAPVTS.addParameterListener(ParameterIDs::masterGain, this);
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
}

VCDrumProcessor::~VCDrumProcessor()
{
}

//==============================================================================
// Editor
//==============================================================================

juce::AudioProcessorEditor* VCDrumProcessor::createEditor()
{
    return new VCDrumEditor(*this);
}
//==============================================================================
// Parameter Layout
//==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
VCDrumProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Kick parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::kickFreqStart, "Kick Freq Start",
        juce::NormalisableRange<float>(20.0f, 500.0f, 1.0f), 150.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::kickFreqEnd, "Kick Freq End",
        juce::NormalisableRange<float>(20.0f, 200.0f, 1.0f), 50.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::kickDecay, "Kick Decay",
        juce::NormalisableRange<float>(10.0f, 2000.0f, 1.0f, 0.5f), 300.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::kickDrive, "Kick Drive",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::kickGain, "Kick Gain",
        juce::NormalisableRange<float>(-30.0f, 12.0f, 0.1f), 0.0f));

    // Snare parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::snareTone, "Snare Tone",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::snareDecay, "Snare Decay",
        juce::NormalisableRange<float>(10.0f, 2000.0f, 1.0f, 0.5f), 200.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::snareGain, "Snare Gain",
        juce::NormalisableRange<float>(-30.0f, 12.0f, 0.1f), 0.0f));

    // HiHat parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::hihatDecayClosed, "HiHat Decay Closed",
        juce::NormalisableRange<float>(5.0f, 500.0f, 1.0f), 50.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::hihatDecayOpen, "HiHat Decay Open",
        juce::NormalisableRange<float>(10.0f, 2000.0f, 1.0f, 0.5f), 300.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::hihatGain, "HiHat Gain",
        juce::NormalisableRange<float>(-30.0f, 12.0f, 0.1f), 0.0f));

    // Clap parameters
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParameterIDs::clapCount, "Clap Count", 2, 8, 3));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::clapDecay, "Clap Decay",
        juce::NormalisableRange<float>(10.0f, 2000.0f, 1.0f, 0.5f), 250.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::clapGain, "Clap Gain",
        juce::NormalisableRange<float>(-30.0f, 12.0f, 0.1f), 0.0f));

    // Master
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::masterGain, "Master Gain",
        juce::NormalisableRange<float>(-30.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    return { params.begin(), params.end() };
}

//==============================================================================
// Prepare / Release
//==============================================================================

void VCDrumProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VCDrumProcessor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// Bus Layout — VSTi: output only (stereo), no input bus
//==============================================================================

bool VCDrumProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // VSTi: output only (stereo)
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // No input required
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    return true;
}

//==============================================================================
// MIDI Note → Drum Engine Mapping
// GM Percussion standard (Channel 10)
//==============================================================================

void VCDrumProcessor::handleMidiNoteOn(int noteNumber, float velocity)
{
    // Map GM percussion note numbers to drum engines
    // Multiple notes can map to the same engine for compatibility
    if (noteNumber == DrumMap::Kick || noteNumber == 35)  // 35=Bass Drum 2
    {
        mDSP.triggerDrum(DrumKick, velocity);
    }
    else if (noteNumber == DrumMap::Snare || noteNumber == 40)  // 40=Electric Snare
    {
        mDSP.triggerDrum(DrumSnare, velocity);
    }
    else if (noteNumber == DrumMap::HiHatClosed ||
             noteNumber == 44)  // 44=Pedal Hi-Hat
    {
        mDSP.triggerDrum(DrumHiHat, velocity, false);  // closed
    }
    else if (noteNumber == DrumMap::HiHatOpen)
    {
        mDSP.triggerDrum(DrumHiHat, velocity, true);  // open
    }
    else if (noteNumber == DrumMap::Clap)
    {
        mDSP.triggerDrum(DrumClap, velocity);
    }
    // Rimshot → Snare
    else if (noteNumber == DrumMap::Rimshot)
    {
        mDSP.triggerDrum(DrumSnare, velocity);
    }
}

void VCDrumProcessor::handleMidiNoteOff(int /*noteNumber*/)
{
    // Drums are one-shot — noteOff has no effect
    // (Could implement choke groups in the future)
}

//==============================================================================
// Process Block — VSTi with MIDI input
//==============================================================================

void VCDrumProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midiBuffer)
{
    juce::ScopedNoDenormals noDenormals;

    // Clear output buffer (instrument has no input)
    buffer.clear();

    if (mBypass)
        return;

    // Process MIDI messages → trigger drum engines
    for (const auto metadata : midiBuffer)
    {
        auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            float velocity = message.getVelocity() / 127.0f;
            handleMidiNoteOn(message.getNoteNumber(), velocity);
        }
        else if (message.isNoteOff())
        {
            handleMidiNoteOff(message.getNoteNumber());
        }
    }

    // Render drum audio through DSP (MIDI-triggered mode)
    auto totalNumSamples = buffer.getNumSamples();
    if (totalNumSamples == 0)
        return;

    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);

    mDSP.render(outL, outR, totalNumSamples);
}

//==============================================================================
// Parameter Changed Callback
//==============================================================================

void VCDrumProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    VCPluginDSP::Params p = mDSP.getParams();

    if (parameterID == ParameterIDs::kickFreqStart)
        p.kick.freqStart = newValue;
    else if (parameterID == ParameterIDs::kickFreqEnd)
        p.kick.freqEnd = newValue;
    else if (parameterID == ParameterIDs::kickDecay)
        p.kick.decay = newValue;
    else if (parameterID == ParameterIDs::kickDrive)
        p.kick.drive = newValue;
    else if (parameterID == ParameterIDs::kickGain)
        mKickGainDB = newValue;
    else if (parameterID == ParameterIDs::snareTone)
        p.snare.tone = newValue;
    else if (parameterID == ParameterIDs::snareDecay)
        p.snare.decay = newValue;
    else if (parameterID == ParameterIDs::snareGain)
        mSnareGainDB = newValue;
    else if (parameterID == ParameterIDs::hihatDecayClosed)
        p.hihat.decayClosed = newValue;
    else if (parameterID == ParameterIDs::hihatDecayOpen)
        p.hihat.decayOpen = newValue;
    else if (parameterID == ParameterIDs::hihatGain)
        mHiHatGainDB = newValue;
    else if (parameterID == ParameterIDs::clapCount)
        p.clap.clapCount = (int)newValue;
    else if (parameterID == ParameterIDs::clapDecay)
        p.clap.decay = newValue;
    else if (parameterID == ParameterIDs::clapGain)
        mClapGainDB = newValue;
    else if (parameterID == ParameterIDs::masterGain)
    {
        mMasterGainDB = newValue;
        p.masterGain = newValue;
    }
    else if (parameterID == ParameterIDs::bypass)
        mBypass = (newValue > 0.5f);
    else
        return;

    mDSP.setParams(p);
    mDSP.setEnabled(!mBypass);
}

//==============================================================================
// Program Support
//==============================================================================

int VCDrumProcessor::getNumPrograms()
{
    return 1;  // TODO: add presets
}

void VCDrumProcessor::setCurrentProgram(int /*index*/)
{
}

const juce::String VCDrumProcessor::getProgramName(int /*index*/)
{
    return "Default";
}

//==============================================================================
// State Save/Restore
//==============================================================================

void VCDrumProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VCDrumProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr)
    {
        auto state = juce::ValueTree::fromXml(*xml);
        mAPVTS.replaceState(state);
    }
}

//==============================================================================
// Plugin Entry Point
//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCDrumProcessor();
}
