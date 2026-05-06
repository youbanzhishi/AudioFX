#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
// 构造函数
//==============================================================================
VCSmoothProcessor::VCSmoothProcessor()
    : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  AudioChannelSet::stereo())
                       .withOutput ("Output", AudioChannelSet::stereo()))
    , mAPVTS (*this, nullptr, Identifier ("VCSmoothParameters"), createParameterLayout())
{
    // 注册参数监听器
    mAPVTS.addParameterListener (ParameterIDs::bypass, this);
    mAPVTS.addParameterListener (ParameterIDs::depth, this);
    mAPVTS.addParameterListener (ParameterIDs::speed, this);
    mAPVTS.addParameterListener (ParameterIDs::freqLow, this);
    mAPVTS.addParameterListener (ParameterIDs::freqHigh, this);
    mAPVTS.addParameterListener (ParameterIDs::sharpness, this);
    mAPVTS.addParameterListener (ParameterIDs::mix, this);
    mAPVTS.addParameterListener (ParameterIDs::inputGain, this);
    mAPVTS.addParameterListener (ParameterIDs::outputGain, this);
    
    // 初始化 DSP 参数
    VCSmoothDSP::Params params;
    params.depth = 0.5f;
    params.speed = 2.0f;
    params.freqLow = 200.0f;
    params.freqHigh = 16000.0f;
    params.sharpness = 1.5f;
    params.mix = 1.0f;
    params.inputGain = 0.0f;
    params.outputGain = 0.0f;
    mDSP.setParams(params);
}

VCSmoothProcessor::~VCSmoothProcessor()
{
}

//==============================================================================
// 参数布局
//==============================================================================
AudioProcessorValueTreeState::ParameterLayout VCSmoothProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    
    // ========== 核心参数 ==========
    
    // Depth (0-1, default 0.5)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::depth, "Depth",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    
    // Speed (0.1-10, default 2.0)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::speed, "Speed",
        NormalisableRange<float> (0.1f, 10.0f, 0.1f), 2.0f));
    
    // Freq Low (20-20000 Hz, default 200Hz)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::freqLow, "Freq Low",
        NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f), 200.0f,
        "Hz"));
    
    // Freq High (20-20000 Hz, default 16000Hz)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::freqHigh, "Freq High",
        NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f), 16000.0f,
        "Hz"));
    
    // Sharpness (0.1-5.0, default 1.5)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::sharpness, "Sharpness",
        NormalisableRange<float> (0.1f, 5.0f, 0.1f), 1.5f));
    
    // Mix (0-1, default 1.0)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::mix, "Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
    
    // ========== 辅助参数 ==========
    
    // Input Gain (-24 to +24 dB)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::inputGain, "Input",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        "dB"));
    
    // Output Gain (-24 to +24 dB)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::outputGain, "Output",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        "dB"));
    
    // Bypass
    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterIDs::bypass, "Bypass", false));
    
    // A/B 参数组切换
    params.push_back (std::make_unique<AudioParameterInt> (
        ParameterIDs::paramSet, "A/B", 0, 1, 0));
    
    return { params.begin(), params.end() };
}

//==============================================================================
// 准备播放
//==============================================================================
void VCSmoothProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mDSP.prepare(sampleRate, samplesPerBlock);
    setLatencySamples(mDSP.getLatencySamples());
}

void VCSmoothProcessor::releaseResources()
{
    mDSP.reset();
}

//==============================================================================
// 总线布局检查
//==============================================================================
bool VCSmoothProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

//==============================================================================
// 主处理块
//==============================================================================
void VCSmoothProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    if (mBypass)
        return;
    
    int numSamples = buffer.getNumSamples();
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getWritePointer(1);
    
    // 使用 DSP 类处理
    mDSP.process(leftChannel, rightChannel, numSamples);
}

//==============================================================================
// 参数变化回调
//==============================================================================
void VCSmoothProcessor::parameterChanged (const String& parameterID, float newValue)
{
    if (parameterID == ParameterIDs::bypass)
    {
        mBypass = newValue > 0.5f;
    }
    else if (parameterID == ParameterIDs::depth ||
             parameterID == ParameterIDs::speed ||
             parameterID == ParameterIDs::freqLow ||
             parameterID == ParameterIDs::freqHigh ||
             parameterID == ParameterIDs::sharpness ||
             parameterID == ParameterIDs::mix ||
             parameterID == ParameterIDs::inputGain ||
             parameterID == ParameterIDs::outputGain)
    {
        // 更新 DSP 参数
        VCSmoothDSP::Params params = mDSP.getParams();
        
        params.depth = mAPVTS.getRawParameterValue(ParameterIDs::depth)->load();
        params.speed = mAPVTS.getRawParameterValue(ParameterIDs::speed)->load();
        params.freqLow = mAPVTS.getRawParameterValue(ParameterIDs::freqLow)->load();
        params.freqHigh = mAPVTS.getRawParameterValue(ParameterIDs::freqHigh)->load();
        params.sharpness = mAPVTS.getRawParameterValue(ParameterIDs::sharpness)->load();
        params.mix = mAPVTS.getRawParameterValue(ParameterIDs::mix)->load();
        params.inputGain = mAPVTS.getRawParameterValue(ParameterIDs::inputGain)->load();
        params.outputGain = mAPVTS.getRawParameterValue(ParameterIDs::outputGain)->load();
        
        mDSP.setParams(params);
    }
    else if (parameterID == ParameterIDs::paramSet)
    {
        mParamSet = static_cast<int>(newValue);
    }
}

//==============================================================================
// 状态保存/恢复
//==============================================================================
void VCSmoothProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    if (xml != nullptr)
    {
        MemoryOutputStream mos (destData, true);
        xml->writeTo (mos, {});
    }
}

void VCSmoothProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xmlState = parseXML (String ((const char*)data, sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState (ValueTree::fromXml (*xmlState));
}

//==============================================================================
// 创建编辑器
//==============================================================================
AudioProcessorEditor* VCSmoothProcessor::createEditor()
{
    return new VCSmoothEditor (*this);
}

//==============================================================================
// 插件入口点
//==============================================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCSmoothProcessor();
}
