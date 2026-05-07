# VC-DeEsser

基于 **JUCE 8** + **CMake** 的 VST3 音频插件 — 去齿音（De-esser）。包含完整的 **三层架构**（DSP/CLI/VST3）和 **双 CLI**（JUCE版本 + 零依赖Standalone版本）。

## 核心特性

- **去齿音处理**：检测并压缩高频齿音（sibilance）
- **可调参数**：
  - **Threshold**（阈值）：-40dB ~ 0dB，控制触发点
  - **Frequency**（频率）：2kHz ~ 12kHz，齿音检测中心频率
  - **Reduction**（衰减量）：-30dB ~ 0dB，最大衰减程度
  - **Mix**（干湿比）：0% ~ 100%
- **三层架构**: DSP核心层 → CLI命令行层 → VST3插件层
- **双CLI支持**: 
  - `VC-DeEsser-CLI`: 使用JUCE读写WAV（需要链接JUCE库）
  - `VC-DeEsser-CLI-Standalone`: 使用dr_wav（零外部依赖）

## 目录结构

```
VC-DeEsser/
├── CMakeLists.txt                      # 构建配置（3个target）
├── README.md                            # 本文件
├── Source/
│   ├── DSP/
│   │   ├── VCPluginDSP.h               # DSP核心头文件
│   │   └── VCPluginDSP.cpp             # DSP核心实现
│   ├── CLI/
│   │   └── main.cpp                    # JUCE版CLI（依赖JUCE）
│   ├── CLI_Standalone/
│   │   └── main.cpp                    # Standalone版CLI（dr_wav）
│   ├── PluginProcessor.h               # VST3处理器头文件
│   ├── PluginProcessor.cpp              # VST3处理器实现
│   ├── PluginEditor.h                   # VST3编辑器头文件
│   └── PluginEditor.cpp                # VST3编辑器实现
└── .github/workflows/                   # CI/CD配置（从根目录同步）
```

## 快速开始

### 1. 构建

```bash
mkdir build && cd build
cmake .. -DJUCE_PATH=/opt/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

#### 只编译Standalone CLI（最快，无需JUCE依赖）

```bash
cmake --build . --target VC-DeEsser-CLI-Standalone
```

#### 只编译VST3插件

```bash
cmake --build . --target VC-DeEsser
```

## 使用CLI

### Standalone CLI（推荐）

```bash
# 基本用法
./build/CLI_Standalone/VC-DeEsser-CLI-Standalone input.wav output.wav

# 使用预设
./build/CLI_Standalone/VC-DeEsser-CLI-Standalone input.wav output.wav --preset moderate

# 自定义参数
./build/CLI_Standalone/VC-DeEsser-CLI-Standalone input.wav output.wav --threshold -24 --frequency 7000

# 帮助
./build/CLI_Standalone/VC-DeEsser-CLI-Standalone --help
```

### 预设

| 预设名 | Threshold | Frequency | Reduction | Mix |
|--------|-----------|-----------|-----------|-----|
| bypass | -20dB | 6000Hz | -10dB | 100% |
| mild | -18dB | 6000Hz | -6dB | 100% |
| moderate | -20dB | 7000Hz | -12dB | 100% |
| heavy | -24dB | 8000Hz | -20dB | 100% |

### JUCE CLI

```bash
./build/CLI/VC-DeEsser-CLI input.wav output.wav --threshold -24
```

## DSP算法

去齿音的核心原理：检测高频齿音（sibilance）→ 压缩高频

1. 用二阶带通滤波器（Bandpass）提取齿音频段能量
2. 对提取的信号计算包络（简单RMS + smoothing）
3. 超过阈值时按比例计算gain reduction
4. 对原始信号乘以reduction gain
5. 混合处理信号与原始信号（dry/wet mix）

## 参数说明

| 参数 | 范围 | 默认值 | 说明 |
|------|------|--------|------|
| Threshold | -40~0 dB | -20 dB | 触发阈值 |
| Frequency | 2000~12000 Hz | 6000 Hz | 齿音检测中心频率 |
| Reduction | -30~0 dB | -10 dB | 最大衰减量 |
| Mix | 0~100% | 100% | 干湿比 |

## CI/CD

模板已配置GitHub Actions工作流（从仓库根目录 `.github/workflows/` 同步）。

## 依赖

- **JUCE 8.0+**（用于VST3和JUCE CLI）
- **CMake 3.22+**
- **dr_wav**（已包含在 `../Libs/dr_wav/`）
- 支持VST3的DAW（REAPER, Ableton Live, Cubase等）

## 参考项目

- [VC-EQ](https://github.com/youbanzhishi/AudioFX) - 5频段参量均衡器
- [VC-Comp](https://github.com/youbanzhishi/AudioFX) - 侧链压缩器
- [VC-Smooth](https://github.com/youbanzhishi/AudioFX) - 频谱共振抑制器
- [VC-Gain](https://github.com/youbanzhishi/AudioFX) - 增益调节器

## 许可

MIT License
