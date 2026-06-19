# VC-Hall480 — Lexicon 480L-Class Algorithmic Reverb
# Based on JUCE 8 + CMake
# Supports: JUCE CLI + Standalone CLI + VST3 Plugin

cmake_minimum_required(VERSION 3.22)

# macOS deployment target - support macOS 10.15+ (Catalina)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
# Universal binary: arm64 (Apple Silicon) + x86_64 (Intel)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")

#==============================================================================
# Plugin Configuration
#==============================================================================
set(PLUGIN_NAME "VC-Hall480")
set(PLUGIN_CODE "VH48")
set(PLUGIN_VERSION "1.0.0")
set(MANUFACTURER_CODE "VCAU")

# JUCE Path - MUST be set before project()
set(JUCE_PATH "/opt/JUCE" CACHE PATH "Path to JUCE")

project(${PLUGIN_NAME} VERSION ${PLUGIN_VERSION} LANGUAGES C CXX)

# Import JUCE via add_subdirectory
add_subdirectory(${JUCE_PATH} JUCE-build)

#==============================================================================
# DSP Sources
#==============================================================================
set(DSP_SOURCES
    Source/DSP/VCPluginDSP.cpp
)

#==============================================================================
# Target 1: JUCE CLI
#==============================================================================
add_executable(${PLUGIN_NAME}-CLI Source/CLI/main.cpp ${DSP_SOURCES})

target_include_directories(${PLUGIN_NAME}-CLI PRIVATE
    ${JUCE_PATH}/modules
)

target_link_libraries(${PLUGIN_NAME}-CLI PRIVATE
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_core
    juce::juce_audio_basics
)

target_compile_definitions(${PLUGIN_NAME}-CLI PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_GTK=0
    JUCE_USE_CURL=0
)

set_target_properties(${PLUGIN_NAME}-CLI PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/CLI
)

#==============================================================================
# Target 2: Standalone CLI
#==============================================================================
add_executable(${PLUGIN_NAME}-CLI-Standalone Source/CLI_Standalone/main.cpp ${DSP_SOURCES})

target_include_directories(${PLUGIN_NAME}-CLI-Standalone PRIVATE
    ${CMAKE_SOURCE_DIR}/../Libs/dr_wav
)

target_compile_definitions(${PLUGIN_NAME}-CLI-Standalone PRIVATE
    VC_STANDALONE
)

target_link_libraries(${PLUGIN_NAME}-CLI-Standalone PRIVATE)

set_target_properties(${PLUGIN_NAME}-CLI-Standalone PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/CLI_Standalone
)

#==============================================================================
# Target 3: VST3 Plugin
#==============================================================================
juce_add_plugin(${PLUGIN_NAME}
    FORMATS VST3
    NAME "${PLUGIN_NAME}"
    PLUGIN_MANUFACTURER_CODE ${MANUFACTURER_CODE}
    PLUGIN_CODE ${PLUGIN_CODE}
    VERSION ${PLUGIN_VERSION}
    HAS_EMBEDDED_TARGET false
    COPY_PLUGIN_AFTER_BUILD TRUE
    PRODUCT_NAME "${PLUGIN_NAME}"
)

target_sources(${PLUGIN_NAME} PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${DSP_SOURCES}
)

target_link_libraries(${PLUGIN_NAME} PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

target_compile_definitions(${PLUGIN_NAME} PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_GTK=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

set_target_properties(${PLUGIN_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    JUCE_VST3_COPY_DIR "$ENV{HOME}/.vst3"
)

#==============================================================================
# Common Compiler Settings
#==============================================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()
