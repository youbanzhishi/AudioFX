# VC-PhaseScope

> 相位抵消检测插件 - 实时相位分析 + 离线CLI工具

## 功能特性

### 1. 相位相关度检测
- 实时显示立体声相位相关度 (-1 到 +1)
- +1 = 完全同相 | 0 = 不相关 | -1 = 完全反相

### 2. 相位矢量显示
- 极坐标/矢量显示
- 立体声宽度可视化

### 3. 底鼓/贝斯冲突检测
- 自动检测低频段 (20-150Hz) 的相位问题
- 四级告警：OK / Low / Medium / HIGH

### 4. CLI离线分析工具
- 支持WAV文件分析
- 输出详细相位报告

## 编译

### 依赖
- C++17 编译器
- CMake 3.15+
- JUCE 7.x (可选，用于VST3插件)

### 编译CLI工具
```bash
mkdir build && cd build
cmake ..
make
```

### 编译VST3插件（需要JUCE）
```bash
# 先安装JUCE
git clone https://github.com/juce-framework/JUCE.git
export JUCE_DIR=/path/to/JUCE

# 重新cmake
cmake -DJUCE_DIR=$JUCE_DIR ..
make
```

## 使用方法

### CLI工具
```bash
# 分析单个文件
./vc-phasescope-cli input.wav

# 输出到文件
./vc-phasescope-cli input.wav -o report.txt
```

### 输出示例
```
═══════════════════════════════════════════════════════
           VC-PhaseScope 分析报告
═══════════════════════════════════════════════════════

文件: input.wav
采样率: 44100 Hz
声道: 立体声

整体相位相关度: 0.85
相位状态: Mono
✓ 相位正常 - 立体声与单声道兼容性好

底鼓/贝斯状态: OK
✓ 正常 - 低频相位无明显问题
```

## 项目结构

```
AudioFX/
├── include/
│   └── VC-PhaseScope.h      # 核心DSP算法
├── src/
│   └── VC-PhaseScope-CLI/   # CLI工具
│       └── main.cpp
├── tests/                    # 测试
├── docs/                     # 文档
├── CMakeLists.txt           # 构建配置
└── README.md
```

## 算法说明

### 相位相关度
```
r = Σ(L[i] * R[i]) / (√(ΣL²) * √(ΣR²))
```

### 底鼓/贝斯冲突检测
1. IIR低通滤波提取低频成分 (20-150Hz)
2. 计算低频段相位相关度
3. 根据阈值判定冲突级别

## License

MIT
