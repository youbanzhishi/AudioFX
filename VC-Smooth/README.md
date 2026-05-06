# VC-Smooth

频谱共振峰平滑器 - Spectral Resonance Smoother

VC-Smooth 是一款基于 FFT 的频谱处理插件，用于检测和抑制音频信号中的共振峰（Formants）。对标 Soothe2、Reso、DSEQ 等专业插件。

## 架构

本项目采用 **DSP/CLI/VST3 三层分离架构**：

```
VC-Smooth/
├── Source/
│   ├── DSP/
│   │   ├── VCSmoothDSP.h      ← 独立 DSP 核心类
│   │   └── VCSmoothDSP.cpp
│   ├── CLI/
│   │   └── main.cpp           ← 命令行处理工具
│   ├── PluginProcessor.cpp/h  ← VST3 壳，调用 DSP
│   └── PluginEditor.cpp/h     ← GUI 编辑器
├── CMakeLists.txt
└── README.md
```

- **DSP 层** (`VCSmoothDSP`)：纯信号处理逻辑，可独立测试和复用
- **CLI 层** (`main.cpp`)：命令行批处理工具，用于离线处理
- **VST3 层** (`PluginProcessor`)：插件壳，提供 GUI 和 DAW 集成

## 构建

### CLI 工具（推荐）

CLI 工具可以独立构建，无需 GUI 依赖：

```bash
cd /tmp/AudioFX/VC-Smooth
mkdir build && cd build
cmake -DBUILD_VST3=OFF ..
make VC-Smooth-CLI
```

### VST3 插件

```bash
cd /tmp/AudioFX/VC-Smooth
mkdir build && cd build
cmake -DBUILD_VST3=ON ..
make
```

## CLI 使用指南

### 基本用法

```bash
VC-Smooth-CLI input.wav output.wav
```

### 参数选项

| 参数 | 默认值 | 范围 | 说明 |
|------|--------|------|------|
| `--depth` | 0.5 | 0-1 | 平滑深度 |
| `--speed` | 2.0 | 0.1-10 | 包络跟踪速度 |
| `--freq-low` | 200 | Hz | 低频边界 |
| `--freq-high` | 16000 | Hz | 高频边界 |
| `--sharpness` | 1.5 | 0.1-5 | 锐度/阈值系数 |
| `--mix` | 1.0 | 0-1 | 干湿比 |
| `--input-gain` | 0 | dB | 输入增益 |
| `--output-gain` | 0 | dB | 输出增益 |

### 示例

```bash
# 轻度平滑
VC-Smooth-CLI in.wav out.wav --depth 0.3 --speed 1.0

# 中度平滑
VC-Smooth-CLI in.wav out.wav --depth 0.6 --speed 2.5

# 仅处理人声频段
VC-Smooth-CLI in.wav out.wav --freq-low 300 --freq-high 4000

# 保留部分原音（50% mix）
VC-Smooth-CLI in.wav out.wav --mix 0.5 --depth 0.8
```

### 批处理

```bash
# 处理目录下所有 WAV 文件
for f in *.wav; do
    VC-Smooth-CLI "$f" "smoothed_$f" --depth 0.5
done
```

## VST3 参数说明

| 参数 | 说明 |
|------|------|
| **Depth** | 共振峰压缩深度，值越大抑制越强 |
| **Speed** | 包络跟踪速度，影响响应速度 |
| **Freq Low/High** | 处理的频率范围 |
| **Sharpness** | 锐度系数，控制检测阈值 |
| **Mix** | 干湿比，0=完全干声，1=完全处理后 |
| **Input/Output** | 输入输出增益 |

## 算法原理

VC-Smooth 使用 FFT 频谱分析进行共振峰检测和抑制：

1. **短时傅里叶变换 (STFT)**：使用 4096 点 FFT，75% 重叠
2. **包络跟踪**：使用指数移动平均跟踪频谱包络
3. **峰值检测**：检测超过包络的共振峰
4. **增益计算**：对检测到的峰值应用压缩增益
5. **时域平滑**：平滑增益变化避免 artifacts
6. **逆变换 (ISTFT)**：Overlap-Add 重构时域信号

## 技术规格

- FFT 大小：4096 点 (~93ms @ 44100Hz)
- Hop 大小：1024 点 (75% 重叠)
- 延迟：2048 样本 (~46ms)
- 格式：VST3 (macOS/Windows/Linux)

## 许可证

MIT License

## 相关项目

- [VC-EQ](https://github.com/youbanzhishi/AudioFX/tree/main/VC-EQ) - 5 段参数均衡器
