# VC-Saturator

基于 **JUCE 8** + **CMake** 的 VST3 音频插件 — 饱和失真（Saturator）。包含完整的 **三层架构**（DSP/CLI/VST3）和 **双 CLI**（JUCE版本 + 零依赖Standalone版本）。

## 核心特性

- **饱和失真处理**：waveshaping函数，模拟模拟电路的软削波
- **三种算法**：
  - **Tape**：tanh曲线，平滑温暖的磁带饱和
  - **Tube**：x/(1+|x|)曲线，更暖的电子管失真
  - **Clip**：硬限幅，数字风格的硬削波
- **可调参数**：
  - **Drive**（驱动）：0dB ~ 24dB，控制饱和强度
  - **Algorithm**（算法）：Tape / Tube / Clip
  - **Mix**（干湿比）：0% ~ 100%
- **三层架构**: DSP核心层 → CLI命令行层 → VST3插件层
- **双CLI支持**: 
  - `VC-Saturator-CLI`: 使用JUCE读写WAV（需要链接JUCE库）
  - `VC-Saturator-CLI-Standalone`: 使用dr_wav（零外部依赖）

## 目录结构

```
VC-Saturator/
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
cmake --build . --target VC-Saturator-CLI-Standalone
```

#### 只编译VST3插件

```bash
cmake --build . --target VC-Saturator
```

## 使用CLI

### Standalone CLI（推荐）

```bash
# 基本用法
./build/CLI_Standalone/VC-Saturator-CLI-Standalone input.wav output.wav

# 使用预设
./build/CLI_Standalone/VC-Saturator-CLI-Standalone input.wav output.wav --preset tape-warm

# 自定义参数
./build/CLI_Standalone/VC-Saturator-CLI-Standalone input.wav output.wav --drive 12 --algorithm 0 --mix 80

# 帮助
./build/CLI_Standalone/VC-Saturator-CLI-Standalone --help
```

### 预设

| 预设名 | Drive | Mix | Algorithm |
|--------|-------|-----|----------|
| bypass | 0dB | 100% | tape |
| warm | 3dB | 100% | tube |
| tape-warm | 6dB | 80% | tape |
| driven | 12dB | 100% | tape |
| crunch | 18dB | 70% | clip |

### JUCE CLI

```bash
./build/CLI/VC-Saturator-CLI input.wav output.wav --drive 6 --algorithm tube
```

## DSP算法

饱和失真的核心：waveshaping函数

```cpp
// Tape saturation (soft): tanh
float tapeSaturate(float x, float drive) {
    return std::tanh(x * drive);
}

// Tube saturation (warmer): x / (1 + |x|)
float tubeSaturate(float x, float drive) {
    float xd = x * drive;
    return xd / (1.0f + std::abs(xd));
}

// Hard clip
float hardClip(float x, float drive) {
    return VC_JCLAMP(x * drive, -1.0f, 1.0f);
}
```

## 参数说明

| 参数 | 范围 | 默认值 | 说明 |
|------|------|--------|------|
| Drive | 0~24 dB | 0 dB | 驱动量/饱和强度 |
| Algorithm | Tape/Tube/Clip | Tape | 饱和算法 |
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
- [VC-DeEsser](https://github.com/youbanzhishi/AudioFX) - 去齿音

## 许可

MIT License
