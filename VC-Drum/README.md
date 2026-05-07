# VC-Drum: Drum Synthesizer Plugin

基于合成的鼓机插件，无需采样文件，通过 DSP 实时生成 Kick/Snare/Hi-hat/Clap 四种鼓声。

## 核心特性

- **4种鼓声合成引擎**：Kick（正弦频率扫描）、Snare（体声+弦声混合）、Hi-hat（多方波+BPF）、Clap（噪声burst序列）
- **10种内置节拍模式**：kick-only, snare-only, hihat-only, basic-beat, house, techno, hiphop, trap, dnb, full
- **Swing & Humanize**：摇摆感和人性化微调
- **内置总线压缩器**：峰值检测，可调阈值/比例/攻击/释放
- **立体声输出**：Kick/Snare 居中，Hi-hat/Clap 有轻微立体声宽度

## 合成引擎

### Kick（底鼓）
- 正弦波频率扫描：从 `freq_start` 快速下滑到 `freq_end`
- 频率包络：`freq(t) = freq_end + (freq_start - freq_end) * envelope`
- 振幅包络：快速 attack + 指数 decay
- 参数：`--kick-freq-start` (Hz, 默认150), `--kick-freq-end` (Hz, 默认50), `--kick-decay` (ms, 默认300)

### Snare（军鼓）
- 体声：三角波 + 频率扫描（200→100Hz）
- 弦声：带通滤波白噪声（中心频率3000Hz, Q=1）
- 混合比例可调
- 参数：`--snare-tone` (0-1, 默认0.5), `--snare-decay` (ms, 默认200)

### Hi-hat（踩镲）
- 6个方波失谐叠加（金属非谐波比例）→ 带通滤波 → 振幅包络
- Open/Closed 两种模式（decay时间不同）
- 参数：`--hihat-decay` (ms, 默认50 closed), `--hihat-decay-open` (ms, 默认300 open)

### Clap（拍手）
- 带通滤波白噪声 + 多次短 burst 模拟多次击掌
- 参数：`--clap-count` (3-8, 默认3), `--clap-spread` (ms, 默认15)

## 信号流

```
[Kick引擎] ──┐
[Snare引擎] ─┼─→ 立体声混合器 → 压缩器 → 输出
[Hi-hat引擎] ┤
[Clap引擎] ──┘
```

## CLI 使用

```bash
# 基本用法 - 生成4小节鼓节拍
./VC-Drum-CLI-Standalone output.wav

# 使用预设
./VC-Drum-CLI-Standalone drums.wav --preset house
./VC-Drum-CLI-Standalone drums.wav --preset trap

# 自定义参数
./VC-Drum-CLI-Standalone drums.wav --pattern basic --bpm 130 --bars 8
./VC-Drum-CLI-Standalone drums.wav --pattern trap --bpm 140 --swing 30
./VC-Drum-CLI-Standalone drums.wav --kick-freq-start 200 --kick-decay 500

# 关闭压缩器
./VC-Drum-CLI-Standalone drums.wav --compressor 0
```

### 完整参数列表

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--preset` | 预设：bypass, kick-only, snare-only, hihat-only, basic-beat, house, techno, hiphop, trap, dnb | - |
| `--pattern` | 节拍模式：kick/snare/hihat/basic/full/house/techno/hiphop/trap/dnb | full |
| `--bpm` | 速度 | 120 |
| `--bars` | 渲染小节数 | 4 |
| `--swing` | 摇摆感 0-100% | 0 |
| `--humanize` | 人性化 0-100% | 0 |
| `--kick-freq-start` | Kick 扫频起始频率 (Hz) | 150 |
| `--kick-freq-end` | Kick 扫频终止频率 (Hz) | 50 |
| `--kick-decay` | Kick 衰减时间 (ms) | 300 |
| `--snare-tone` | Snare 体声/噪声混合 0-1 | 0.5 |
| `--snare-decay` | Snare 衰减时间 (ms) | 200 |
| `--hihat-decay` | Hi-hat Closed 衰减 (ms) | 50 |
| `--hihat-decay-open` | Hi-hat Open 衰减 (ms) | 300 |
| `--clap-count` | Clap burst 数量 3-8 | 3 |
| `--clap-spread` | Clap burst 间隔 (ms) | 15 |
| `--compressor` | 总线压缩器 0/1 | 1 |
| `--master-gain` | 主增益 (dB) | 0 |

### 预设 BPM

| 预设 | BPM | 说明 |
|------|-----|------|
| house | 126 | 四四拍 + 离拍踩镲 + 拍手 |
| techno | 132 | 四四拍 + 密集踩镲 |
| hiphop | 90 | 摇摆底鼓 + 碎镲 |
| trap | 140 | 快速踩镲 + 切分底鼓 |
| dnb | 174 | 经典 DNB 底鼓模式 |

## 编译

### Standalone CLI（零依赖）

```bash
cd VC-Drum
g++ -std=c++17 -DVC_STANDALONE -O2 -I../Libs/dr_wav \
    Source/CLI_Standalone/main.cpp Source/DSP/VCPluginDSP.cpp \
    -o VC-Drum-CLI-Standalone -lm
```

### JUCE VST3 + CLI

```bash
mkdir build && cd build
cmake .. -DJUCE_PATH=/opt/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## 目录结构

```
VC-Drum/
├── CMakeLists.txt
├── README.md
├── Source/
│   ├── DSP/
│   │   ├── VCPluginDSP.h          # 4种鼓声引擎 + 音序器 + 压缩器
│   │   └── VCPluginDSP.cpp        # DSP实现
│   ├── CLI_Standalone/
│   │   └── main.cpp               # Standalone CLI
│   ├── CLI/
│   │   └── main.cpp               # JUCE CLI
│   ├── PluginProcessor.h/cpp      # VST3处理器
│   └── PluginEditor.h/cpp         # VST3编辑器
└── .gitignore
```

## 依赖

- **JUCE 8.0+**（VST3和JUCE CLI）
- **CMake 3.22+**
- **dr_wav**（已包含在 `../Libs/dr_wav/`）

## 许可

MIT License
