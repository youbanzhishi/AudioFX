# VC-Plugin-Template - VST3 Audio Plugin Template
# 基于 JUCE 8 + CMake 构建系统

cmake_minimum_required (VERSION 3.22)

# ============================================================
# 插件名称配置 - 修改这里以创建新插件
# ============================================================
set (PLUGIN_NAME "VC-Template")
set (PLUGIN_CODE "VCTP")
set (PLUGIN_VERSION "1.0.0")

# JUCE 路径 - 必须放在 project() 之前！
set (JUCE_PATH "/opt/JUCE" CACHE PATH "Path to JUCE")

project (${PLUGIN_NAME} VERSION ${PLUGIN_VERSION} LANGUAGES C CXX)

# 导入 JUCE（通过 add_subdirectory）
add_subdirectory (${JUCE_PATH} JUCE-build)

# ============================================================
# JUCE 插件配置
# ============================================================
juce_add_plugin (${PLUGIN_NAME}
    # 插件格式
    FORMATS VST3
    # 插件名称
    NAME "${PLUGIN_NAME}"
    # 插件标识符 (Manufacturer Code + Plugin Code)
    PLUGIN_MANUFACTURER_CODE VC
    PLUGIN_CODE ${PLUGIN_CODE}
    # 版本
    VERSION ${PLUGIN_VERSION}
    # 是否需要 JUCE ResizableLogoArea（否）
    HAS_EMBEDDED_TARGET false
    # 是否复制插件到安装目录
    COPY_PLUGIN_AFTER_BUILD TRUE
    # 产品名称（可选）
    PRODUCT_NAME "${PLUGIN_NAME}"
)

# ============================================================
# 源文件
# ============================================================
target_sources (${PLUGIN_NAME} PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# ============================================================
# 链接 JUCE 模块
# ============================================================
target_link_libraries (${PLUGIN_NAME} PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

# ============================================================
# 禁用不需要的功能
# ============================================================
target_compile_definitions (${PLUGIN_NAME} PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_GTK=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# ============================================================
# C++ 标准配置
# ============================================================
set_target_properties (${PLUGIN_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# ============================================================
# VST3 输出目录
# ============================================================
set_target_properties (${PLUGIN_NAME} PROPERTIES
    JUCE_VST3_COPY_DIR "$ENV{HOME}/.vst3"
)
