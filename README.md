# VocalChain AudioFX

> 20个专业音频VST3插件的C++开源集合 — 跨平台、轻量高性能、AI Agent友好

## 插件清单

### Gen1 (16插件)
| 插件 | 功能 | 特性 |
|------|------|------|
| VC-EQ | 5段参量均衡器 | LowShelf/Peak/HighShelf/LP/HP, IIR安全钳位 |
| VC-Comp | 压缩器+侧链 | soft/hard/auto膝点, warm/clean音色 |
| VC-Smooth | 平滑器 | 嘎吱声消除 |
| VC-DeEsser | 去齿音 | 频段检测+衰减 |
| VC-Gain | 增益/声像 | 立体声增益+声像控制 |
| VC-Saturator | 饱和器 | tape/tube/digital失真 |
| VC-Limiter | 限制器 | 砖墙限制+True Peak |
| VC-Delay | 延迟 | BPM同步+反馈+滤波 |
| VC-Reverb | 混响 | room/decay/damping/mix/predelay |
| VC-DynamicEQ | 动态EQ | 频段动态处理 |
| VC-Distortion | 失真 | 多种失真算法 |
| VC-Noise | 噪声 | 白/粉/棕噪声生成 |
| VC-SurgicalDeEsser | 精准去齿音 | 双频段检测 |
| VC-Tune | 修音 | YIN检测+PSOLA+5音阶+K-S调性 |
| VC-Gate | 噪声门 | 阈值+attack/release+hysteresis |
| VC-Chorus | 合唱 | LFO调制+立体声展宽 |

### Gen2 (4插件)
| 插件 | 升级内容 | 新特性 |
|------|----------|--------|
| VC-Reverb | FDN升级 | 8延迟线+Householder矩阵+早期反射+6新预设 |
| VC-Comp | 多段压缩 | LR4分频(120Hz/1kHz/8kHz)+4段独立压缩 |
| VC-Stereo | 🆕新插件 | MS编解码+宽度控制+声像+低频单声道 |
| VC-PitchShift | 🆕新插件 | Phase Vocoder+半音/cent微调 |

### 开发中
| 插件 | 状态 |
|------|------|
| VC-Tune Gen2 | 🏃 PSOLA完整化+共振峰保留 |
| VC-MultiBand | 🏃 4频段LR4分频路由器 |

## CLI Standalone模式

每个插件都支持独立CLI编译，无需JUCE依赖：

```bash
# 编译
g++ -std=c++17 -DVC_STANDALONE -O2 -I./Libs/dr_wav \
  Source/CLI_Standalone/main.cpp Source/DSP/VCPluginDSP.cpp \
  -o PluginName-CLI-Standalone -lm

# 使用
./VC-Comp-CLI-Standalone input.wav output.wav --threshold -20 --ratio 3
./VC-Reverb-CLI-Standalone input.wav output.wav --room 50 --decay 40
./VC-Comp-CLI-Standalone input.wav output.wav --multiband 1 --band-threshold -20,-18,-22,-30
```

## 技术特性

- **框架**: JUCE 8.0 + Standalone CLI (无JUCE依赖)
- **格式**: VST3 (macOS/Windows/Linux) + AU (macOS)
- **标准**: C++17
- **音频I/O**: dr_wav (CLI) / JUCE (VST3)
- **算法**: Audio EQ Cookbook / FDN / PSOLA / Phase Vocoder / LR4 Crossover

## 版本历史

| 版本 | 日期 | 内容 |
|------|------|------|
| v1.3.0 | 2025-05-07 | VC-Comp CLI多段参数 + VC-PitchShift修复 + VC-EQ IIR安全修复 |
| v1.2.0 | 2025-05-07 | VC-Comp多段压缩升级 (LR4+4段) |
| v1.1.0 | 2025-05-07 | VC-Stereo + VC-PitchShift新插件 + VC-Reverb FDN升级 |
| v1.0.0 | 2025-05-07 | 16 Gen1插件全族完成 |
| v0.2.0 | 2025-05-06 | 首批7插件 + CI |

## 许可证

MIT License
