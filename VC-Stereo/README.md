# VC-Stereo

基于 **JUCE 8** + **CMake** 的立体声处理 VST3 插件。支持立体声宽度控制、MS编解码、声像控制和低频单声道合并。

## 功能

- **立体声宽度控制**: 通过MS编解码调整立体声宽度
  - 0% = 单声道, 100% = 原始, 200% = 超宽立体声
- **MS编解码**: L/R ↔ Mid/Side 转换
- **声像控制**: 基于恒定功率声像法则的左右平衡
- **低频单声道合并**: 可选将指定频率以下的低频内容合并为单声道，避免低频相位问题

## 算法

```
L/R → M/S编码 → 宽度控制 → M/S解码 → 声像控制 → 输出L/R
                    ↑                           ↑
              (调整Side增益)            (cos/sin声像法则)

可选: 低频(<bassFreq)提取为单声道后替换回输出
```

## CLI参数

| 参数 | 范围 | 默认值 | 说明 |
|------|------|--------|------|
| `--width` | 0~200 % | 100 | 立体声宽度 |
| `--pan` | -100~100 | 0 | 声像 (-100=全左, 0=居中, 100=全右) |
| `--mono-bass` | 0\|1 | 0 | 低频单声道合并 |
| `--bass-freq` | 50~300 Hz | 150 | 低频交叉频率 |
| `--bypass` | 0\|1 | 0 | 旁通 |

## 预设

bypass, mono, wide, extra-wide, bass-mono, center-pan

## 使用示例

```bash
# 宽立体声
./VC-Stereo-CLI-Standalone in.wav out.wav --preset wide

# 自定义参数
./VC-Stereo-CLI-Standalone in.wav out.wav --width 150 --pan -20 --mono-bass 1

# 单声道
./VC-Stereo-CLI-Standalone in.wav out.wav --preset mono
```

## 编译

```bash
# Standalone CLI (秒编)
g++ -std=c++17 -DVC_STANDALONE -O2 -I/tmp/AudioFX/Libs/dr_wav \
    Source/CLI_Standalone/main.cpp Source/DSP/VCPluginDSP.cpp \
    -o VC-Stereo-CLI-Standalone -lm

# 完整CMake构建
mkdir build && cd build
cmake .. -DJUCE_PATH=/opt/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## 许可

MIT License
