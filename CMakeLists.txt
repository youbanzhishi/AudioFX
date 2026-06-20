cmake_minimum_required(VERSION 3.15)
project(VC-PhaseScope VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找JUCE（可选，用于VST插件编译）
find_package(JUCE CONFIG QUIET)
set(HAS_JUCE ${JUCE_FOUND})

# Core DSP库
add_library(audiofx_core INTERFACE)
target_include_directories(audiofx_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_sources(audiofx_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/VC-PhaseScope.h
)

# CLI工具
add_executable(vc-phasescope-cli
    src/VC-PhaseScope-CLI/main.cpp
)
target_link_libraries(vc-phasescope-cli audiofx_core)
target_include_directories(vc-phasescope-cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# VST3插件（需要JUCE）
if(JUCE_FOUND)
    message(STATUS "JUCE found - building VST3 plugin")
    
    # 添加VST3插件源文件
    set(VST3_SOURCES
        src/VC-PhaseScope/Plugin.cpp
        src/VC-PhaseScope/ProjectInfo.h
    )
    
    # JUCE插件相关设置
    add_library(VC-PhaseScope MODULE ${VST3_SOURCES})
    target_link_libraries(VC-PhaseScope PRIVATE
        juce::juce_core
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_audio_utils
    )
    
    # 设置插件元数据
    set_target_properties(VC-PhaseScope PROPERTIES
        PREFIX ""
        SUFFIX ".so"
    )
else()
    message(STATUS "JUCE not found - VST3 plugin disabled")
    message(STATUS "Install JUCE to enable VST3 plugin: https://juce.com/")
endif()

# 测试
enable_testing()
add_test(NAME PhaseCorrelationTest COMMAND vc-phasescope-cli)

# 安装
install(TARGETS vc-phasescope-cli DESTINATION bin)
install(FILES include/VC-PhaseScope.h DESTINATION include/audiofx)
