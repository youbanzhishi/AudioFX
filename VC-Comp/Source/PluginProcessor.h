#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include "DSP/VCCompDSP.h"

namespace ParameterIDs
{
    static const juce::String threshold {"threshold"};
    static const juce::String ratio {"ratio"};
    static const juce::String attack {"attack"};
    static const juce::String release {"release"};
    static const juce::String gain {"gain"};
    static const juce::String releaseMode {"releaseMode"};
    static const juce::String compBehavior {"compBehavior"};
    static const juce::String kneeMode {"kneeMode"};
    static const juce::String character {"character"};
    static const juce::String mix {"mix"};
    static const juce::String trim {"trim"};
    static const juce::String scSource {"scSource"};
    static const juce::String scHPF {"scHPF"};
    static const juce::String scListen {"scListen"};
    static const juce::String bypass {"bypass"};
    static const juce::String paramSet {"paramSet"};
}

class VCCompAudioProcessor : public juce::AudioProcessor,
                              public juce::AudioProcessorValueTreeState::Listener
{
public:
    VCCompAudioProcessor();
    ~VCCompAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts {*this, nullptr, "VC-Comp", createParameterLayout()};
    
    float getInputLevelL() const { return inputLevelL; }
    float getInputLevelR() const { return inputLevelR; }
    float getOutputLevelL() const { return outputLevelL; }
    float getOutputLevelR() const { return outputLevelR; }
    float getGainReduction() const { return gainReductionDB; }
    bool isLimiterYellow() const { return limiterYellow; }
    bool isLimiterRed() const { return limiterRed; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void updateParameters();
    
    struct ParamSet
    {
        float threshold = 0.0f;
        float ratio = 1.0f;
        float attack = 16.0f;
        float release = 160.0f;
        float gain = 0.0f;
        int releaseMode = 0;
        int compBehavior = 0;
        int kneeMode = 1;
        int character = 1;
        float mix = 100.0f;
        float trim = 0.0f;
        int scSource = 0;
        int scHPF = 0;
    };
    
    ParamSet paramSetA, paramSetB, currentParams;
    int activeParamSet = 0;
    
    // Use DSP class
    VCCompDSP dsp;
    
    bool scListenActive = false;
    std::vector<SCHPF> scHPFs;
    
    float inputLevelL = 0.0f, inputLevelR = 0.0f;
    float outputLevelL = 0.0f, outputLevelR = 0.0f;
    float gainReductionDB = 0.0f;
    bool limiterYellow = false;
    bool limiterRed = false;
    
    double currentSampleRate = 44100.0;
    bool bypassed = false;
    
    const float hpfFrequencies[5] = {0.0f, 60.0f, 100.0f, 200.0f, 500.0f};
};

// Sidechain HPF class (used by VST3 plugin only)
class SCHPF
{
public:
    void setFrequency(float freqHz, float sampleRate)
    {
        if (freqHz <= 0.0f)
        {
            b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
            a1 = 0.0f; a2 = 0.0f;
            return;
        }
        
        float omega = 2.0f * juce::MathConstants<float>::pi * freqHz / sampleRate;
        float sinOmega = std::sin(omega);
        float cosOmega = std::cos(omega);
        float alpha = sinOmega / (2.0f * 1.4142f);
        
        b0 = (1.0f + cosOmega) / 2.0f;
        b1 = -(1.0f + cosOmega);
        b2 = (1.0f + cosOmega) / 2.0f;
        float a0 = 1.0f + alpha;
        a1 = -2.0f * cosOmega;
        a2 = 1.0f - alpha;
        
        b0 /= a0; b1 /= a0; b2 /= a0;
        a1 /= a0; a2 /= a0;
    }
    
    void reset() { x1 = x2 = y1 = y2 = 0.0f; }
    
    float processSample(float x)
    {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
    
private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
};
