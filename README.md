# VocalChain

> 基于 JUCE 框架的 VST3 音频插件集合

## 插件列表

### VC-EQ
参数化均衡器插件，提供专业级的音频频段调节能力。

### VC-Comp
压缩器插件，用于动态范围控制。

## 技术特性

- **框架**: JUCE 8.0
- **格式**: VST3 (macOS/Windows/Linux)
- **额外**: macOS 支持 Audio Unit (AU) 格式
- **标准**: C++17

## 系统要求

- **macOS**: 10.14+
- **Windows**: Windows 10/11
- **Linux**: Ubuntu 18.04+

## 构建说明

### 使用 GitHub Actions（推荐）

本项目配置了 GitHub Actions 自动化构建，push 到 main 分支后会自动构建三个平台的插件。

构建产物在 Actions 页面下载：
1. 进入 Actions 标签页
2. 选择 "Build VST3 Plugins"
3. 查看运行状态，完成后下载 Artifact

### 本地构建

```bash
# 克隆 JUCE
git clone --depth 1 --branch 8.0.0 https://github.com/juce-framework/JUCE.git

# 构建 VC-EQ
mkdir -p VC-EQ/build && cd VC-EQ/build
cmake .. -DJUCE_PATH=../../JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# 构建 VC-Comp
mkdir -p VC-Comp/build && cd VC-Comp/build
cmake .. -DJUCE_PATH=../../JUCE -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## 目录结构

```
VocalChain/
├── .github/
│   └── workflows/
│       └── build.yml          # GitHub Actions 配置
├── VC-EQ/
│   ├── CMakeLists.txt
│   └── Source/
│       ├── PluginProcessor.h/cpp
│       └── PluginEditor.h/cpp
└── VC-Comp/
    ├── CMakeLists.txt
    └── Source/
        ├── PluginProcessor.h/cpp
        └── PluginEditor.h/cpp
```

## CI/CD 平台支持

| 平台 | 状态 | 插件格式 |
|------|------|----------|
| macOS | ✅ | VST3, AU |
| Windows | ✅ | VST3 |
| Linux | ✅ | VST3 |

## 许可证

MIT License
