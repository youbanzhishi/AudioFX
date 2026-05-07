# VC-Plugin-Template

基于 **JUCE 8** + **CMake** 的 VST3 音频插件开发模板。包含完整的 **三层架构**（DSP/CLI/VST3）和 **双 CLI**（JUCE版本 + 零依赖Standalone版本）。

## 核心特性

- **三层架构**: DSP核心层 → CLI命令行层 → VST3插件层
- **双CLI支持**: 
  - `VC-Plugin-CLI`: 使用JUCE读写WAV（需要链接JUCE库）
  - `VC-Plugin-CLI-Standalone`: 使用dr_wav（零外部依赖）
- **条件编译**: DSP代码可在JUCE模式和Standalone模式间切换
- **CI/CD就绪**: 包含GitHub Actions工作流配置

## 目录结构

```
VC-Plugin-Template/
├── CMakeLists.txt                      # 构建配置（3个target）
├── .gitignore                           # Git忽略规则
├── README.md                            # 本文件
├── Source/
│   ├── DSP/
│   │   ├── VCPluginDSP.h               # DSP核心头文件
│   │   └── VCPluginDSP.cpp             # DSP核心实现
│   ├── CLI/
│   │   └── main.cpp                    # JUCE版CLI（依赖JUCE）
│   ├── CLI_Standalone/
│   │   └── main.cpp                    # Standalone版CLI（dr_wav）
│   ├── PluginProcessor.h               # VST3处理器头文件
│   ├── PluginProcessor.cpp              # VST3处理器实现
│   ├── PluginEditor.h                   # VST3编辑器头文件
│   └── PluginEditor.cpp                # VST3编辑器实现
└── .github/workflows/                   # CI/CD配置（从根目录同步）
```

## 快速开始

### 1. 复制模板

```bash
cd /tmp/AudioFX
cp -r VC-Plugin-Template VC-MyPlugin
cd VC-MyPlugin
```

### 2. 替换占位符

**必须替换的占位符**（在所有`.cpp`和`.h`文件中）：

| 占位符 | 说明 | 示例 |
|--------|------|------|
| `__PLUGIN_NAME__` | 插件名称 | `VC-MyPlugin` |
| `__PLUGIN_CODE__` | 4字符插件代码 | `VCMP` |
| `__MANUFACTURER_CODE__` | 4字符厂商代码 | `VCAU` |

**自动替换命令**：

```bash
# 定义变量
PLUGIN_NAME="VC-MyPlugin"
PLUGIN_CODE="VCMP"
MANUFACTURER_CODE="VCAU"

# 替换所有占位符
find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "CMakeLists.txt" \) \
    -exec sed -i "s/__PLUGIN_NAME__/${PLUGIN_NAME}/g" {} \;
find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "CMakeLists.txt" \) \
    -exec sed -i "s/__PLUGIN_CODE__/${PLUGIN_CODE}/g" {} \;
find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "CMakeLists.txt" \) \
    -exec sed -i "s/__MANUFACTURER_CODE__/${MANUFACTURER_CODE}/g" {} \;
```

### 3. 实现DSP算法

在 `Source/DSP/VCPluginDSP.h/cpp` 中实现你的DSP算法：

```cpp
// VCPluginDSP.h - 定义参数结构
struct Params {
    float gainDB = 0.0f;      // 修改为你的参数
    float mix = 100.0f;
    bool enabled = true;
};

// VCPluginDSP.cpp - 实现process()或processIIR()
```

### 4. 编译

#### 方式一：编译所有目标

```bash
mkdir build && cd build
cmake .. -DJUCE_PATH=/opt/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

#### 方式二：只编译特定目标

```bash
# 只编译Standalone CLI（最快，无需JUCE依赖）
mkdir build && cd build
cmake .. -DJUCE_PATH=/opt/JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build . --target VC-MyPlugin-CLI-Standalone

# 只编译VST3插件
cmake --build . --target VC-MyPlugin
```

## 使用CLI

### Standalone CLI（推荐）

```bash
# 基本用法
./build/CLI_Standalone/VC-MyPlugin-CLI-Standalone input.wav output.wav

# 使用预设
./build/CLI_Standalone/VC-MyPlugin-CLI-Standalone input.wav output.wav --preset bypass

# 自定义参数
./build/CLI_Standalone/VC-MyPlugin-CLI-Standalone input.wav output.wav --gain 6.0 --mix 75

# 帮助
./build/CLI_Standalone/VC-MyPlugin-CLI-Standalone --help
```

### JUCE CLI

```bash
./build/CLI/VC-MyPlugin-CLI input.wav output.wav --gain 3.0
```

## JUCE 8 踩坑经验汇总

### ⚠️ 重要注意事项

1. **inputBuses[] 返回值非引用**
   ```cpp
   // ❌ 错误：编译报错
   auto& inputLayout = layouts.inputBuses[0];
   
   // ✅ 正确：按值捕获
   const auto inputLayout = layouts.inputBuses[0];
   ```

2. **createWriterFor 需要6个参数**
   ```cpp
   // ❌ 错误：只有5个参数
   writer = wavFmt.createWriterFor(stream, sampleRate, channels, 32, {});
   
   // ✅ 正确：6个参数，第5个是StringPairArray，第6个quality传0
   writer = wavFmt.createWriterFor(stream, sampleRate, channels, 32, {}, 0);
   ```

3. **metadata 类型是 StringPairArray，不是 StringArray**
   ```cpp
   // ❌ 错误：StringArray不是正确类型
   StringArray metadata;
   
   // ✅ 正确：空的大括号会创建StringPairArray
   writer = wavFmt.createWriterFor(..., {});
   ```

4. **BusesProperties 必须传基类构造函数**
   ```cpp
   // ❌ 错误：局部变量
   BusesProperties buses;
   buses.withInput(...);
   
   // ✅ 正确：传基类构造函数
   AudioProcessor(BusesProperties()
       .withInput("Input", AudioChannelSet::stereo())
       .withOutput("Output", AudioChannelSet::stereo()))
   ```

5. **AudioBlock 缓冲区必须非交错布局**
   ```cpp
   // ✅ 正确：先复制到非交错缓冲区
   float* leftBuf = mInternalBuffer.data();
   float* rightBuf = mInternalBuffer.data() + numSamples;
   for (int i = 0; i < numSamples; ++i) {
       leftBuf[i] = left[i];
       rightBuf[i] = right[i];
   }
   juce::dsp::AudioBlock<float> block(mInternalPtrs.data(), 2, numSamples);
   
   // ❌ 错误：直接用交错缓冲区假装非交错
   ```

6. **getInputChannelSet(1) 已在JUCE 8中移除**
   ```cpp
   // ❌ 错误：方法不存在
   layouts.getInputChannelSet(1);
   
   // ✅ 正确：使用inputBuses[1]
   if (layouts.inputBuses.size() > 1) {
       auto sidechain = layouts.inputBuses[1];
   }
   ```

## 条件编译

DSP代码支持JUCE和Standalone两种编译模式：

```cpp
#ifdef VC_STANDALONE
// Standalone模式：使用标准库，无JUCE依赖
#include <vector>
#include <cmath>
#else
// JUCE模式：使用JUCE DSP模块
#include <juce_dsp/juce_dsp.h>
#endif
```

## CI/CD

模板已配置GitHub Actions工作流：

- **多平台构建**: macOS, Windows, Ubuntu
- **自动化测试**: CLI功能测试
- **发布管理**: 支持GitHub Release

详见 `.github/workflows/build.yml`（从仓库根目录同步）。

## 依赖

- **JUCE 8.0+**（用于VST3和JUCE CLI）
- **CMake 3.22+**
- **dr_wav**（已包含在 `../Libs/dr_wav/`）
- 支持VST3的DAW（REAPER, Ableton Live, Cubase等）

## 参考项目

- [VC-EQ](https://github.com/youbanzhishi/AudioFX) - 5频段参量均衡器
- [VC-Comp](https://github.com/youbanzhishi/AudioFX) - 侧链压缩器
- [VC-Smooth](https://github.com/youbanzhishi/AudioFX) - 频谱共振抑制器

## 许可

MIT License
