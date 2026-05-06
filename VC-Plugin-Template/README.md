# VC-Plugin-Template

基于 JUCE 8 + CMake 的 VST3 音频插件开发模板。

## 快速开始

### 1. 创建新插件

```bash
# 复制模板
cp -r VC-Plugin-Template VC-MyPlugin

# 进入目录
cd VC-MyPlugin

# 编辑 CMakeLists.txt，设置插件名称和代码
# 修改 PLUGIN_NAME, PLUGIN_CODE
```

### 2. 配置插件信息

编辑 `CMakeLists.txt`:
```cmake
set (PLUGIN_NAME "VC-MyPlugin")
set (PLUGIN_CODE "VCMP")  # 4字符插件代码
```

### 3. 替换模板占位符

```bash
# 替换所有 [[PLUGIN_NAME]] 为你的插件名
find . -type f \( -name "*.h" -o -name "*.cpp" \) \
    -exec sed -i 's/\[\[PLUGIN_NAME\]\]/VC-MyPlugin/g' {} \;
```

### 4. 编译

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j$(nproc)
```

插件将自动安装到 `~/.vst3/`

## 目录结构

```
VC-Plugin-Template/
├── CMakeLists.txt          # 构建配置
├── Source/
│   ├── PluginProcessor.h   # 处理器头文件
│   ├── PluginProcessor.cpp # 处理器实现
│   ├── PluginEditor.h      # 编辑器头文件
│   └── PluginEditor.cpp    # 编辑器实现
└── README.md               # 本文件
```

## 模板说明

### PluginProcessor.h
- 定义参数 ID 命名空间
- 定义处理器类，继承 `juce::AudioProcessor`
- 包含参数监听接口 `AudioProcessorValueTreeState::Listener`
- 预留 DSP 成员变量位置

### PluginProcessor.cpp
- 构造函数：注册参数监听器
- `createParameterLayout()`: 定义所有参数
- `prepareToPlay()`: 准备 DSP 处理
- `processBlock()`: 主处理循环
- `parameterChanged()`: 参数变化回调

### PluginEditor.h/cpp
- 继承 `juce::AudioProcessorEditor`
- 使用 `AudioProcessorValueTreeState::Attachment` 绑定控件

## 常用 JUCE 参数类型

```cpp
// 浮点参数
std::make_unique<AudioParameterFloat>(
    "param_id", "Display Name",
    NormalisableRange<float>(min, max), defaultValue,
    "unit");

// 整数参数
std::make_unique<AudioParameterInt>(
    "param_id", "Display Name",
    min, max, defaultValue);

// 布尔参数
std::make_unique<AudioParameterBool>(
    "param_id", "Display Name", defaultValue);

// 枚举参数
std::make_unique<AudioParameterChoice>(
    "param_id", "Display Name",
    StringArray{"Option1", "Option2", "Option3"}, defaultIndex);
```

## 示例：添加 Gain 参数

### 1. 在 PluginProcessor.h 中添加:

```cpp
namespace ParameterIDs {
    static const juce::String gain = "gain";
}

// 在类中添加成员变量
float mGain = 1.0f;
```

### 2. 在 PluginProcessor.cpp 中:

```cpp
// 构造函数中注册监听
mAPVTS.addParameterListener(ParameterIDs::gain, this);

// createParameterLayout 中添加
params.push_back(std::make_unique<AudioParameterFloat>(
    ParameterIDs::gain, "Gain",
    NormalisableRange<float>(-24.0f, 24.0f), 0.0f, "dB"));

// parameterChanged 中处理
else if (parameterID == ParameterIDs::gain)
{
    mGain = Decibels::decibelsToGain(newValue);
}

// processBlock 中使用
buffer.applyGain(mGain);
```

### 3. 在 PluginEditor.cpp 中添加控件:

```cpp
juce::Slider gainSlider;
std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

// 构造函数中
addAndMakeVisible(gainSlider);
gainSlider.setSliderStyle(Slider::Rotary);
gainSlider.setColour(Slider::rotarySliderFillColourId, Colours::cyan);
gainAttachment.reset(new AudioProcessorValueTreeState::SliderAttachment(
    processor.getAPVTS(), ParameterIDs::gain, gainSlider));

// resized 中
gainSlider.setBounds(20, 80, 70, 70);
```

## 依赖

- JUCE 8.0+
- CMake 3.22+
- 支持 VST3 的 DAW (如 REAPER, Ableton Live, Cubase 等)

## 参考项目

- [VC-EQ](https://github.com/youbanzhishi/AudioFX) - 5频段参量均衡器
- [VC-Comp](https://github.com/youbanzhishi/AudioFX) - 压缩器
- [VC-Smooth](https://github.com/youbanzhishi/AudioFX) - 频谱共振抑制器
