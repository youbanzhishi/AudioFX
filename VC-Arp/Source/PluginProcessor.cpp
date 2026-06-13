#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Construction / Destruction
//==============================================================================

VCArpProcessor::VCArpProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      mAPVTS(*this, nullptr, "Parameters", createParameterLayout())
{
    mAPVTS.addParameterListener(ParameterIDs::mode, this);
    mAPVTS.addParameterListener(ParameterIDs::rate, this);
    mAPVTS.addParameterListener(ParameterIDs::octaveRange, this);
    mAPVTS.addParameterListener(ParameterIDs::gate, this);
    mAPVTS.addParameterListener(ParameterIDs::swing, this);
    mAPVTS.addParameterListener(ParameterIDs::humanize, this);
    mAPVTS.addParameterListener(ParameterIDs::velocityMode, this);
    mAPVTS.addParameterListener(ParameterIDs::bpm, this);
    mAPVTS.addParameterListener(ParameterIDs::transpose, this);
    mAPVTS.addParameterListener(ParameterIDs::waveform, this);
    mAPVTS.addParameterListener(ParameterIDs::volume, this);
    mAPVTS.addParameterListener(ParameterIDs::bypass, this);
}

VCArpProcessor::~VCArpProcessor()
{
}

//==============================================================================
// Editor
//==============================================================================

juce::AudioProcessorEditor* VCArpProcessor::createEditor()
{
    return new VCArpEditor(*this);
}

//==============================================================================
// Parameter Layout
//==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
VCArpProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Arp Mode
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterIDs::mode, "Arp Mode",
        juce::StringArray{"Up", "Down", "Up-Down", "Down-Up",
                          "Random", "As-Played", "Chord"}, 0));

    // Rate (subdivision)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterIDs::rate, "Rate",
        juce::StringArray{"1/1", "1/2", "1/4", "1/8", "1/16", "1/32"}, 3));

    // Octave Range
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParameterIDs::octaveRange, "Octave Range", 1, 4, 1));

    // Gate
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::gate, "Gate",
        juce::NormalisableRange<float>(1.0f, 200.0f, 1.0f), 100.0f));

    // Swing
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::swing, "Swing",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f));

    // Humanize
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::humanize, "Humanize",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f));

    // Velocity Mode
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterIDs::velocityMode, "Velocity Mode",
        juce::StringArray{"Original", "Ascending", "Descending", "Random"}, 0));

    // BPM
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::bpm, "BPM",
        juce::NormalisableRange<float>(40.0f, 300.0f, 0.1f), 120.0f));

    // Transpose
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParameterIDs::transpose, "Transpose", -24, 24, 0));

    // Waveform (internal synth)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterIDs::waveform, "Waveform",
        juce::StringArray{"Sine", "Saw", "Square"}, 0));

    // Volume
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::volume, "Volume",
        juce::NormalisableRange<float>(-60.0f, 12.0f, 0.1f), -6.0f));

    // Bypass
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));

    return { params.begin(), params.end() };
}

//==============================================================================
// Prepare / Release
//==============================================================================

void VCArpProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mDSP.prepare(sampleRate, samplesPerBlock);
}

void VCArpProcessor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// Bus Layout — VSTi: output only (stereo), no input bus
//==============================================================================

bool VCArpProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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
// Process Block — VSTi with MIDI input
//==============================================================================

void VCArpProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& midiBuffer)
{
    juce::ScopedNoDenormals noDenormals;

    // Clear output buffer (instrument has no input)
    buffer.clear();

    if (mBypass)
        return;

    // Process MIDI messages → track held notes for chord input
    for (const auto metadata : midiBuffer)
    {
        auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            int note = message.getNoteNumber();
            float velocity = message.getVelocity() / 127.0f;

            // Add to held notes if not already present
            if (std::find(mHeldNotes.begin(), mHeldNotes.end(), note) == mHeldNotes.end())
                mHeldNotes.push_back(note);

            // Update arpeggiator with current chord
            mDSP.noteOn(note, velocity);
            mDSP.setChordNotes(mHeldNotes);
        }
        else if (message.isNoteOff())
        {
            int note = message.getNoteNumber();

            // Remove from held notes
            mHeldNotes.erase(
                std::remove(mHeldNotes.begin(), mHeldNotes.end(), note),
                mHeldNotes.end());

            mDSP.noteOff(note);

            // Update arpeggiator with remaining chord notes
            mDSP.setChordNotes(mHeldNotes);
        }
    }

    // Render arpeggio audio
    auto totalNumSamples = buffer.getNumSamples();
    if (totalNumSamples == 0)
        return;

    // Create temp buffers for DSP
    std::vector<float> left(totalNumSamples, 0.0f);
    std::vector<float> right(totalNumSamples, 0.0f);

    mDSP.render(left.data(), right.data(), totalNumSamples);

    // Copy to output buffer
    auto* outL = buffer.getWritePointer(0);
    auto* outR = buffer.getWritePointer(1);

    for (int i = 0; i < totalNumSamples; ++i)
    {
        outL[i] = left[i];
        outR[i] = right[i];
    }
}

//==============================================================================
// Parameter Changed Callback
//==============================================================================

void VCArpProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    VCPluginDSP::Params p = mDSP.getParams();

    if (parameterID == ParameterIDs::mode)
        p.mode = static_cast<VCArpMode>((int)newValue);
    else if (parameterID == ParameterIDs::rate)
        p.rate = static_cast<VCArpRate>((int)newValue);
    else if (parameterID == ParameterIDs::octaveRange)
        p.octaveRange = (int)newValue;
    else if (parameterID == ParameterIDs::gate)
        p.gate = newValue;
    else if (parameterID == ParameterIDs::swing)
        p.swing = newValue;
    else if (parameterID == ParameterIDs::humanize)
        p.humanize = newValue;
    else if (parameterID == ParameterIDs::velocityMode)
        p.velocityMode = static_cast<VCVelocityMode>((int)newValue);
    else if (parameterID == ParameterIDs::bpm)
        p.bpm = newValue;
    else if (parameterID == ParameterIDs::transpose)
        p.transpose = (int)newValue;
    else if (parameterID == ParameterIDs::waveform)
        p.waveform = static_cast<VCArpSynth::Waveform>((int)newValue);
    else if (parameterID == ParameterIDs::volume)
        p.volumeDB = newValue;
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

int VCArpProcessor::getNumPrograms()
{
    return VCPluginDSP::getNumPresets();
}

void VCArpProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= VCPluginDSP::getNumPresets()) return;
    mCurrentProgram = index;

    VCPluginDSP::Params p;
    if (VCPluginDSP::getPreset(index, p))
        mDSP.setParams(p);
}

const juce::String VCArpProcessor::getProgramName(int index)
{
    const char* name = VCPluginDSP::getPresetName(index);
    return name ? juce::String(name) : juce::String();
}

//==============================================================================
// State Save/Restore
//==============================================================================

void VCArpProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void VCArpProcessor::setStateInformation(const void* data, int sizeInBytes)
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
    return new VCArpProcessor();
}
