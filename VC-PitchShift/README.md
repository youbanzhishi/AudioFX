# VC-PitchShift

基于 **JUCE 8** + **CMake** 的高品质变调 VST3 插件。使用 Phase Vocoder 算法实现不改变速度的变调。

## 功能

- **高品质变调**: Phase Vocoder 算法，不改变速度
- **半音调节**: -12 到 +12 半音
- **微调(Cents)**: -100 到 +100 cents 精细调节
- **共振峰保留**: 可选保留原始共振峰特征

## 算法

```
输入信号 → STFT分析(FFT 2048, hop 512)
         → 相位差计算与展开
         → 频率缩放(pitch ratio)
         → 相位累积
         → (可选)共振峰保留
         → ISTFT合成(overlap-add)
         → 输出信号
```

## CLI参数

| 参数 | 范围 | 默认值 | 说明 |
|------|------|--------|------|
| `--semitones` | -12~+12 | 0 | 变调半音数 |
| `--cents` | -100~+100 | 0 | 微调(cents) |
| `--formant` | 0\|1 | 0 | 共振峰保留 |
| `--bypass` | 0\|1 | 0 | 旁通 |

## 预设

bypass, up1, down1, up3, down3, octave-up, octave-down, formant-shift

## 使用示例

```bash
# 上调3个半音
./VC-PitchShift-CLI-Standalone in.wav out.wav --preset up3

# 下调7个半音 + 微调30cents
./VC-PitchShift-CLI-Standalone in.wav out.wav --semitones -7 --cents 30

# 上调5个半音保留共振峰
./VC-PitchShift-CLI-Standalone in.wav out.wav --semitones 5 --formant 1
```

## 编译

```bash
# Standalone CLI (秒编)
g++ -std=c++17 -DVC_STANDALONE -O2 -I/tmp/AudioFX/Libs/dr_wav \
    Source/CLI_Standalone/main.cpp Source/DSP/VCPluginDSP.cpp \
    -o VC-PitchShift-CLI-Standalone -lm

# 完整CMake构建
mkdir build && cd build
cmake .. -DJUCE_PATH=/opt/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## 许可

MIT License
