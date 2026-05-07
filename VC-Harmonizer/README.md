# VC-Harmonizer

**Intelligent Harmony Generator** - VocalChain 系列和声生成插件

基于 **JUCE 8** + **CMake** 的 VST3 音频插件。自动根据输入人声生成多声部和声，支持调性检测、共振峰保留和声像控制。

## 核心特性

- **智能和声生成**: 根据检测到的调性自动生成3度/5度/7度/8度音程和声
- **多声部输出**: 最多4个和声声部，每声部独立增益和声像
- **共振峰保留**: 基于LPC的共振峰处理，避免花栗鼠效应
- **调性检测**: Krumhansl-Schmuckler算法自动检测调性
- **自定义音程**: 支持任意半音数组合

## 信号流程

```
Input → [YIN Detect] → [Scale Quantize] → [Interval Shift] → [LPC Formant] → [Mix] → Output
              │                                      │
              └→ F0, Key, Scale          Voice1(+3st) ─→ gain+pan ─┐
                                               Voice2(+7st) ─→ gain+pan ─┤→ Sum
                                               Voice3(-5st) ─→ gain+pan ─┘
```

## 复用代码

- YIN检测：从VC-Tune复制
- LPC共振峰：从VC-Tune Gen2复制
- 音高偏移：简单重采样（线性插值）

## CLI 参数

```
--voices <1-4>              和声声部数 (默认: 2)
--intervals <3,7,12>        音程半音数，逗号分隔 (默认: 3,7,12,-5)
--voice-gain <dB,...>       每声部增益 (默认: 0,0,0,0)
--voice-pan <-1~1,...>      每声部声像 (默认: -0.5,0.5,0.7,-0.7)
--formant-preserve <0-100>  共振峰保留 (默认: 100)
--autokey <0|1>             自动调性检测 (默认: 0)
--scale <0-5>               音阶 (默认: 0=半音阶)
--direction <up|down|both>  和声方向 (默认: both)
--midi-track <num>          MIDI轨道占位 (默认: -1)
--bypass <0|1>              旁通 (默认: 0)
--preset <name>             预设
```

## 预设

| 预设名 | 描述 |
|--------|------|
| 3rd-5th | 经典3度+5度和声 |
| choir | 4声部合唱 (3度+5度+8度-5度) |
| octave | 八度加倍 |
| autokey | 自动调性检测+3度5度 |
| up-only | 仅上方和声 |
| subharmonic | 次和声 (5度+7度下方) |

## 使用示例

```bash
# 经典3度5度和声
./VC-Harmonizer-CLI-Standalone vocal.wav harmony.wav --preset 3rd-5th

# 自定义音程
./VC-Harmonizer-CLI-Standalone vocal.wav harmony.wav --voices 3 --intervals 3,7,12

# 自动调性检测
./VC-Harmonizer-CLI-Standalone vocal.wav harmony.wav --autokey 1 --intervals 4,7

# 仅上方和声
./VC-Harmonizer-CLI-Standalone vocal.wav harmony.wav --direction up --voice-gain -3,-6,-9
```

## 编译

```bash
# Standalone CLI (最快)
cd /tmp/AudioFX/VC-Harmonizer
g++ -std=c++17 -DVC_STANDALONE -O2 -I/tmp/AudioFX/Libs/dr_wav \
    Source/CLI_Standalone/main.cpp Source/DSP/VCPluginDSP.cpp \
    -o VC-Harmonizer-CLI-Standalone -lm

# CMake (包含VST3)
mkdir build && cd build
cmake .. -DJUCE_PATH=/opt/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## 依赖

- **JUCE 8.0+** (用于VST3和JUCE CLI)
- **CMake 3.22+**
- **dr_wav** (已包含在 `../Libs/dr_wav/`)
- C++17 编译器

## 许可

MIT License
