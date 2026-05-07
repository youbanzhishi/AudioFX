# VC-MultiBand

VocalChain系列独立多段分频处理插件。基于**LR4 Linkwitz-Riley 24dB/oct**分频器，将信号分成4个频段，每频段独立增益和压缩处理。

## 核心特性

- **4频段LR4分频**：24dB/oct Linkwitz-Riley，相位一致
- **可调分频点**：3个分频频率独立可调（默认120/1000/8000Hz）
- **每频段增益**：独立增益控制（dB）
- **每频段压缩器**：简化压缩器（阈值+比率）
- **Solo/Mute**：频段独奏和静音
- **幅度完美重建**：LR4保证分频后合路幅度响应平坦

## 频段划分

| 频段 | 范围 | 默认分频点 |
|------|------|-----------|
| Low | < xover1 | < 120Hz |
| Mid-Low | xover1 - xover2 | 120Hz - 1kHz |
| Mid-High | xover2 - xover3 | 1kHz - 8kHz |
| High | > xover3 | > 8kHz |

## 架构

```
Input → [LR4 Split] → Low ─────→ [Gain] → [Comp] → ─┐
                    → Mid-Low ─→ [Gain] → [Comp] → ─┤
                    → Mid-High → [Gain] → [Comp] → ─┤→ [Sum] → Output
                    → High ────→ [Gain] → [Comp] → ─┘
```

## 目录结构

```
VC-MultiBand/
├── CMakeLists.txt                      # 构建配置（3个target）
├── Source/
│   ├── DSP/
│   │   ├── VCPluginDSP.h               # DSP核心头文件（LR4分频+压缩器）
│   │   └── VCPluginDSP.cpp             # DSP核心实现
│   ├── CLI/
│   │   └── main.cpp                    # JUCE版CLI
│   ├── CLI_Standalone/
│   │   └── main.cpp                    # Standalone版CLI（dr_wav）
│   ├── PluginProcessor.h/cpp           # VST3处理器
│   └── PluginEditor.h/cpp              # VST3编辑器
└── README.md
```

## 编译

### Standalone CLI（零依赖，推荐测试）

```bash
cd /tmp/AudioFX/VC-MultiBand
g++ -std=c++17 -DVC_STANDALONE -O2 \
    -I/tmp/AudioFX/Libs/dr_wav \
    Source/CLI_Standalone/main.cpp Source/DSP/VCPluginDSP.cpp \
    -o VC-MultiBand-CLI-Standalone -lm
```

### CMake全量编译

```bash
mkdir build && cd build
cmake .. -DJUCE_PATH=/opt/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## CLI使用

### 分频点控制

```bash
# 自定义分频点
./VC-MultiBand-CLI-Standalone in.wav out.wav --xover1 80 --xover2 500 --xover3 5000
```

### 每频段增益（4个逗号分隔值：Low,Mid-Low,Mid-High,High）

```bash
# 低频-6dB，中高频+3dB
./VC-MultiBand-CLI-Standalone in.wav out.wav --band-gain -6,0,+3,0
```

### 每频段压缩

```bash
# 中高频和高频压缩
./VC-MultiBand-CLI-Standalone in.wav out.wav \
    --band-threshold 0,0,-10,-20 \
    --band-ratio 1,1,3,4
```

### Solo/Mute

```bash
# 独奏低频段
./VC-MultiBand-CLI-Standalone in.wav out.wav --solo-band 1

# 静音高频段
./VC-MultiBand-CLI-Standalone in.wav out.wav --mute-band 4

# 多频段静音
./VC-MultiBand-CLI-Standalone in.wav out.wav --mute-band 3 --mute-band 4
```

### 预设

| 预设名 | 说明 |
|--------|------|
| bypass | 旁路（直通） |
| de-ess | 去齿音：压缩高频段 |
| loudness-plus | 等响度补偿：提升低频和高频 |
| vocal-balance | 人声平衡：提升中高频，压缩低频 |
| solo-low | 独听低频段 |
| mute-high | 静音高频段 |

```bash
./VC-MultiBand-CLI-Standalone in.wav out.wav --preset de-ess
```

## LR4分频技术说明

### Linkwitz-Riley 4阶分频

LR4 = 两个相同的2阶Butterworth滤波器级联（Q=0.7071）：
- LP: 两个2阶Butterworth低通级联 → 24dB/oct衰减
- HP: 两个2阶Butterworth高通级联 → 24dB/oct衰减

### 幅度重建特性

LR4分频的LP+HP总和在幅度上完美重建（全频段0dB），但存在全通相位偏移。
这是标准行为，所有专业多段处理器均采用此方式。相位偏移在音频处理中不可察觉。

### 2阶Butterworth系数

```
omega = 2π·fc/fs
cosW = cos(omega), sinW = sin(omega)
alpha = sinW / (2·Q), Q = 0.7071

LP: b0=(1-cosW)/2, b1=1-cosW, b2=(1-cosW)/2
    a0=1+alpha, a1=-2·cosW, a2=1-alpha

HP: b0=(1+cosW)/2, b1=-(1+cosW), b2=(1+cosW)/2
    a0=1+alpha, a1=-2·cosW, a2=1-alpha
```

## 测试验证

```bash
# 编译
g++ -std=c++17 -DVC_STANDALONE -O2 \
    -I/tmp/AudioFX/Libs/dr_wav \
    Source/CLI_Standalone/main.cpp Source/DSP/VCPluginDSP.cpp \
    -o VC-MultiBand-CLI-Standalone -lm

# 功能测试
./VC-MultiBand-CLI-Standalone test.wav out.wav --preset bypass
./VC-MultiBand-CLI-Standalone test.wav out.wav --band-gain -6,+3,+2,-6
./VC-MultiBand-CLI-Standalone test.wav out.wav --solo-band 1
./VC-MultiBand-CLI-Standalone test.wav out.wav --mute-band 4
./VC-MultiBand-CLI-Standalone test.wav out.wav --band-threshold 0,0,-10,-20 --band-ratio 1,1,3,4
```

## 依赖

- **JUCE 8.0+**（用于VST3和JUCE CLI）
- **CMake 3.22+**
- **dr_wav**（已包含在 `../Libs/dr_wav/`）

## 许可

MIT License
