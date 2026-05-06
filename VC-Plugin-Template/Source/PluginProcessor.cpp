#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// ============================================================
// 构造函数
// ============================================================
[[PLUGIN_NAME]]Processor::[[PLUGIN_NAME]]Processor()
    : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  AudioChannelSet::stereo())
                       .withOutput ("Output", AudioChannelSet::stereo()))
    , mAPVTS (*this, nullptr, Identifier ("[[PLUGIN_NAME]]Parameters"), createParameterLayout())
{
    // ============================================================
    // TODO: 注册参数监听器
    // mAPVTS.addParameterListener (ParameterIDs::bypass, this);
    // mAPVTS.addParameterListener (ParameterIDs::gain, this);
    // ============================================================
    
    // ============================================================
    // TODO: 初始化 DSP 相关成员
    // ============================================================
}

[[PLUGIN_NAME]]Processor::~[[PLUGIN_NAME]]Processor()
{
}

// ============================================================
// 参数布局
// ============================================================
AudioProcessorValueTreeState::ParameterLayout [[PLUGIN_NAME]]Processor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    
    // ============================================================
    // TODO: 定义插件参数
    // 
    // 常用参数类型:
    // - AudioParameterFloat: 浮点参数
    // - AudioParameterInt: 整数参数
    // - AudioParameterBool: 布尔参数
    // - AudioParameterChoice: 枚举参数
    //
    // 示例:
    // params.push_back (std::make_unique<AudioParameterFloat> (
    //     ParameterIDs::gain, "Gain",
    //     NormalisableRange<float> (-24.0f, 24.0f), 0.0f,
    //     "dB"));
    //
    // params.push_back (std::make_unique<AudioParameterBool> (
    //     ParameterIDs::bypass, "Bypass", false));
    // ============================================================
    
    // Bypass 参数（通用）
    params.push_back (std::make_unique<AudioParameterBool> (
        "bypass", "Bypass", false));
    
    return { params.begin(), params.end() };
}

// ============================================================
// 准备播放
// ============================================================
void [[PLUGIN_NAME]]Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = samplesPerBlock;
    mProcessSpec.numChannels = getMainBusNumOutputChannels();
    
    // ============================================================
    // TODO: 准备 DSP 处理模块
    // 示例: 
    // - 准备滤波器
    // - 初始化 FFT
    // - 重置缓冲区
    // ============================================================
}

void [[PLUGIN_NAME]]Processor::releaseResources()
{
    // ============================================================
    // TODO: 释放 DSP 资源
    // ============================================================
}

// ============================================================
// 总线布局检查
// ============================================================
bool [[PLUGIN_NAME]]Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // ============================================================
    // TODO: 根据需要修改总线支持条件
    // ============================================================
    
    // 默认：只支持立体声
    return layouts.getMainInputChannelSet() == AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

// ============================================================
// 处理音频块
// ============================================================
void [[PLUGIN_NAME]]Processor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    // ============================================================
    // TODO: 实现 DSP 算法
    // ============================================================
    
    if (mBypass)
        return;
    
    // 获取音频数据
    int numSamples = buffer.getNumSamples();
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getWritePointer(1);
    
    // ============================================================
    // 示例 DSP 处理框架:
    //
    // 1. 输入处理（如需要）
    // for (int i = 0; i < numSamples; ++i) {
    //     leftChannel[i] *= inputGain;
    //     rightChannel[i] *= inputGain;
    // }
    //
    // 2. 主 DSP 算法
    // 示例: processDSP(buffer);
    //
    // 3. 输出处理（如需要）
    // for (int i = 0; i < numSamples; ++i) {
    //     leftChannel[i] *= outputGain;
    //     rightChannel[i] *= outputGain;
    // }
    // ============================================================
}

// ============================================================
// 参数变化回调
// ============================================================
void [[PLUGIN_NAME]]Processor::parameterChanged (const String& parameterID, float newValue)
{
    // ============================================================
    // TODO: 处理参数变化
    //
    // 示例:
    // if (parameterID == ParameterIDs::bypass)
    // {
    //     mBypass = newValue > 0.5f;
    // }
    // else if (parameterID == ParameterIDs::gain)
    // {
    //     mGain = Decibels::decibelsToGain(newValue);
    // }
    // ============================================================
    
    if (parameterID == "bypass")
    {
        mBypass = newValue > 0.5f;
    }
}

// ============================================================
// DSP 处理方法示例（需根据插件功能实现）
// ============================================================
#if 0
void [[PLUGIN_NAME]]Processor::processDSP(AudioBuffer<float>& buffer)
{
    // TODO: 实现具体的 DSP 算法
    // 可以是: FFT 频谱处理、滤波器组、动态处理等
}
#endif

// ============================================================
// 状态保存
// ============================================================
void [[PLUGIN_NAME]]Processor::getStateInformation (MemoryBlock& destData)
{
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    if (xml != nullptr)
    {
        MemoryOutputStream mos (destData, true);
        xml->writeTo (mos, {});
    }
}

// ============================================================
// 状态恢复
// ============================================================
void [[PLUGIN_NAME]]Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xmlState = parseXML (String ((const char*)data, sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState (ValueTree::fromXml (*xmlState));
}

// ============================================================
// 创建编辑器
// ============================================================
AudioProcessorEditor* [[PLUGIN_NAME]]Processor::createEditor()
{
    return new [[PLUGIN_NAME]]Editor (*this);
}

// ============================================================
// 插件入口点
// ============================================================
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new [[PLUGIN_NAME]]Processor();
}
