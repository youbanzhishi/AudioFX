# VC-DynamicEQ

基于 **JUCE 8** + **CMake** 的动态均衡器 VST3 音频插件。动态EQ结合了EQ和压缩器的特性，当某个频段的能量超过阈值时，动态地增益或衰减该频段。

## 核心特性

- **动态均衡器**: 当目标频段能量超过阈值时自动应用增益/衰减
- **静态+动态增益**: 支持固定EQ增益 + 动态增益范围
- **可调包络跟随器**: Attack/Release时间可调
- **Dry/Wet混合**: 保留原始信号比例
- **三层架构**: DSP核心层 → CLI命令行层 → VST3插件层
- **双CLI支持**: 
  - `VC-DynamicEQ-CLI`: 使用JUCE读写WAV
  - `VC-DynamicEQ-CLI-Standalone`: 使用dr_wav（零外部依赖）

## 参数说明

| 参数 | 范围 | 默认值 | 说明 |
|------|------|--------|------|
| Frequency | 20~20000 Hz | 200 Hz | 中心频率 |
| Gain | -18~+18 dB | -6 dB | 静态增益 |
| Q | 0.1~10 | 1.0 | Q值（带宽） |
| Threshold | -48~0 dB | -12 dB | 动态阈值 |
| Range | -24~+24 dB | -12 dB | 动态范围（负值=衰减，正值=增强） |
| Attack | 0.1~50 ms | 10 ms | 启动时间 |
| Release | 10~500 ms | 100 ms | 释放时间 |
| Mix | 0~100% | 100% | Dry/Wet混合 |
| Bypass | 0/1 | 0 | 旁通开关 |

## 典型应用

- **去箱声(De-Boom)**: 衰减低频共振
- **去齿音(De-Harsh)**: 高频动态衰减
- **存在感增强**: 动态提升特定频段

## 预设

| 预设名 | 应用场景 |
|--------|----------|
| bypass | 旁通 |
| de-boom | 去除低频轰鸣（150Hz，-12dB衰减范围）|
| de-harsh | 去除高频刺耳（3500Hz，-8dB衰减范围）|
| presence-boost | 存在感增强（4000Hz，+6dB提升范围）|

## 目录结构

```
VC-DynamicEQ/
├── CMakeLists.txt
├── README.md
├── Source/
│   ├── DSP/
│   │   ├── VCPluginDSP.h
│   │   └── VCPluginDSP.cpp
│   ├── CLI/
│   │   └── main.cpp
│   ├── CLI_Standalone/
│   │   └── main.cpp
│   ├── PluginProcessor.h
│   ├── PluginProcessor.cpp
│   ├── PluginEditor.h
│   └── PluginEditor.cpp
```

## 使用CLI

### Standalone CLI（推荐，无需JUCE）

```bash
./build/CLI_Standalone/VC-DynamicEQ-CLI-Standalone input.wav output.wav --preset de-boom
./build/CLI_Standalone/VC-DynamicEQ-CLI-Standalone input.wav output.wav --frequency 200 --gain -6 --threshold -12
```

### JUCE CLI

```bash
./build/CLI/VC-DynamicEQ-CLI input.wav output.wav --preset de-harsh
```

## CI/CD

本插件使用GitHub Actions进行自动化构建，支持：
- macOS (x64 + ARM64)
- Windows (x64)
- Ubuntu (x64)

详见 `.github/workflows/build.yml`。

## 依赖

- **JUCE 8.0+**（用于VST3和JUCE CLI）
- **CMake 3.22+**
- **dr_wav**（已包含在 `../Libs/dr_wav/`）
- 支持VST3的DAW（REAPER, Ableton Live, Cubase等）

## 算法原理

动态EQ的核心算法：

1. **频段检测**: 用带通滤波器提取目标频段能量
2. **包络跟随**: 根据Attack/Release时间计算包络
3. **动态增益计算**: 能量超过阈值时，按比例计算额外增益
4. **均衡处理**: 用Peaking EQ应用静态+动态增益
5. **Dry/Wet混合**: 混合处理后的信号与原始信号

## 参考项目

- [VC-EQ](https://github.com/youbanzhishi/AudioFX) - 5频段参量均衡器
- [VC-Comp](https://github.com/youbanzhishi/AudioFX) - 侧链压缩器
- [VC-Smooth](https://github.com/youbanzhishi/AudioFX) - 频谱共振抑制器

## 许可

MIT License
