/**
 * VC-PhaseScope-CLI - 相位检测命令行工具
 * 
 * 用法：
 *   vc-phasescope-cli input.wav [--output report.txt]
 *   vc-phasescope-cli file1.wav file2.wav --compare
 * 
 * 输出：
 *   - 相位相关度
 *   - 底鼓/贝斯冲突状态
 *   - 频段分析
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <cstring>
#include <algorithm>
#include "VC-PhaseScope.h"

// 简化版WAV读取（仅支持44.1kHz/48kHz 16bit/32bit stereo）
struct WavReader {
    std::vector<float> left;
    std::vector<float> right;
    int sampleRate = 44100;
    int bitsPerSample = 16;
    int channels = 2;
    
    bool read(const std::string& path) {
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) {
            std::cerr << "Error: Cannot open file " << path << std::endl;
            return false;
        }
        
        // 读取WAV头
        char riff[4], wave[4], fmt[4], data[4];
        fread(riff, 1, 4, f);
        if (strncmp(riff, "RIFF", 4) != 0) {
            std::cerr << "Error: Not a valid WAV file" << std::endl;
            fclose(f);
            return false;
        }
        
        fread(wave, 1, 4, f);
        fread(fmt, 1, 4, f);
        
        // 读取fmt块
        short audioFormat, numChannels;
        int byteRate, blockAlign;
        fread(&audioFormat, 2, 1, f);
        fread(&numChannels, 2, 1, f);
        fread(&sampleRate, 4, 1, f);
        fread(&byteRate, 4, 1, f);
        fread(&blockAlign, 2, 1, f);
        fread(&bitsPerSample, 2, 1, f);
        
        channels = numChannels;
        
        // 跳过额外字节
        short extraBytes;
        fread(&extraBytes, 2, 1, f);
        fseek(f, extraBytes, SEEK_CUR);
        
        // 查找data块
        while (fread(data, 1, 4, f) == 4) {
            int chunkSize;
            fread(&chunkSize, 4, 1, f);
            
            if (strncmp(data, "data", 4) == 0) {
                // 读取音频数据
                size_t numSamples = chunkSize / (bitsPerSample / 8);
                left.resize(numSamples / channels);
                right.resize(numSamples / channels);
                
                if (bitsPerSample == 16) {
                    std::vector<short> buffer(numSamples);
                    fread(buffer.data(), 2, numSamples, f);
                    for (size_t i = 0; i < left.size(); ++i) {
                        left[i] = buffer[i * channels] / 32768.0f;
                        right[i] = channels > 1 ? buffer[i * channels + 1] / 32768.0f : left[i];
                    }
                } else if (bitsPerSample == 32) {
                    std::vector<int> buffer(numSamples);
                    fread(buffer.data(), 4, numSamples, f);
                    for (size_t i = 0; i < left.size(); ++i) {
                        left[i] = buffer[i * channels] / 2147483648.0f;
                        right[i] = channels > 1 ? buffer[i * channels + 1] / 2147483648.0f : left[i];
                    }
                } else {
                    std::cerr << "Error: Unsupported bit depth: " << bitsPerSample << std::endl;
                    fclose(f);
                    return false;
                }
                
                fclose(f);
                return true;
            } else {
                fseek(f, chunkSize, SEEK_CUR);
            }
        }
        
        fclose(f);
        return false;
    }
};

void printReport(const std::string& filename, WavReader& wav) {
    using namespace audiofx;
    
    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "           VC-PhaseScope 分析报告\n";
    std::cout << "═══════════════════════════════════════════════════════\n\n";
    
    std::cout << "文件: " << filename << "\n";
    std::cout << "采样率: " << wav.sampleRate << " Hz\n";
    std::cout << "位深: " << wav.bitsPerSample << " bit\n";
    std::cout << "声道: " << (wav.channels > 1 ? "立体声" : "单声道") << "\n";
    std::cout << "时长: " << (wav.left.size() / wav.sampleRate) << " 秒\n";
    std::cout << "样本数: " << wav.left.size() << "\n\n";
    
    // 1. 整体相位相关度
    float correlation = PhaseCorrelation::compute(wav.left.data(), wav.right.data(), 
                                                  wav.left.size());
    
    std::cout << "───────────────────────────────────────────────────────\n";
    std::cout << "                    相位分析\n";
    std::cout << "───────────────────────────────────────────────────────\n";
    std::cout << "整体相位相关度: " << correlation << "\n";
    std::cout << "相位状态: " << PhaseVector::getStateName(correlation) << "\n";
    
    if (correlation > 0.8f) {
        std::cout << "✓ 相位正常 - 立体声与单声道兼容性好\n";
    } else if (correlation > 0.0f) {
        std::cout << "⚠ 警告 - 存在一定相位差异\n";
    } else if (correlation > -0.5f) {
        std::cout << "✗ 错误 - 存在相位反转\n";
    } else {
        std::cout << "✗✗ 严重 - 完全反相，会产生相位抵消\n";
    }
    std::cout << "\n";
    
    // 2. 底鼓/贝斯冲突检测
    KickBassDetector::ConflictLevel kickBassConflict = 
        KickBassDetector().detect(wav.left.data(), wav.right.data(),
                                  (float)wav.sampleRate, wav.left.size());
    
    std::cout << "───────────────────────────────────────────────────────\n";
    std::cout << "                  低频冲突检测\n";
    std::cout << "───────────────────────────────────────────────────────\n";
    std::cout << "底鼓/贝斯状态: " << KickBassDetector::getLevelName(kickBassConflict) << "\n";
    
    if (kickBassConflict == KickBassDetector::ConflictLevel::High) {
        std::cout << "✗✗ 严重冲突 - 低频会产生相位抵消，\n";
        std::cout << "             建议检查底鼓与贝斯的相位关系\n";
    } else if (kickBassConflict == KickBassDetector::ConflictLevel::Medium) {
        std::cout << "⚠ 中等冲突 - 建议检查低频相位\n";
    } else if (kickBassConflict == KickBassDetector::ConflictLevel::Low) {
        std::cout << "⚠ 轻微冲突 - 建议关注\n";
    } else {
        std::cout << "✓ 正常 - 低频相位无明显问题\n";
    }
    std::cout << "\n";
    
    // 3. RMS电平
    float rmsL = 0.0f, rmsR = 0.0f;
    for (size_t i = 0; i < wav.left.size(); ++i) {
        rmsL += wav.left[i] * wav.left[i];
        rmsR += wav.right[i] * wav.right[i];
    }
    rmsL = std::sqrt(rmsL / wav.left.size());
    rmsR = std::sqrt(rmsR / wav.right.size());
    
    std::cout << "───────────────────────────────────────────────────────\n";
    std::cout << "                    电平分析\n";
    std::cout << "───────────────────────────────────────────────────────\n";
    std::cout << "左声道 RMS: " << (20 * std::log10(rmsL + 1e-10f)) << " dB\n";
    std::cout << "右声道 RMS: " << (20 * std::log10(rmsR + 1e-10f)) << " dB\n";
    std::cout << "声道差: " << std::abs(20 * std::log10(rmsL + 1e-10f) - 20 * std::log10(rmsR + 1e-10f)) << " dB\n";
    std::cout << "\n";
    
    std::cout << "═══════════════════════════════════════════════════════\n\n";
}

void printUsage(const char* prog) {
    std::cout << "VC-PhaseScope-CLI - 相位检测工具\n\n";
    std::cout << "用法:\n";
    std::cout << "  " << prog << " <input.wav>              - 分析单个文件\n";
    std::cout << "  " << prog << " <input.wav> -o <report.txt> - 输出到文件\n";
    std::cout << "\n";
    std::cout << "功能:\n";
    std::cout << "  - 相位相关度检测 (-1 到 +1)\n";
    std::cout << "  - 底鼓/贝斯冲突检测\n";
    std::cout << "  - RMS电平分析\n";
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string inputFile = argv[1];
    std::string outputFile;
    
    // 解析参数
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            outputFile = argv[++i];
        }
    }
    
    // 读取WAV文件
    WavReader wav;
    if (!wav.read(inputFile)) {
        return 1;
    }
    
    // 生成报告
    std::streambuf* oldBuf = nullptr;
    std::ofstream outFile;
    
    if (!outputFile.empty()) {
        outFile.open(outputFile);
        if (!outFile.is_open()) {
            std::cerr << "Error: Cannot open output file " << outputFile << std::endl;
            return 1;
        }
        oldBuf = std::cout.rdbuf(outFile.rdbuf());
    }
    
    printReport(inputFile, wav);
    
    if (oldBuf) {
        std::cout.rdbuf(oldBuf);
        outFile.close();
        std::cout << "报告已保存到: " << outputFile << std::endl;
    }
    
    return 0;
}
