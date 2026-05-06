// VC-Smooth CLI - 命令行共振峰平滑工具
// 用法: VC-Smooth-CLI input.wav output.wav [选项]

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../DSP/VCSmoothDSP.h"

void printHelp(const char* progName) {
    std::cout << "VC-Smooth CLI - 频谱共振峰平滑工具\n\n";
    std::cout << "用法: " << progName << " <input.wav> <output.wav> [选项]\n\n";
    std::cout << "选项:\n";
    std::cout << "  --help, -h              显示帮助\n";
    std::cout << "  --depth <0-1>           平滑深度 (默认: 0.5)\n";
    std::cout << "  --speed <0.1-10>        包络跟踪速度 (默认: 2.0)\n";
    std::cout << "  --freq-low <Hz>         低频边界 (默认: 200)\n";
    std::cout << "  --freq-high <Hz>        高频边界 (默认: 16000)\n";
    std::cout << "  --sharpness <0.1-5>     锐度/阈值系数 (默认: 1.5)\n";
    std::cout << "  --mix <0-1>             干湿比 (默认: 1.0)\n";
    std::cout << "  --input-gain <dB>       输入增益 (默认: 0)\n";
    std::cout << "  --output-gain <dB>      输出增益 (默认: 0)\n\n";
    std::cout << "示例:\n";
    std::cout << "  " << progName << " in.wav out.wav\n";
    std::cout << "  " << progName << " in.wav out.wav --depth 0.7 --speed 3.0\n";
    std::cout << "  " << progName << " in.wav out.wav --freq-low 500 --freq-high 8000\n";
}

// 解析命令行参数
std::map<std::string, std::string> parseArgs(int argc, char** argv) {
    std::map<std::string, std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            args["--help"] = "";
        } else if (arg.substr(0, 2) == "--") {
            std::string key = arg;
            std::string value;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                value = argv[++i];
            }
            args[key] = value;
        }
    }
    return args;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printHelp(argv[0]);
        return 1;
    }
    
    auto args = parseArgs(argc, argv);
    
    if (args.count("--help")) {
        printHelp(argv[0]);
        return 0;
    }
    
    // 获取输入输出文件
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] != '-') {
            files.push_back(arg);
        }
    }
    
    if (files.size() < 2) {
        std::cerr << "错误: 需要指定输入和输出文件\n\n";
        printHelp(argv[0]);
        return 1;
    }
    
    std::string inputFile = files[0];
    std::string outputFile = files[1];
    
    std::cout << "VC-Smooth CLI\n";
    std::cout << "输入: " << inputFile << "\n";
    std::cout << "输出: " << outputFile << "\n";
    
    // 解析参数
    VCSmoothDSP::Params params;
    
    if (args.count("--depth")) {
        params.depth = std::stof(args["--depth"]);
        std::cout << "Depth: " << params.depth << "\n";
    }
    if (args.count("--speed")) {
        params.speed = std::stof(args["--speed"]);
        std::cout << "Speed: " << params.speed << "\n";
    }
    if (args.count("--freq-low")) {
        params.freqLow = std::stof(args["--freq-low"]);
        std::cout << "Freq Low: " << params.freqLow << " Hz\n";
    }
    if (args.count("--freq-high")) {
        params.freqHigh = std::stof(args["--freq-high"]);
        std::cout << "Freq High: " << params.freqHigh << " Hz\n";
    }
    if (args.count("--sharpness")) {
        params.sharpness = std::stof(args["--sharpness"]);
        std::cout << "Sharpness: " << params.sharpness << "\n";
    }
    if (args.count("--mix")) {
        params.mix = std::stof(args["--mix"]);
        std::cout << "Mix: " << params.mix << "\n";
    }
    if (args.count("--input-gain")) {
        params.inputGain = std::stof(args["--input-gain"]);
        std::cout << "Input Gain: " << params.inputGain << " dB\n";
    }
    if (args.count("--output-gain")) {
        params.outputGain = std::stof(args["--output-gain"]);
        std::cout << "Output Gain: " << params.outputGain << " dB\n";
    }
    
    // 读取音频文件
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(juce::File(inputFile)));
    
    if (!reader) {
        std::cerr << "错误: 无法读取文件 " << inputFile << "\n";
        return 1;
    }
    
    std::cout << "采样率: " << reader->sampleRate << " Hz\n";
    std::cout << "声道数: " << reader->numChannels << "\n";
    std::cout << "采样数: " << reader->lengthInSamples << "\n";
    
    // 读取音频数据
    juce::AudioBuffer<float> buffer(
        static_cast<int>(reader->numChannels),
        static_cast<int>(reader->lengthInSamples));
    
    reader->read(buffer.getArrayOfWritePointers(), 
                 buffer.getNumChannels(), 
                 0, 
                 buffer.getNumSamples());
    
    // 初始化 DSP
    VCSmoothDSP dsp;
    dsp.prepare(reader->sampleRate, 4096);
    dsp.setParams(params);
    
    // 处理音频
    std::cout << "处理中...\n";
    
    if (buffer.getNumChannels() >= 2) {
        dsp.process(buffer.getWritePointer(0), 
                    buffer.getWritePointer(1), 
                    buffer.getNumSamples());
    } else if (buffer.getNumChannels() == 1) {
        // 单声道处理两次
        auto* ptr = buffer.getWritePointer(0);
        dsp.process(ptr, ptr, buffer.getNumSamples());
    }
    
    // 写入 WAV 文件 (JUCE 8 需要 6 个参数)
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(
            new juce::FileOutputStream(juce::File(outputFile)),
            reader->sampleRate,
            static_cast<unsigned int>(buffer.getNumChannels()),
            32,  // bits per sample
            {},  // metadata
            0    // quality (for compressed formats)
        )
    );
    
    if (!writer) {
        std::cerr << "错误: 无法写入文件 " << outputFile << "\n";
        return 1;
    }
    
    writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    
    std::cout << "完成!\n";
    return 0;
}
