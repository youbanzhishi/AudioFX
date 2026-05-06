// VC-Comp CLI - 命令行压缩器工具
#include <iostream>
#include <string>
#include <map>
#include <cmath>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/VCCompDSP.h"

void printHelp(const char* p) {
    std::cout << "VC-Comp CLI - 侧链压缩器\n\n";
    std::cout << "用法: " << p << " <input.wav> <output.wav> [选项]\n\n";
    std::cout << "选项:\n";
    std::cout << "  --help, -h           显示帮助\n";
    std::cout << "  --threshold <dB>     阈值 (-60 ~ 0), 默认: 0\n";
    std::cout << "  --ratio <ratio>     压缩比 (0.5 ~ 50), 默认: 1.0\n";
    std::cout << "  --attack <ms>       启动时间 (0.5 ~ 500), 默认: 16\n";
    std::cout << "  --release <ms>      释放时间 (5 ~ 5000), 默认: 160\n";
    std::cout << "  --gain <dB>         Makeup增益 (-30 ~ 30), 默认: 0\n";
    std::cout << "  --knee <mode>       拐点模式: soft, hard, 默认: soft\n";
    std::cout << "  --character <mode>  特性: clean, warm, 默认: warm\n";
    std::cout << "  --mix <%>           干湿比 (0 ~ 100), 默认: 100\n";
    std::cout << "  --trim <dB>         Trim输出 (-18 ~ 18), 默认: 0\n";
    std::cout << "\n预设:\n";
    std::cout << "  --preset <name>     预设名称\n";
    std::cout << "    vocal-1db    轻微压缩 (阈值 -20dB, 比 2:1)\n";
    std::cout << "    vocal-3db    中度压缩 (阈值 -10dB, 比 4:1)\n";
    std::cout << "    vocal-6db    强力压缩 (阈值 -5dB, 比 6:1)\n";
    std::cout << "    drums        鼓组压缩 (阈值 -15dB, 比 3:1, 快释放)\n";
    std::cout << "    bass         低音压缩 (阈值 -12dB, 比 4:1)\n";
    std::cout << "    limiter      限制模式 (阈值 -1dB, 比 20:1)\n";
    std::cout << "\n示例:\n";
    std::cout << "  " << p << " in.wav out.wav --threshold -20 --ratio 3 --attack 10\n";
    std::cout << "  " << p << " in.wav out.wav --preset vocal-3db\n";
}

std::map<std::string, std::string> parseArgs(int c, char** v) {
    std::map<std::string, std::string> a;
    for (int i = 1; i < c; ++i) {
        std::string arg = v[i];
        if (arg == "--help" || arg == "-h") {
            a["--help"] = "";
        } else if (arg.substr(0, 2) == "--") {
            std::string k = arg, val;
            if (i + 1 < c && v[i + 1][0] != '-') val = v[++i];
            a[k] = val;
        }
    }
    return a;
}

bool loadPreset(const std::string& name, VCCompDSP::Params& p) {
    if (name == "vocal-1db") {
        p.threshold = -20.0f; p.ratio = 2.0f; p.attack = 15.0f; p.release = 200.0f;
        p.gain = 0.0f; p.mix = 100.0f;
        return true;
    } else if (name == "vocal-3db") {
        p.threshold = -10.0f; p.ratio = 4.0f; p.attack = 10.0f; p.release = 150.0f;
        p.gain = 2.0f; p.mix = 100.0f;
        return true;
    } else if (name == "vocal-6db") {
        p.threshold = -5.0f; p.ratio = 6.0f; p.attack = 5.0f; p.release = 100.0f;
        p.gain = 4.0f; p.mix = 100.0f;
        return true;
    } else if (name == "drums") {
        p.threshold = -15.0f; p.ratio = 3.0f; p.attack = 1.0f; p.release = 50.0f;
        p.gain = 2.0f; p.mix = 100.0f;
        return true;
    } else if (name == "bass") {
        p.threshold = -12.0f; p.ratio = 4.0f; p.attack = 20.0f; p.release = 300.0f;
        p.gain = 3.0f; p.mix = 100.0f;
        return true;
    } else if (name == "limiter") {
        p.threshold = -1.0f; p.ratio = 20.0f; p.attack = 0.5f; p.release = 50.0f;
        p.gain = 0.0f; p.mix = 100.0f; p.trim = -0.3f;
        return true;
    }
    return false;
}

int main(int c, char** v) {
    if (c < 2) { printHelp(v[0]); return 1; }
    
    auto a = parseArgs(c, v);
    if (a.count("--help")) { printHelp(v[0]); return 0; }
    
    // Get input/output files
    std::vector<std::string> files;
    for (int i = 1; i < c; ++i) {
        if (v[i][0] != '-') files.push_back(v[i]);
    }
    if (files.size() < 2) {
        std::cerr << "错误: 需要输入和输出文件\n\n";
        printHelp(v[0]);
        return 1;
    }
    
    std::string inFile = files[0];
    std::string outFile = files[1];
    
    std::cout << "VC-Comp CLI\n";
    std::cout << "输入: " << inFile << "\n";
    std::cout << "输出: " << outFile << "\n";
    
    // Read audio file
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(juce::File(inFile)));
    if (!reader) {
        std::cerr << "错误: 无法读取文件: " << inFile << "\n";
        return 1;
    }
    
    std::cout << "采样率: " << reader->sampleRate << " Hz\n";
    std::cout << "声道数: " << reader->numChannels << "\n";
    std::cout << "采样数: " << reader->lengthInSamples << "\n";
    
    // Read buffer
    juce::AudioBuffer<float> buffer(
        static_cast<int>(reader->numChannels),
        static_cast<int>(reader->lengthInSamples));
    reader->read(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), 
                 0, buffer.getNumSamples());
    
    // Setup DSP
    VCCompDSP dsp;
    dsp.prepare(reader->sampleRate, 4096);
    
    VCCompDSP::Params params;
    
    // Load preset if specified
    if (a.count("--preset")) {
        std::string presetName = a["--preset"];
        std::cout << "预设: " << presetName << "\n";
        if (!loadPreset(presetName, params)) {
            std::cerr << "错误: 未知预设 '" << presetName << "'\n";
            return 1;
        }
    }
    
    // Override with command line arguments
    if (a.count("--threshold")) params.threshold = std::stof(a["--threshold"]);
    if (a.count("--ratio")) params.ratio = std::stof(a["--ratio"]);
    if (a.count("--attack")) params.attack = std::stof(a["--attack"]);
    if (a.count("--release")) params.release = std::stof(a["--release"]);
    if (a.count("--gain")) params.gain = std::stof(a["--gain"]);
    if (a.count("--mix")) params.mix = std::stof(a["--mix"]);
    if (a.count("--trim")) params.trim = std::stof(a["--trim"]);
    
    if (a.count("--knee")) {
        std::string knee = a["--knee"];
        if (knee == "hard") params.kneeMode = 0;
        else if (knee == "soft") params.kneeMode = 1;
        else if (knee == "auto") params.kneeMode = 2;
    }
    
    if (a.count("--character")) {
        std::string chr = a["--character"];
        if (chr == "clean") params.character = 0;
        else if (chr == "warm") params.character = 1;
    }
    
    // Print settings
    std::cout << "\n参数设置:\n";
    std::cout << "  阈值: " << params.threshold << " dB\n";
    std::cout << "  压缩比: " << params.ratio << ":1\n";
    std::cout << "  启动: " << params.attack << " ms\n";
    std::cout << "  释放: " << params.release << " ms\n";
    std::cout << "  Makeup: " << params.gain << " dB\n";
    std::cout << "  干湿比: " << params.mix << "%\n";
    std::cout << "  Trim: " << params.trim << " dB\n";
    std::cout << "  拐点: " << (params.kneeMode == 0 ? "Hard" : params.kneeMode == 1 ? "Soft" : "Auto") << "\n";
    std::cout << "  特性: " << (params.character == 0 ? "Clean" : "Warm") << "\n";
    
    dsp.setParams(params);
    
    // Process
    std::cout << "\n处理中...\n";
    if (buffer.getNumChannels() >= 2) {
        dsp.process(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());
    } else if (buffer.getNumChannels() == 1) {
        auto* left = buffer.getWritePointer(0);
        dsp.process(left, left, buffer.getNumSamples());
    }
    
    // Print GR stats
    float gr = dsp.getGainReduction();
    std::cout << "增益减少: " << gr << " dB\n";
    
    // Write output
    juce::WavAudioFormat wavFmt;
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFmt.createWriterFor(
        new juce::FileOutputStream(juce::File(outFile)),
        reader->sampleRate,
        static_cast<unsigned int>(buffer.getNumChannels()),
        32,     // bit depth
        {},
        0));    // quality (for WAV this is ignored)
    
    if (writer) {
        writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
        std::cout << "完成! 输出文件: " << outFile << "\n";
    } else {
        std::cerr << "错误: 无法写入文件: " << outFile << "\n";
        return 1;
    }
    
    return 0;
}
