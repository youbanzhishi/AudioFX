// VC-EQ DSP 单元测试
#include <iostream>
#include <cmath>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "../Source/DSP/VCEQDSP.h"

int main() {
    std::cout << "VC-EQ DSP 单元测试\n";
    int passed = 0, failed = 0;
    
    // 测试1: 初始化
    std::cout << "\n[测试 1] 初始化\n";
    {
        VCEQDSP dsp;
        if (VCEQDSP::kNumBands == 5) {
            std::cout << "[PASS] 频段数量正确\n";
            passed++;
        } else {
            std::cout << "[FAIL] 频段数量错误\n";
            failed++;
        }
    }
    
    // 测试2: prepare
    std::cout << "\n[测试 2] prepare\n";
    {
        VCEQDSP dsp;
        dsp.prepare(44100.0, 512);
        if (dsp.getSampleRate() == 44100.0) {
            std::cout << "[PASS] 采样率设置正确\n";
            passed++;
        } else {
            std::cout << "[FAIL] 采样率设置错误\n";
            failed++;
        }
    }
    
    // 测试3: 频段参数设置
    std::cout << "\n[测试 3] 频段参数设置\n";
    {
        VCEQDSP dsp;
        dsp.prepare(44100.0, 512);
        VCEQDSP::BandParams p;
        p.frequency = 2000.0f;
        p.q = 2.0f;
        p.gainDB = 3.0f;
        p.type = VCEQDSP::FilterType::Parametric;
        p.enabled = true;
        dsp.setBand(2, p);
        auto r = dsp.getBand(2);
        if (std::abs(r.frequency - 2000.0f) < 0.1f && 
            std::abs(r.q - 2.0f) < 0.1f &&
            std::abs(r.gainDB - 3.0f) < 0.1f) {
            std::cout << "[PASS] 频段参数设置正确\n";
            passed++;
        } else {
            std::cout << "[FAIL] 频段参数设置错误\n";
            failed++;
        }
    }
    
    // 测试4: 音频处理
    std::cout << "\n[测试 4] 音频处理\n";
    {
        VCEQDSP dsp;
        dsp.prepare(44100.0, 512);
        const int N = 512;
        float left[N], right[N];
        for (int i = 0; i < N; ++i) {
            left[i] = 0.5f;
            right[i] = 0.5f;
        }
        // 平坦EQ处理
        dsp.process(left, right, N);
        bool ok = true;
        for (int i = 0; i < N; ++i) {
            if (std::abs(left[i]) > 2.0f || std::abs(right[i]) > 2.0f) {
                ok = false;
                break;
            }
        }
        if (ok) {
            std::cout << "[PASS] 音频处理正常\n";
            passed++;
        } else {
            std::cout << "[FAIL] 音频处理输出异常\n";
            failed++;
        }
    }
    
    // 测试5: 启用/禁用
    std::cout << "\n[测试 5] 启用/禁用\n";
    {
        VCEQDSP dsp;
        dsp.prepare(44100.0, 512);
        dsp.setEnabled(false);
        if (!dsp.isEnabled()) {
            std::cout << "[PASS] 禁用状态正确\n";
            passed++;
        } else {
            std::cout << "[FAIL] 禁用状态错误\n";
            failed++;
        }
        dsp.setEnabled(true);
        if (dsp.isEnabled()) {
            std::cout << "[PASS] 启用状态正确\n";
            passed++;
        } else {
            std::cout << "[FAIL] 启用状态错误\n";
            failed++;
        }
    }
    
    // 测试6: 预设
    std::cout << "\n[测试 6] 默认频率\n";
    {
        VCEQDSP dsp;
        bool ok = true;
        for (int i = 0; i < VCEQDSP::kNumBands; ++i) {
            auto p = dsp.getBand(i);
            if (std::abs(p.frequency - VCEQDSP::kDefaultFrequencies[i]) > 0.1f) {
                ok = false;
                break;
            }
        }
        if (ok) {
            std::cout << "[PASS] 默认频率正确\n";
            passed++;
        } else {
            std::cout << "[FAIL] 默认频率错误\n";
            failed++;
        }
    }
    
    std::cout << "\n==================\n";
    std::cout << "结果: " << passed << " 通过, " << failed << " 失败\n";
    std::cout << "==================\n";
    
    return failed > 0 ? 1 : 0;
}
