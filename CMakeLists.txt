cmake_minimum_required(VERSION 3.22)

project(AbaloneU55 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile commands for clangd/clang-tidy.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static MSVC runtime (/MT Release, /MTd Debug) so Sonar loads with no extra DLLs.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE
    GIT_TAG 8.0.15
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AbaloneU55
    COMPANY_NAME "Abalone"
    PLUGIN_MANUFACTURER_CODE Abal
    PLUGIN_CODE Au55
    FORMATS VST3
    PRODUCT_NAME "Abalone-U55"
)

target_sources(AbaloneU55 PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
)

target_compile_definitions(AbaloneU55
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AbaloneU55
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
