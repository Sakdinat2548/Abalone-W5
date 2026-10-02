cmake_minimum_required(VERSION 3.22)

project(AbaloneW5 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile commands for clangd/clang-tidy.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static MSVC runtime (/MT Release, /MTd Debug) so Sonar loads with no extra DLLs.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

enable_testing()

add_executable(ToneTargetsTest tests/ToneTargetsTest.cpp)
target_include_directories(ToneTargetsTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME ToneTargetsTest COMMAND ToneTargetsTest)

add_executable(GainStageTest tests/GainStageTest.cpp)
target_include_directories(GainStageTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME GainStageTest COMMAND GainStageTest)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE
    GIT_TAG 8.0.15
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AbaloneW5
    COMPANY_NAME "Abalone"
    PLUGIN_MANUFACTURER_CODE Abal
    PLUGIN_CODE Au55
    FORMATS VST3
    PRODUCT_NAME "Abalone W5"
)

target_sources(AbaloneW5 PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
)

target_compile_definitions(AbaloneW5
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AbaloneW5
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
