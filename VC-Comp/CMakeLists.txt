# VC-Comp VST3 Plugin - Compression with Sidechain
# 对标 Waves Renaissance Compressor

cmake_minimum_required (VERSION 3.22)

# JUCE 路径 - 必须放在 project() 之前！
set (JUCE_PATH "/opt/JUCE" CACHE PATH "Path to JUCE")

project (VC-Comp VERSION 1.0.0 LANGUAGES C CXX)

# 导入 JUCE
add_subdirectory (${JUCE_PATH} JUCE-build)

# JUCE 插件配置
juce_add_plugin (VC-Comp
    FORMATS VST3
    NAME "VC-Comp"
    PLUGIN_MANUFACTURER_CODE VC
    PLUGIN_CODE VCCM
    VERSION 1.0.0
    HAS_EMBEDDED_TARGET false
    COPY_PLUGIN_AFTER_BUILD TRUE
    PRODUCT_NAME "VC-Comp"
)

# 包含源文件
target_sources (VC-Comp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# 链接 JUCE 模块
target_link_libraries (VC-Comp PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

# 禁用不需要的功能
target_compile_definitions (VC-Comp PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_GTK=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# C++ 标准
set_target_properties (VC-Comp PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# VST3 输出目录
set_target_properties (VC-Comp PROPERTIES
    JUCE_VST3_COPY_DIR "$ENV{HOME}/.vst3"
)
