#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
VCEQProcessor::VCEQProcessor()
    : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  AudioChannelSet::stereo())
                       .withOutput ("Output", AudioChannelSet::stereo()))
    , mAPVTS (*this, nullptr, Identifier ("VCEQParameters"), createParameterLayout())
{
    // 注册自身为参数监听器
    mAPVTS.addParameterListener (ParameterIDs::bypass, this);
    mAPVTS.addParameterListener (ParameterIDs::inputGain, this);
    mAPVTS.addParameterListener (ParameterIDs::outputGain, this);
    
    for (int i = 0; i < EQBands::kNumBands; ++i)
    {
        mAPVTS.addParameterListener (ParameterIDs::getBandParam(i, ParameterIDs::freqSuffix), this);
        mAPVTS.addParameterListener (ParameterIDs::getBandParam(i, ParameterIDs::qSuffix), this);
        mAPVTS.addParameterListener (ParameterIDs::getBandParam(i, ParameterIDs::gainSuffix), this);
        mAPVTS.addParameterListener (ParameterIDs::getBandParam(i, ParameterIDs::typeSuffix), this);
        mAPVTS.addParameterListener (ParameterIDs::getBandParam(i, ParameterIDs::enabledSuffix), this);
        mAPVTS.addParameterListener (ParameterIDs::getBandParam(i, ParameterIDs::soloSuffix), this);
    }
}

VCEQProcessor::~VCEQProcessor()
{
}

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout VCEQProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    
    // 输入增益 (-24dB to +24dB)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::inputGain, "Input Gain",
        NormalisableRange<float> (-24.0f, 24.0f), 0.0f));
    
    // 输出增益 (-24dB to +24dB)
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterIDs::outputGain, "Output Gain",
        NormalisableRange<float> (-24.0f, 24.0f), 0.0f));
    
    // Bypass
    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterIDs::bypass, "Bypass", false));
    
    // A/B切换
    params.push_back (std::make_unique<AudioParameterInt> (
        ParameterIDs::abSwitch, "A/B", 0, 1, 0));
    
    // 5个频段的参数
    for (int i = 0; i < EQBands::kNumBands; ++i)
    {
        String bandName;
        switch (i)
        {
            case EQBands::LowShelf:   bandName = "Low Shelf"; break;
            case EQBands::LowMid:     bandName = "Low Mid"; break;
            case EQBands::HighMid:    bandName = "High Mid"; break;
            case EQBands::HighShelf:  bandName = "High Shelf"; break;
            default:                  bandName = "Mid"; break;
        }
        
        // 频率 (20Hz - 20kHz, 对数)
        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterIDs::getBandParam(i, ParameterIDs::freqSuffix),
            bandName + " Freq",
            NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f),
            EQBands::kDefaultFrequencies[i]));
        
        // Q值 (0.1 - 10)
        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterIDs::getBandParam(i, ParameterIDs::qSuffix),
            bandName + " Q",
            NormalisableRange<float> (0.1f, 10.0f), EQBands::kDefaultQ[i]));
        
        // 增益 (-24dB to +24dB)
        params.push_back (std::make_unique<AudioParameterFloat> (
            ParameterIDs::getBandParam(i, ParameterIDs::gainSuffix),
            bandName + " Gain",
            NormalisableRange<float> (-24.0f, 24.0f), EQBands::kDefaultGains[i]));
        
        // 滤波器类型 (0=LowShelf, 1=HighShelf, 2=Parametric)
        params.push_back (std::make_unique<AudioParameterInt> (
            ParameterIDs::getBandParam(i, ParameterIDs::typeSuffix),
            bandName + " Type", 0, 2, (i == EQBands::LowShelf || i == EQBands::HighShelf) ? i : 2));
        
        // 启用开关
        params.push_back (std::make_unique<AudioParameterBool> (
            ParameterIDs::getBandParam(i, ParameterIDs::enabledSuffix),
            bandName + " Enabled", true));
        
        // Solo开关
        params.push_back (std::make_unique<AudioParameterBool> (
            ParameterIDs::getBandParam(i, ParameterIDs::soloSuffix),
            bandName + " Solo", false));
    }
    
    // 预设选择
    params.push_back (std::make_unique<AudioParameterInt> (
        ParameterIDs::preset, "Preset", 0, 3, 0));
    
    return { params.begin(), params.end() };
}

//==============================================================================
void VCEQProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    mProcessSpec.sampleRate = sampleRate;
    mProcessSpec.maximumBlockSize = samplesPerBlock;
    mProcessSpec.numChannels = getMainBusNumOutputChannels();
    
    // 准备所有滤波器
    for (auto& band : mBands)
    {
        band.filter.prepare (mProcessSpec);
    }
    
    updateAllFilters();
}

void VCEQProcessor::releaseResources()
{
    for (auto& band : mBands)
    {
        band.filter.reset();
    }
}

//==============================================================================
bool VCEQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // 只支持立体声
    return layouts.getMainInputChannelSet() == AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

//==============================================================================
void VCEQProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    if (mBypass)
        return;
    
    // 应用输入增益
    buffer.applyGain (mInputGain);
    
    // 获取音频块
    dsp::AudioBlock<float> block (buffer);
    
    // 检查是否有任何频段被Solo
    bool anySolo = false;
    for (int i = 0; i < EQBands::kNumBands; ++i)
    {
        if (mBands[i].solo)
        {
            anySolo = true;
            break;
        }
    }
    
    // 依次通过各频段
    for (int i = 0; i < EQBands::kNumBands; ++i)
    {
        // Solo逻辑：如果有Solo且当前频段未Solo，跳过
        if (anySolo && !mBands[i].solo)
            continue;
        
        // 如果频段未启用，跳过
        if (!mBands[i].enabled)
            continue;
        
        // 处理该频段
        dsp::ProcessContextReplacing<float> context (block);
        mBands[i].filter.process (context);
    }
    
    // 应用输出增益
    buffer.applyGain (mOutputGain);
}

//==============================================================================
void VCEQProcessor::parameterChanged (const String& parameterID, float newValue)
{
    if (parameterID == ParameterIDs::bypass)
    {
        mBypass = newValue > 0.5f;
    }
    else if (parameterID == ParameterIDs::inputGain)
    {
        mInputGain = Decibels::decibelsToGain (newValue);
    }
    else if (parameterID == ParameterIDs::outputGain)
    {
        mOutputGain = Decibels::decibelsToGain (newValue);
    }
    else
    {
        // 解析频段参数
        for (int i = 0; i < EQBands::kNumBands; ++i)
        {
            if (parameterID == ParameterIDs::getBandParam(i, ParameterIDs::freqSuffix))
            {
                mBands[i].frequency = newValue;
                updateFilterCoefficients(i);
                break;
            }
            else if (parameterID == ParameterIDs::getBandParam(i, ParameterIDs::qSuffix))
            {
                mBands[i].q = newValue;
                updateFilterCoefficients(i);
                break;
            }
            else if (parameterID == ParameterIDs::getBandParam(i, ParameterIDs::gainSuffix))
            {
                mBands[i].gainDB = newValue;
                updateFilterCoefficients(i);
                break;
            }
            else if (parameterID == ParameterIDs::getBandParam(i, ParameterIDs::typeSuffix))
            {
                mBands[i].type = static_cast<FilterType> (static_cast<int> (newValue));
                updateFilterCoefficients(i);
                break;
            }
            else if (parameterID == ParameterIDs::getBandParam(i, ParameterIDs::enabledSuffix))
            {
                mBands[i].enabled = newValue > 0.5f;
                break;
            }
            else if (parameterID == ParameterIDs::getBandParam(i, ParameterIDs::soloSuffix))
            {
                mBands[i].solo = newValue > 0.5f;
                handleSoloChange();
                break;
            }
        }
    }
}

void VCEQProcessor::handleSoloChange()
{
    // 可以在这里添加Solo互斥逻辑（目前允许多个频段同时Solo）
}

//==============================================================================
void VCEQProcessor::updateFilterCoefficients(int band)
{
    auto& b = mBands[band];
    
    // 检查参数是否变化
    if (mCachedParams[band].freq == b.frequency &&
        mCachedParams[band].q == b.q &&
        mCachedParams[band].gainDB == b.gainDB &&
        mCachedParams[band].enabled == b.enabled)
    {
        return; // 参数未变化，不需要更新
    }
    
    // 缓存新值
    mCachedParams[band].freq = b.frequency;
    mCachedParams[band].q = b.q;
    mCachedParams[band].gainDB = b.gainDB;
    mCachedParams[band].enabled = b.enabled;
    
    // 根据滤波器类型计算系数
    // gainFactor = Decibels to linear gain
    float gainFactor = Decibels::decibelsToGain (b.gainDB);
    
    if (b.type == LowShelf)
    {
        // Low Shelf filter
        b.filter.coefficients = dsp::IIR::Coefficients<float>::makeLowShelf (
            mProcessSpec.sampleRate,
            b.frequency,
            b.q,
            gainFactor);
    }
    else if (b.type == HighShelf)
    {
        // High Shelf filter
        b.filter.coefficients = dsp::IIR::Coefficients<float>::makeHighShelf (
            mProcessSpec.sampleRate,
            b.frequency,
            b.q,
            gainFactor);
    }
    else
    {
        // Peak/Parametric filter (default)
        b.filter.coefficients = dsp::IIR::Coefficients<float>::makePeakFilter (
            mProcessSpec.sampleRate,
            b.frequency,
            b.q,
            gainFactor);
    }
}

void VCEQProcessor::updateAllFilters()
{
    for (int i = 0; i < EQBands::kNumBands; ++i)
    {
        updateFilterCoefficients(i);
    }
}

//==============================================================================
void VCEQProcessor::getStateInformation (MemoryBlock& destData)
{
    // 保存当前状态
    auto state = mAPVTS.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    if (xml != nullptr)
    {
        // 使用 MemoryOutputStream 写入 XML
        MemoryOutputStream mos (destData, true);
        xml->writeTo (mos, {});
    }
}

void VCEQProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // 恢复状态 - 使用新的 API
    auto xmlState = parseXML (String ((const char*)data, sizeInBytes));
    if (xmlState.get() != nullptr)
        mAPVTS.replaceState (ValueTree::fromXml (*xmlState));
}

//==============================================================================
// 编辑器创建
AudioProcessorEditor* VCEQProcessor::createEditor()
{
    return new VCEQEditor (*this);
}

//==============================================================================
// 插件入口点
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VCEQProcessor();
}
