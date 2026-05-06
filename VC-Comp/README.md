# VC-Comp

侧链压缩器插件，对标 Waves Renaissance Compressor。

## 架构

采用 DSP/CLI/VST3 三层分离架构：

```
VC-Comp/
├── Source/
│   ├── DSP/
│   │   ├── VCCompDSP.h      # DSP核心类
│   │   └── VCCompDSP.cpp
│   ├── CLI/
│   │   └── main.cpp         # 命令行入口
│   ├── PluginProcessor.cpp/h # VST3处理器
│   └── PluginEditor.cpp/h   # VST3编辑器
├── CMakeLists.txt
└── README.md
```

## 编译

### CLI

```bash
cd build
cmake .. && make VC-Comp-CLI
```

### VST3

```bash
cd build
cmake -DBUILD_VST3=ON .. && make VC-Comp
```

## CLI 使用指南

### 基本用法

```bash
VC-Comp-CLI input.wav output.wav [选项]
```

### 参数选项

| 参数 | 说明 | 默认值 | 范围 |
|------|------|--------|------|
| `--threshold <dB>` | 阈值 | 0 | -60 ~ 0 |
| `--ratio <ratio>` | 压缩比 | 1.0 | 0.5 ~ 50 |
| `--attack <ms>` | 启动时间 | 16 | 0.5 ~ 500 |
| `--release <ms>` | 释放时间 | 160 | 5 ~ 5000 |
| `--gain <dB>` | Makeup增益 | 0 | -30 ~ 30 |
| `--knee <mode>` | 拐点模式 | soft | hard/soft/auto |
| `--character <mode>` | 特性 | warm | clean/warm |
| `--mix <%>` | 干湿比 | 100 | 0 ~ 100 |
| `--trim <dB>` | Trim输出 | 0 | -18 ~ 18 |

### 预设

| 预设 | 说明 |
|------|------|
| `vocal-1db` | 轻微压缩，适合 vocals |
| `vocal-3db` | 中度压缩 |
| `vocal-6db` | 强力压缩 |
| `drums` | 鼓组压缩 |
| `bass` | 低音压缩 |
| `limiter` | 限制模式 |

### 示例

```bash
# 使用参数
VC-Comp-CLI in.wav out.wav --threshold -20 --ratio 3 --attack 10

# 使用预设
VC-Comp-CLI in.wav out.wav --preset vocal-3db

# 组合使用
VC-Comp-CLI in.wav out.wav --preset drums --gain 2
```

## VST3 参数

### A/B 参数组

支持两组独立参数，可通过 `Param Set` 切换。

### 侧链

- **SC Source**: Internal (使用输入信号) / External (使用侧链输入)
- **SC HPF**: 侧链高通滤波器频率 (Off, 60/100/200/500 Hz)
- **SC Listen**: 监听侧链信号

### Release Mode

- **ARC**: 自适应释放时间
- **Manual**: 手动设置释放时间

### Comp Behavior

- **Electro**: 电吉他风格，快速响应
- **Opto**: 光电压缩风格，平滑响应

### Character

- **Warm**: 添加温暖的谐波失真
- **Smooth**: 干净透明的处理

## DSP 接口

```cpp
#include "DSP/VCCompDSP.h"

VCCompDSP dsp;
dsp.prepare(sampleRate, blockSize);

VCCompDSP::Params p;
p.threshold = -20.0f;
p.ratio = 3.0f;
p.attack = 10.0f;
p.release = 100.0f;
// ... 其他参数
dsp.setParams(p);

dsp.process(left, right, numSamples);

float gr = dsp.getGainReduction(); // 获取增益减少量
```
