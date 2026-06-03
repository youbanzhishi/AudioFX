// ============================================================================
// VC-SpaceMaker CLI - JUCE-based command line tool
// Processes WAV file with dynamic frequency avoidance
// ============================================================================

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/VCSpaceMakerDSP.h"

int main (int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "VC-SpaceMaker v1.0.0 - Dynamic Frequency Avoidance CLI" << std::endl;
        std::cout << "对标 Wavesfactory TrackSpacer" << std::endl;
        std::cout << std::endl;
        std::cout << "Usage: VC-SpaceMaker-CLI <input.wav> <output.wav> [amount] [attack_ms] [release_ms]" << std::endl;
        std::cout << "  amount:   0.0-1.0 (default: 0.5)" << std::endl;
        std::cout << "  attack:   0.1-200ms (default: 5.0)" << std::endl;
        std::cout << "  release:  1-2000ms (default: 50.0)" << std::endl;
        return 1;
    }

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    // 读取输入文件
    juce::File inputFile (argv[1]);
    auto reader = std::unique_ptr<juce::AudioFormatReader> (formatManager.createReaderFor (inputFile));

    if (! reader)
    {
        std::cerr << "Error: Cannot read input file: " << argv[1] << std::endl;
        return 1;
    }

    double sampleRate = reader->sampleRate;
    int numChannels = reader->numChannels;
    int numSamples = (int) reader->lengthInSamples;

    std::cout << "Input: " << argv[1] << std::endl;
    std::cout << "  Sample Rate: " << sampleRate << std::endl;
    std::cout << "  Channels: " << numChannels << std::endl;
    std::cout << "  Duration: " << (numSamples / sampleRate) << "s" << std::endl;

    // 读取音频数据
    juce::AudioBuffer<float> buffer (numChannels, numSamples);
    reader->read (&buffer, 0, numSamples, 0, true, true);

    // 设置参数
    float amount = (argc > 3) ? std::atof (argv[3]) : 0.5f;
    float attack = (argc > 4) ? std::atof (argv[4]) : 5.0f;
    float release = (argc > 5) ? std::atof (argv[5]) : 50.0f;

    VCSpaceMakerDSP dsp;
    VCSpaceMakerDSP::Parameters params;
    params.amount = amount;
    params.attackMs = attack;
    params.releaseMs = release;
    dsp.setParameters (params);
    dsp.prepareToPlay (sampleRate, 512);

    std::cout << "Processing with amount=" << amount
              << " attack=" << attack << "ms"
              << " release=" << release << "ms" << std::endl;

    // 处理（使用自身作为侧链 - 演示模式）
    int blockSize = 512;
    for (int start = 0; start < numSamples; start += blockSize)
    {
        int samplesThisBlock = std::min (blockSize, numSamples - start);

        float* channelL = buffer.getWritePointer (0) + start;
        float* channelR = (numChannels > 1) ? buffer.getWritePointer (1) + start : channelL;

        // 自侧链模式：用自身信号作为侧链输入
        float scL[512], scR[512];
        for (int i = 0; i < samplesThisBlock; i++)
        {
            scL[i] = channelL[i];
            scR[i] = channelR[i];
        }

        dsp.processBlock (channelL, channelR, scL, scR, samplesThisBlock);
    }

    // 写入输出文件
    juce::File outputFile (argv[2]);
    outputFile.deleteFile();

    juce::WavAudioFormat wavFormat;
    auto writer = std::unique_ptr<juce::AudioFormatWriter> (
        wavFormat.createWriterFor (new juce::FileOutputStream (outputFile), sampleRate,
                                   numChannels, 16, {}, 0));

    if (! writer)
    {
        std::cerr << "Error: Cannot create output file: " << argv[2] << std::endl;
        return 1;
    }

    writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);

    std::cout << "Output: " << argv[2] << std::endl;
    std::cout << "Done!" << std::endl;

    return 0;
}
