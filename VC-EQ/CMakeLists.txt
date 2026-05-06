# VC-EQ VST3 Plugin
# 必须声明 LANGUAGES C CXX 否则会导致 juceaide CMAKE_C_COMPILE_OBJECT 错误！

cmake_minimum_required (VERSION 3.22)

# JUCE 路径 - 必须放在 project() 之前！
set (JUCE_PATH "/opt/JUCE" CACHE PATH "Path to JUCE")

project (VC-EQ VERSION 1.0.0 LANGUAGES C CXX)

# 导入 JUCE（通过 add_subdirectory）
add_subdirectory (${JUCE_PATH} JUCE-build)

# JUCE 插件配置
juce_add_plugin (VC-EQ
    # 插件格式
    FORMATS VST3
    # 插件名称
    NAME "VC-EQ"
    # 插件标识符
    PLUGIN_MANUFACTURER_CODE VC
    PLUGIN_CODE VCEQ
    # 版本
    VERSION 1.0.0
    # 是否需要 JUCE ResizableLogoArea（否）
    HAS_EMBEDDED_TARGET false
    # 是否复制插件到安装目录
    COPY_PLUGIN_AFTER_BUILD TRUE
)

# 包含源文件
target_sources (VC-EQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# 链接 JUCE 模块 - 添加 juce_dsp 模块
target_link_libraries (VC-EQ PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

# 禁用 VST2、GTK 和 curl 相关功能
target_compile_definitions (VC-EQ PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_GTK=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# C++ 标准
set_target_properties (VC-EQ PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# VST3 输出目录
set_target_properties (VC-EQ PROPERTIES
    JUCE_VST3_COPY_DIR "$ENV{HOME}/.vst3"
)
