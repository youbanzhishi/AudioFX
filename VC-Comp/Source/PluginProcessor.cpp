#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// Utility functions
//==============================================================================
static inline float dBToLinear(float dB) { return std::pow(10.0f, dB / 20.0f); }
static inline float linearToDb(float linear) { 
    return 20.0f * std::log10(std::max(linear, 1e-10f)); 
}

//==============================================================================
// Constructor
//==============================================================================
VCCompAudioProcessor::VCCompAudioProcessor()
{
    // Sidechain bus configuration
    AudioProcessor::BusesProperties props;
    props = props.withInput("Input", AudioChannelSet::stereo())
             .withOutput("Output", AudioChannelSet::stereo())
             .withInput("Sidechain", AudioChannelSet::stereo());
    
    apvts.addParameterListener(ParameterIDs::threshold, this);
    apvts.addParameterListener(ParameterIDs::ratio, this);
    apvts.addParameterListener(ParameterIDs::attack, this);
    apvts.addParameterListener(ParameterIDs::release, this);
    apvts.addParameterListener(ParameterIDs::gain, this);
    apvts.addParameterListener(ParameterIDs::releaseMode, this);
    apvts.addParameterListener(ParameterIDs::compBehavior, this);
    apvts.addParameterListener(ParameterIDs::character, this);
    apvts.addParameterListener(ParameterIDs::mix, this);
    apvts.addParameterListener(ParameterIDs::trim, this);
    apvts.addParameterListener(ParameterIDs::scSource, this);
    apvts.addParameterListener(ParameterIDs::scHPF, this);
    apvts.addParameterListener(ParameterIDs::scListen, this);
    apvts.addParameterListener(ParameterIDs::bypass, this);
    apvts.addParameterListener(ParameterIDs::paramSet, this);
    apvts.addParameterListener(ParameterIDs::kneeMode, this);
    
    updateParameters();
}

VCCompAudioProcessor::~VCCompAudioProcessor()
{
    apvts.removeParameterListener(ParameterIDs::threshold, this);
    apvts.removeParameterListener(ParameterIDs::ratio, this);
    apvts.removeParameterListener(ParameterIDs::attack, this);
    apvts.removeParameterListener(ParameterIDs::release, this);
    apvts.removeParameterListener(ParameterIDs::gain, this);
    apvts.removeParameterListener(ParameterIDs::releaseMode, this);
    apvts.removeParameterListener(ParameterIDs::compBehavior, this);
    apvts.removeParameterListener(ParameterIDs::character, this);
    apvts.removeParameterListener(ParameterIDs::mix, this);
    apvts.removeParameterListener(ParameterIDs::trim, this);
    apvts.removeParameterListener(ParameterIDs::scSource, this);
    apvts.removeParameterListener(ParameterIDs::scHPF, this);
    apvts.removeParameterListener(ParameterIDs::scListen, this);
    apvts.removeParameterListener(ParameterIDs::bypass, this);
    apvts.removeParameterListener(ParameterIDs::paramSet, this);
    apvts.removeParameterListener(ParameterIDs::kneeMode, this);
}

//==============================================================================
// Parameter Layout
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout VCCompAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::threshold, "Threshold",
        NormalisableRange<float>(-60.0f, 0.0f, 0.1f), 0.0f));
    
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::ratio, "Ratio",
        NormalisableRange<float>(0.5f, 50.0f, 0.01f), 1.0f));
    
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::attack, "Attack",
        NormalisableRange<float>(0.5f, 500.0f, 0.1f), 16.0f));
    
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::release, "Release",
        NormalisableRange<float>(5.0f, 5000.0f, 1.0f), 160.0f));
    
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::gain, "Gain",
        NormalisableRange<float>(-30.0f, 30.0f, 0.1f), 0.0f));
    
    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterIDs::releaseMode, "Release Mode",
        StringArray({"ARC", "Manual"}), 0));
    
    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterIDs::compBehavior, "Comp Behavior",
        StringArray({"Electro", "Opto"}), 0));
    
    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterIDs::kneeMode, "Knee",
        StringArray({"Hard", "Soft", "Auto"}), 1));
    
    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterIDs::character, "Character",
        StringArray({"Warm", "Smooth"}), 1));
    
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::mix, "Mix",
        NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
    
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterIDs::trim, "Trim",
        NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f));
    
    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterIDs::scSource, "SC Source",
        StringArray({"Internal", "External"}), 0));
    
    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterIDs::scHPF, "SC HPF",
        StringArray({"Off", "60 Hz", "100 Hz", "200 Hz", "500 Hz"}), 0));
    
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::scListen, "SC Listen", false));
    
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterIDs::bypass, "Bypass", false));
    
    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterIDs::paramSet, "Param Set",
        StringArray({"A", "B"}), 0));
    
    return { params.begin(), params.end() };
}

//==============================================================================
// Parameter Changed Callback
//==============================================================================
void VCCompAudioProcessor::parameterChanged(const String& parameterID, float newValue)
{
    if (parameterID == ParameterIDs::paramSet)
    {
        activeParamSet = (int)newValue;
        currentParams = (activeParamSet == 0) ? paramSetA : paramSetB;
        return;
    }
    
    ParamSet* targetSet = (activeParamSet == 0) ? &paramSetA : &paramSetB;
    
    if (parameterID == ParameterIDs::threshold) targetSet->threshold = newValue;
    else if (parameterID == ParameterIDs::ratio) targetSet->ratio = newValue;
    else if (parameterID == ParameterIDs::attack) targetSet->attack = newValue;
    else if (parameterID == ParameterIDs::release) targetSet->release = newValue;
    else if (parameterID == ParameterIDs::gain) targetSet->gain = newValue;
    else if (parameterID == ParameterIDs::releaseMode) targetSet->releaseMode = (int)newValue;
    else if (parameterID == ParameterIDs::compBehavior) targetSet->compBehavior = (int)newValue;
    else if (parameterID == ParameterIDs::kneeMode) targetSet->kneeMode = (int)newValue;
    else if (parameterID == ParameterIDs::character) targetSet->character = (int)newValue;
    else if (parameterID == ParameterIDs::mix) targetSet->mix = newValue;
    else if (parameterID == ParameterIDs::trim) targetSet->trim = newValue;
    else if (parameterID == ParameterIDs::scSource) targetSet->scSource = (int)newValue;
    else if (parameterID == ParameterIDs::scHPF) targetSet->scHPF = (int)newValue;
    else if (parameterID == ParameterIDs::scListen) scListenActive = (newValue > 0.5f);
    else if (parameterID == ParameterIDs::bypass) bypassed = (newValue > 0.5f);
    
    if (parameterID != ParameterIDs::scListen && parameterID != ParameterIDs::bypass)
        currentParams = *targetSet;
}

void VCCompAudioProcessor::updateParameters()
{
    currentParams = (activeParamSet == 0) ? paramSetA : paramSetB;
}

//==============================================================================
// Buses Layout
//==============================================================================
bool VCCompAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Require stereo input/output
    if (layouts.getMainInputChannelSet() != AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;
    
    // Sidechain can be stereo or empty (input bus index 1)
    const auto& scSet = layouts.inputBuses[1];
    if (scSet != AudioChannelSet::stereo() && !scSet.isDisabled())
        return false;
    
    return true;
}

//==============================================================================
// Prepare to Play
//==============================================================================
void VCCompAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    
    // Prepare DSP
    dsp.prepare(sampleRate, samplesPerBlock);
    
    // Prepare SC HPFs
    scHPFs.resize(2);
    for (auto& hpf : scHPFs)
        hpf.reset();
    
    // Update parameters
    VCCompDSP::Params p;
    p.threshold = currentParams.threshold;
    p.ratio = currentParams.ratio;
    p.attack = currentParams.attack;
    p.release = currentParams.release;
    p.gain = currentParams.gain;
    p.releaseMode = currentParams.releaseMode;
    p.compBehavior = currentParams.compBehavior;
    p.kneeMode = currentParams.kneeMode;
    p.character = currentParams.character;
    p.mix = currentParams.mix;
    p.trim = currentParams.trim;
    dsp.setParams(p);
    
    // Reset levels
    inputLevelL = inputLevelR = 0.0f;
    outputLevelL = outputLevelR = 0.0f;
    gainReductionDB = 0.0f;
}

//==============================================================================
// Release Resources
//==============================================================================
void VCCompAudioProcessor::releaseResources()
{
    dsp.reset();
}

//==============================================================================
// Process Block
//==============================================================================
void VCCompAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer&)
{
    if (bypassed) return;
    
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();
    
    // SC Listen mode - copy sidechain to output
    if (scListenActive)
    {
        auto scBuffer = getBusBuffer(buffer, true, 1);
        if (scBuffer.getNumChannels() > 0)
        {
            int scCh = scBuffer.getNumChannels();
            for (int ch = 0; ch < numChannels; ++ch)
            {
                int srcCh = (scCh > 0) ? jmin(ch, scCh - 1) : 0;
                buffer.copyFrom(ch, 0, scBuffer, srcCh, 0, numSamples);
            }
            return;
        }
    }
    
    // Determine detection source
    AudioBuffer<float>* detectionBuffer = nullptr;
    bool hasSidechain = false;
    
    if (currentParams.scSource == 1)
    {
        auto scBuffer = getBusBuffer(buffer, true, 1);
        if (scBuffer.getNumChannels() > 0)
        {
            detectionBuffer = &scBuffer;
            hasSidechain = true;
        }
    }
    
    if (!detectionBuffer)
        detectionBuffer = &buffer;
    
    // Update SC HPF
    float hpfFreq = hpfFrequencies[currentParams.scHPF];
    for (int ch = 0; ch < numChannels && ch < (int)scHPFs.size(); ++ch)
        scHPFs[ch].setFrequency(hpfFreq, currentSampleRate);
    
    // Get input levels
    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float inputLevel = std::abs(buffer.getSample(ch, sample));
            if (ch == 0) inputLevelL = jmax(inputLevelL * 0.95f, inputLevel);
            else inputLevelR = jmax(inputLevelR * 0.95f, inputLevel);
        }
    }
    
    // Update DSP params
    VCCompDSP::Params p;
    p.threshold = currentParams.threshold;
    p.ratio = currentParams.ratio;
    p.attack = currentParams.attack;
    p.release = currentParams.release;
    p.gain = currentParams.gain;
    p.releaseMode = currentParams.releaseMode;
    p.compBehavior = currentParams.compBehavior;
    p.kneeMode = currentParams.kneeMode;
    p.character = currentParams.character;
    p.mix = currentParams.mix;
    p.trim = currentParams.trim;
    dsp.setParams(p);
    dsp.setEnabled(!bypassed);
    
    // Process samples with sidechain detection
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Get detection signals (apply HPF if needed)
        std::vector<float> detectionSignal(numChannels);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            int scCh = jmin(ch, detectionBuffer->getNumChannels() - 1);
            float sig = detectionBuffer->getSample(scCh, sample);
            
            if (ch < (int)scHPFs.size() && hpfFreq > 0.0f)
                sig = scHPFs[ch].processSample(sig);
            
            detectionSignal[ch] = sig;
        }
        
        // Calculate RMS detection
        float totalDetection = 0.0f;
        for (float s : detectionSignal)
            totalDetection += s * s;
        totalDetection = std::sqrt(totalDetection / (float)numChannels);
        float detectionDb = linearToDb(totalDetection + 1e-10f);
        
        // Process each channel
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float inputSample = buffer.getSample(ch, sample);
            float drySample = inputSample;
            
            // Calculate envelope for GR
            float envelope = 0.0f;
            for (float s : detectionSignal)
                envelope += s * s;
            envelope = std::sqrt(envelope / (float)numChannels);
            float envelopeDb = linearToDb(envelope + 1e-10f);
            
            // Get effective release time
            float effectiveRelease = currentParams.release;
            
            // Apply ARC if needed
            if (currentParams.releaseMode == 0)
            {
                // ARC calculation would go here
                // For now, use simple adaptive release
                if (envelopeDb > currentParams.threshold + 10.0f)
                    effectiveRelease *= 0.5f;
                else if (envelopeDb < currentParams.threshold - 10.0f)
                    effectiveRelease *= 1.5f;
            }
            
            // Apply behavior-specific release modifiers
            if (currentParams.compBehavior == 0) // Electro
            {
                if (gainReductionDB < 3.0f)
                    effectiveRelease *= 0.3f;
                else
                    effectiveRelease *= 2.0f;
            }
            else // Opto
            {
                if (gainReductionDB < 3.0f)
                    effectiveRelease *= 3.0f;
                else
                    effectiveRelease *= 0.5f;
            }
            
            // Calculate gain reduction
            float kneeWidth = 6.0f;
            if (currentParams.kneeMode == 0) kneeWidth = 0.0f; // Hard
            else if (currentParams.kneeMode == 2) // Auto
                kneeWidth = 6.0f + gainReductionDB * 0.5f;
            
            float targetGR = 0.0f;
            float x = envelopeDb;
            float T = currentParams.threshold;
            float R = currentParams.ratio;
            float halfW = kneeWidth / 2.0f;
            
            if (kneeWidth > 0.0f)
            {
                if (x <= T - halfW)
                    targetGR = 0.0f;
                else if (x >= T + halfW)
                    targetGR = (x - T) * (1.0f - 1.0f / R);
                else
                {
                    float offset = x - (T - halfW);
                    float slope = (1.0f - 1.0f / R) / kneeWidth;
                    targetGR = slope * offset * offset / (2.0f * kneeWidth);
                }
            }
            else
            {
                if (x > T)
                    targetGR = (x - T) * (1.0f - 1.0f / R);
            }
            
            // Smooth GR
            float grCoef = std::exp(-1.0f / (effectiveRelease * 0.001f * currentSampleRate * 0.05f));
            gainReductionDB = gainReductionDB * grCoef + targetGR * (1.0f - grCoef);
            
            // Apply GR
            float grLinear = dBToLinear(-gainReductionDB);
            float wetSample = inputSample * grLinear;
            
            // Apply makeup gain
            wetSample *= dBToLinear(currentParams.gain);
            
            // Wet/dry mix
            float mixFactor = currentParams.mix / 100.0f;
            float outputSample = drySample * (1.0f - mixFactor) + wetSample * mixFactor;
            
            // Apply trim
            outputSample *= dBToLinear(currentParams.trim);
            
            // Limiter
            float absOutput = std::abs(outputSample);
            if (absOutput > 1.0f)
            {
                outputSample = outputSample >= 0.0f ? 1.0f : -1.0f;
                limiterYellow = true;
                limiterRed = true;
            }
            else
            {
                limiterYellow = absOutput > 0.7f;
                limiterRed = false;
            }
            
            buffer.setSample(ch, sample, outputSample);
            
            // Get output levels
            float outputLevel = std::abs(outputSample);
            if (ch == 0) outputLevelL = jmax(outputLevelL * 0.95f, outputLevel);
            else outputLevelR = jmax(outputLevelR * 0.95f, outputLevel);
        }
    }
    
    // Decay levels
    inputLevelL *= 0.995f;
    inputLevelR *= 0.995f;
    outputLevelL *= 0.995f;
    outputLevelR *= 0.995f;
}

//==============================================================================
// Editor
//==============================================================================
AudioProcessorEditor* VCCompAudioProcessor::createEditor() { return new VCCompAudioProcessorEditor(*this); }
bool VCCompAudioProcessor::hasEditor() const { return true; }

//==============================================================================
// Info
//==============================================================================
const String VCCompAudioProcessor::getName() const { return "VC-Comp"; }
bool VCCompAudioProcessor::acceptsMidi() const { return false; }
bool VCCompAudioProcessor::producesMidi() const { return false; }
bool VCCompAudioProcessor::isMidiEffect() const { return false; }
double VCCompAudioProcessor::getTailLengthSeconds() const { return 0.0; }

//==============================================================================
// Programs
//==============================================================================
int VCCompAudioProcessor::getNumPrograms() { return 1; }
int VCCompAudioProcessor::getCurrentProgram() { return 0; }
void VCCompAudioProcessor::setCurrentProgram(int) {}
const String VCCompAudioProcessor::getProgramName(int) { return "Default"; }
void VCCompAudioProcessor::changeProgramName(int, const String&) {}

//==============================================================================
// State Information
//==============================================================================
void VCCompAudioProcessor::getStateInformation(MemoryBlock& destData)
{
    std::unique_ptr<XmlElement> xml(new XmlElement("VC-Comp"));
    
    auto* setA = xml->createNewChildElement("ParamSetA");
    setA->setAttribute("threshold", paramSetA.threshold);
    setA->setAttribute("ratio", paramSetA.ratio);
    setA->setAttribute("attack", paramSetA.attack);
    setA->setAttribute("release", paramSetA.release);
    setA->setAttribute("gain", paramSetA.gain);
    setA->setAttribute("releaseMode", paramSetA.releaseMode);
    setA->setAttribute("compBehavior", paramSetA.compBehavior);
    setA->setAttribute("kneeMode", paramSetA.kneeMode);
    setA->setAttribute("character", paramSetA.character);
    setA->setAttribute("mix", paramSetA.mix);
    setA->setAttribute("trim", paramSetA.trim);
    setA->setAttribute("scSource", paramSetA.scSource);
    setA->setAttribute("scHPF", paramSetA.scHPF);
    
    auto* setB = xml->createNewChildElement("ParamSetB");
    setB->setAttribute("threshold", paramSetB.threshold);
    setB->setAttribute("ratio", paramSetB.ratio);
    setB->setAttribute("attack", paramSetB.attack);
    setB->setAttribute("release", paramSetB.release);
    setB->setAttribute("gain", paramSetB.gain);
    setB->setAttribute("releaseMode", paramSetB.releaseMode);
    setB->setAttribute("compBehavior", paramSetB.compBehavior);
    setB->setAttribute("kneeMode", paramSetB.kneeMode);
    setB->setAttribute("character", paramSetB.character);
    setB->setAttribute("mix", paramSetB.mix);
    setB->setAttribute("trim", paramSetB.trim);
    setB->setAttribute("scSource", paramSetB.scSource);
    setB->setAttribute("scHPF", paramSetB.scHPF);
    
    xml->setAttribute("activeParamSet", activeParamSet);
    
    copyXmlToBinary(*xml, destData);
}

void VCCompAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = XmlDocument::parse(String((const char*)data, sizeInBytes));
    if (!xml) return;
    
    if (auto* setA = xml->getChildByName("ParamSetA"))
    {
        paramSetA.threshold = (float)setA->getDoubleAttribute("threshold", 0.0);
        paramSetA.ratio = (float)setA->getDoubleAttribute("ratio", 1.0);
        paramSetA.attack = (float)setA->getDoubleAttribute("attack", 16.0);
        paramSetA.release = (float)setA->getDoubleAttribute("release", 160.0);
        paramSetA.gain = (float)setA->getDoubleAttribute("gain", 0.0);
        paramSetA.releaseMode = setA->getIntAttribute("releaseMode", 0);
        paramSetA.compBehavior = setA->getIntAttribute("compBehavior", 0);
        paramSetA.kneeMode = setA->getIntAttribute("kneeMode", 1);
        paramSetA.character = setA->getIntAttribute("character", 1);
        paramSetA.mix = (float)setA->getDoubleAttribute("mix", 100.0);
        paramSetA.trim = (float)setA->getDoubleAttribute("trim", 0.0);
        paramSetA.scSource = setA->getIntAttribute("scSource", 0);
        paramSetA.scHPF = setA->getIntAttribute("scHPF", 0);
    }
    
    if (auto* setB = xml->getChildByName("ParamSetB"))
    {
        paramSetB.threshold = (float)setB->getDoubleAttribute("threshold", 0.0);
        paramSetB.ratio = (float)setB->getDoubleAttribute("ratio", 1.0);
        paramSetB.attack = (float)setB->getDoubleAttribute("attack", 16.0);
        paramSetB.release = (float)setB->getDoubleAttribute("release", 160.0);
        paramSetB.gain = (float)setB->getDoubleAttribute("gain", 0.0);
        paramSetB.releaseMode = setB->getIntAttribute("releaseMode", 0);
        paramSetB.compBehavior = setB->getIntAttribute("compBehavior", 0);
        paramSetB.kneeMode = setB->getIntAttribute("kneeMode", 1);
        paramSetB.character = setB->getIntAttribute("character", 1);
        paramSetB.mix = (float)setB->getDoubleAttribute("mix", 100.0);
        paramSetB.trim = (float)setB->getDoubleAttribute("trim", 0.0);
        paramSetB.scSource = setB->getIntAttribute("scSource", 0);
        paramSetB.scHPF = setB->getIntAttribute("scHPF", 0);
    }
    
    activeParamSet = xml->getIntAttribute("activeParamSet", 0);
    currentParams = (activeParamSet == 0) ? paramSetA : paramSetB;
}

//==============================================================================
// Plugin Entry Point
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCCompAudioProcessor();
}
