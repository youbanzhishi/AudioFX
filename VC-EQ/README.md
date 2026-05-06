# VC-EQ

5段参数均衡器 VST3 插件，支持 DSP/CLI/VST3 三层架构。

## 项目结构

```
VC-EQ/
├── Source/
│   ├── DSP/
│   │   ├── VCEQDSP.h          ← DSP核心类，5段IIR滤波器
│   │   └── VCEQDSP.cpp
│   ├── CLI/
│   │   └── main.cpp           ← CLI入口
│   ├── PluginProcessor.cpp/h  ← VST3壳
│   └── PluginEditor.cpp/h
├── tests/
│   └── test_vceq_dsp.cpp      ← DSP单元测试
└── CMakeLists.txt
```

## 构建

```bash
cd VC-EQ
rm -rf build && mkdir build && cd build
cmake .. -DJUCE_PATH=/opt/JUCE -DCMAKE_BUILD_TYPE=Release
make -j4
```

### 构建产物

- **CLI 工具**: `build/CLI/VC-EQ-CLI`
- **单元测试**: `build/tests/VC-EQ-DSP-Tests`

### 运行测试

```bash
cd VC-EQ/build
ctest --output-on-failure
./tests/VC-EQ-DSP-Tests
```

## CLI 使用指南

### 基本用法

```bash
./VC-EQ-CLI <input.wav> <output.wav> [选项]
```

### 频段参数（band0-band4）

| 参数 | 说明 | 范围 |
|------|------|------|
| `--band<N>-freq <Hz>` | 频率 | 20-20000 Hz |
| `--band<N>-gain <dB>` | 增益 | -24 to +24 dB |
| `--band<N>-q <value>` | Q值 | 0.1-10 |
| `--band<N>-type <int>` | 类型 | 0=LowShelf, 1=HighShelf, 2=Peak |
| `--band<N>-on <0|1>` | 启用状态 | 0=关闭, 1=开启 |

### 默认频段配置

| 频段 | 默认频率 | 类型 |
|------|---------|------|
| Band 0 | 80 Hz | Low Shelf |
| Band 1 | 300 Hz | Peak |
| Band 2 | 1000 Hz | Peak |
| Band 3 | 3000 Hz | Peak |
| Band 4 | 8000 Hz | High Shelf |

### 预设

| 预设名 | 说明 |
|--------|------|
| `vocal-boost` | 人声增强，提亮高频 |
| `vocal-cut` | 人声削弱，降低齿音 |
| `warmth` | 温暖音色，增强低频 |
| `brightness` | 明亮音色，增强高频 |
| `flat` | 平坦响应 |

### 使用示例

```bash
# 使用人声增强预设
./VC-EQ-CLI input.wav output.wav --preset vocal-boost

# 自定义参数
./VC-EQ-CLI input.wav output.wav --band3-freq 3000 --band3-gain 2.5 --band4-freq 8000 --band4-gain 3

# 增加温暖感
./VC-EQ-CLI vocal.wav vocal-warm.wav --band0-freq 80 --band0-gain 2 --band1-freq 300 --band1-gain 1.5
```

## 许可证

MIT License
