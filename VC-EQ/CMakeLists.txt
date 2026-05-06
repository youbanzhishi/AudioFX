# VC-EQ DSP/CLI/VST3 Architecture
cmake_minimum_required (VERSION 3.22)
set (JUCE_PATH "/opt/JUCE" CACHE PATH "Path to JUCE")
project (VC-EQ VERSION 1.0.0 LANGUAGES C CXX)
add_subdirectory (${JUCE_PATH} JUCE-build)

set (DSP_SOURCES Source/DSP/VCEQDSP.cpp)

# CLI Target (JUCE version)
add_executable (VC-EQ-CLI Source/CLI/main.cpp ${DSP_SOURCES})
target_include_directories (VC-EQ-CLI PRIVATE ${JUCE_PATH}/modules)
target_link_libraries (VC-EQ-CLI PRIVATE juce::juce_audio_formats juce::juce_dsp juce::juce_core)
target_compile_definitions (VC-EQ-CLI PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_GTK=0 JUCE_USE_CURL=0)
set_target_properties (VC-EQ-CLI PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/CLI)

# Standalone CLI (no JUCE dependency)
add_executable (VC-EQ-CLI-Standalone Source/CLI_Standalone/main.cpp ${DSP_SOURCES})
target_include_directories (VC-EQ-CLI-Standalone PRIVATE 
    ${CMAKE_SOURCE_DIR}/Libs/dr_wav
)
target_compile_definitions (VC-EQ-CLI-Standalone PRIVATE VC_STANDALONE)
target_link_libraries (VC-EQ-CLI-Standalone PRIVATE)  # No extra dependencies
set_target_properties (VC-EQ-CLI-Standalone PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/CLI_Standalone
)

# Unit Tests
enable_testing()
add_executable (VC-EQ-DSP-Tests tests/test_vceq_dsp.cpp ${DSP_SOURCES})
target_include_directories (VC-EQ-DSP-Tests PRIVATE ${JUCE_PATH}/modules)
target_link_libraries (VC-EQ-DSP-Tests PRIVATE juce::juce_dsp juce::juce_core)
target_compile_definitions (VC-EQ-DSP-Tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_GTK=0 JUCE_USE_CURL=0)
set_target_properties (VC-EQ-DSP-Tests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
add_test (NAME VC-EQ-DSP-Tests COMMAND VC-EQ-DSP-Tests)

# VST3 (optional)
option(BUILD_VST3 "Build VST3 plugin" OFF)
if(BUILD_VST3)
    juce_add_plugin (VC-EQ FORMATS VST3 NAME "VC-EQ" PLUGIN_MANUFACTURER_CODE VCAU PLUGIN_CODE VCEQ VERSION 1.0.0 HAS_EMBEDDED_TARGET false COPY_PLUGIN_AFTER_BUILD TRUE)
    target_sources (VC-EQ PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp ${DSP_SOURCES})
    target_link_libraries (VC-EQ PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
    target_compile_definitions (VC-EQ PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_GTK=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    set_target_properties (VC-EQ PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON JUCE_VST3_COPY_DIR "$ENV{HOME}/.vst3")
endif()
