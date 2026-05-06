#pragma once

// JUCE 模块头文件
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <cmath>
#include "DSP/VCSmoothDSP.h"

//==============================================================================
// VC-Smooth 参数 ID
namespace ParameterIDs
{
    // 核心参数
    static const juce::String depth{"depth"};
    static const juce::String speed{"speed"};
    static const juce::String freqLow{"freqLow"};
    static const juce::String freqHigh{"freqHigh"};
    static const juce::String sharpness{"sharpness"};
    static const juce::String mix{"mix"};
    
    // 辅助参数
    static const juce::String inputGain{"inputGain"};
    static const juce::String outputGain{"outputGain"};
    static const juce::String bypass{"bypass"};
    static const juce::String paramSet{"paramSet"};
}

//==============================================================================
class VCSmoothProcessor : public juce::AudioProcessor, 
                          public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    VCSmoothProcessor();
    ~VCSmoothProcessor();

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "VC-Smooth"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override { 
        return static_cast<double>(VCSmoothConfig::kLatency) / 44100.0; 
    }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    //==============================================================================
    // 参数监听
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    
    //==============================================================================
    // 获取 AudioProcessorValueTreeState
    juce::AudioProcessorValueTreeState& getAPVTS() { return mAPVTS; }
    
    //==============================================================================
    // 获取 DSP 对象
    VCSmoothDSP& getDSP() { return mDSP; }

private:
    //==============================================================================
    // 创建参数布局
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    //==============================================================================
    juce::AudioProcessorValueTreeState mAPVTS;
    
    // DSP 核心
    VCSmoothDSP mDSP;
    
    // Bypass 状态
    bool mBypass = false;
    
    // A/B 参数组
    int mParamSet = 0;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCSmoothProcessor)
};
