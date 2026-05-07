# VC-Smooth VST3/CLI Plugin - Spectral Resonance Suppressor
# DSP/CLI/VST3 三层架构

cmake_minimum_required (VERSION 3.22)

# JUCE 路径 - 必须放在 project() 之前！
set (JUCE_PATH "/opt/JUCE" CACHE PATH "Path to JUCE")

project (VC-Smooth VERSION 1.0.0 LANGUAGES C CXX)

# 导入 JUCE
add_subdirectory (${JUCE_PATH} JUCE-build)

# DSP 源文件
set (DSP_SOURCES Source/DSP/VCSmoothDSP.cpp)

#==============================================================================
# CLI Target (JUCE version)
#==============================================================================
add_executable (VC-Smooth-CLI Source/CLI/main.cpp ${DSP_SOURCES})
target_include_directories (VC-Smooth-CLI PRIVATE ${JUCE_PATH}/modules)
target_include_directories (VC-Smooth-CLI PRIVATE Source)
target_link_libraries (VC-Smooth-CLI PRIVATE 
    juce::juce_audio_formats 
    juce::juce_dsp 
    juce::juce_core)
target_compile_definitions (VC-Smooth-CLI PRIVATE 
    JUCE_WEB_BROWSER=0 
    JUCE_USE_GTK=0 
    JUCE_USE_CURL=0 
    JUCE_VST3_CAN_REPLACE_VST2=0)
set_target_properties (VC-Smooth-CLI PROPERTIES 
    CXX_STANDARD 17 
    CXX_STANDARD_REQUIRED ON 
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/CLI)

#==============================================================================
# Standalone CLI (no JUCE dependency)
#==============================================================================
add_executable (VC-Smooth-CLI-Standalone Source/CLI_Standalone/main.cpp ${DSP_SOURCES})
target_include_directories (VC-Smooth-CLI-Standalone PRIVATE 
    ${CMAKE_SOURCE_DIR}/../Libs/dr_wav
)
target_compile_definitions (VC-Smooth-CLI-Standalone PRIVATE VC_STANDALONE)
target_link_libraries (VC-Smooth-CLI-Standalone PRIVATE)  # No extra dependencies
set_target_properties (VC-Smooth-CLI-Standalone PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/CLI_Standalone
)

#==============================================================================
# VST3 Plugin Target (默认关闭，需要 -DBUILD_VST3=ON 开启)
#==============================================================================
option(BUILD_VST3 "Build VST3 plugin" OFF)

if(BUILD_VST3)
    juce_add_plugin (VC-Smooth
        FORMATS VST3
        NAME "VC-Smooth"
        PLUGIN_MANUFACTURER_CODE VC
        PLUGIN_CODE VCSM
        VERSION 1.0.0
        HAS_EMBEDDED_TARGET false
        COPY_PLUGIN_AFTER_BUILD TRUE
        PRODUCT_NAME "VC-Smooth"
    )
    
    target_sources (VC-Smooth PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        ${DSP_SOURCES}
    )
    
    target_link_libraries (VC-Smooth PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
    )
    
    target_compile_definitions (VC-Smooth PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_GTK=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
    
    set_target_properties (VC-Smooth PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        JUCE_VST3_COPY_DIR "$ENV{HOME}/.vst3"
    )
endif()
