// VC-EQ CLI - 命令行均衡器工具
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/VCEQDSP.h"

void printHelp(const char* p) {
    std::cout << "VC-EQ CLI - 5段参数均衡器\n\n";
    std::cout << "用法: " << p << " <input.wav> <output.wav> [选项]\n\n";
    std::cout << "选项:\n";
    std::cout << "  --help, -h 显示帮助\n";
    std::cout << "频段参数 (band0-band4):\n";
    std::cout << "  --band<N>-freq <Hz> --band<N>-gain <dB> --band<N>-q <value>\n";
    std::cout << "  --band<N>-type <0|1|2> --band<N>-on <0|1>\n\n";
    std::cout << "示例:\n";
    std::cout << "  " << p << " in.wav out.wav --band3-freq 3000 --band3-gain 2.5\n";
}

std::map<std::string, std::string> parseArgs(int c, char** v) {
    std::map<std::string, std::string> a;
    for (int i = 1; i < c; ++i) {
        std::string arg = v[i];
        if (arg == "--help" || arg == "-h") a["--help"] = "";
        else if (arg.substr(0,2) == "--") {
            std::string k = arg, val;
            if (i+1 < c && v[i+1][0] != '-') val = v[++i];
            a[k] = val;
        }
    }
    return a;
}

bool buildBandParams(const std::map<std::string, std::string>& a, int idx, VCEQDSP::BandParams& p) {
    std::string pre = "--band" + std::to_string(idx);
    bool has = false;
    for (const auto& x : a) if (x.first.find(pre) == 0) { has = true; break; }
    if (!has) return false;
    p = VCEQDSP::BandParams();
    p.frequency = VCEQDSP::kDefaultFrequencies[idx];
    p.q = VCEQDSP::kDefaultQ[idx];
    p.enabled = true;
    if (a.count(pre+"-freq")) p.frequency = std::stof(a.at(pre+"-freq"));
    if (a.count(pre+"-q")) p.q = std::stof(a.at(pre+"-q"));
    if (a.count(pre+"-type")) p.type = static_cast<VCEQDSP::FilterType>(std::stoi(a.at(pre+"-type")));
    if (a.count(pre+"-on")) p.enabled = (a.at(pre+"-on") == "1");
    return true;
}

int main(int c, char** v) {
    if (c < 2) { printHelp(v[0]); return 1; }
    auto a = parseArgs(c, v);
    if (a.count("--help")) { printHelp(v[0]); return 0; }
    std::vector<std::string> files;
    for (int i=1;i<c;++i) if (v[i][0] != '-') files.push_back(v[i]);
    if (files.size() < 2) { std::cerr << "需要输入和输出文件\n"; printHelp(v[0]); return 1; }
    std::string in = files[0], out = files[1];
    std::cout << "VC-EQ CLI\n输入: " << in << "\n输出: " << out << "\n";
    
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(juce::File(in)));
    if (!r) { std::cerr << "无法读取文件: " << in << "\n"; return 1; }
    std::cout << "采样率: " << r->sampleRate << "Hz, 声道: " << r->numChannels << "\n";
    
    juce::AudioBuffer<float> buf(static_cast<int>(r->numChannels), static_cast<int>(r->lengthInSamples));
    r->read(buf.getArrayOfWritePointers(), buf.getNumChannels(), 0, buf.getNumSamples());
    
    VCEQDSP dsp;
    dsp.prepare(r->sampleRate, 4096);
    VCEQDSP::BandParams bands[VCEQDSP::kNumBands];
    
    if (a.count("--preset")) {
        // Preset support: use flat default (all bands at 0dB)
        for (int i = 0; i < VCEQDSP::kNumBands; ++i) {
            bands[i].frequency = VCEQDSP::kDefaultFrequencies[i];
            bands[i].q = VCEQDSP::kDefaultQ[i];
            bands[i].gain = VCEQDSP::kDefaultGains[i];
            bands[i].type = VCEQDSP::BandType::Bell;
            bands[i].enabled = true;
        }
        dsp.setAllBands(bands);
    }
    
    // Set individual band parameters from CLI args
    for (int i = 0; i < VCEQDSP::kNumBands; ++i) {
        VCEQDSP::BandParams bp = bands[i]; // start from current (default or preset)
        std::string prefix = "--band" + std::to_string(i+1);
        if (a.count(prefix + "-freq")) bp.frequency = std::stof(a[prefix + "-freq"]);
        if (a.count(prefix + "-q")) bp.q = std::stof(a[prefix + "-q"]);
        if (a.count(prefix + "-gain")) bp.gain = std::stof(a[prefix + "-gain"]);
        dsp.setBand(i, bp);
    }
    
    std::cout << "处理中...\n";
    if (buf.getNumChannels() >= 2) {
        dsp.process(buf.getWritePointer(0), buf.getWritePointer(1), buf.getNumSamples());
    } else if (buf.getNumChannels() == 1) {
        auto* l = buf.getWritePointer(0);
        dsp.process(l, l, buf.getNumSamples());
    }
    
    // 写入WAV
    juce::WavAudioFormat wavFmt;
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFmt.createWriterFor(
        new juce::FileOutputStream(juce::File(out)), r->sampleRate, 
        static_cast<unsigned int>(buf.getNumChannels()), 32, {}, 0));
    if (writer) writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
    else { std::cerr << "无法写入文件: " << out << "\n"; return 1; }
    
    std::cout << "完成!\n";
    return 0;
}
