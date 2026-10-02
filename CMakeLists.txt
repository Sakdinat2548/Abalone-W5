cmake_minimum_required(VERSION 3.22)

project(AbaloneW5 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile commands for clangd/clang-tidy.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static MSVC runtime (/MT Release, /MTd Debug) so Sonar loads with no extra DLLs.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

enable_testing()

# Test-integrity: MSVC Release defines NDEBUG, which compiles bare assert()
# out of existence and makes ctest PASS evidence vacuous. Every DSP test must
# be created via this helper so its asserts stay live under ctest -C Release.
function(add_dsp_test name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    # /U (MSVC) / -U (GNU/Clang) undefines the flag-injected NDEBUG for this
    # target only; it sorts after CMAKE_CXX_FLAGS_RELEASE on the command line.
    if (MSVC)
        target_compile_options(${name} PRIVATE $<$<CONFIG:Release>:/UNDEBUG>)
    else ()
        target_compile_options(${name} PRIVATE $<$<CONFIG:Release>:-UNDEBUG>)
    endif ()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_dsp_test(ToneTargetsTest tests/ToneTargetsTest.cpp)

add_dsp_test(ToneBankTest tests/ToneBankTest.cpp)
target_compile_definitions(ToneBankTest PRIVATE
    TONE_CSV_PATH="${CMAKE_CURRENT_SOURCE_DIR}/analysis/u5_tone_curves_digitized.csv")

add_dsp_test(GainStageTest tests/GainStageTest.cpp)

add_dsp_test(HighCutTest tests/HighCutTest.cpp)

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
