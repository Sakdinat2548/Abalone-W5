cmake_minimum_required(VERSION 3.22)

project(AbaloneW5 VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Export compile commands for clangd/clang-tidy.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static MSVC runtime (/MT Release, /MTd Debug) so Sonar loads with no extra DLLs.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Ninja + MSVC: CMake routes RC dependency scanning through its cmcldeps
# helper, but the MSYS2-packaged cmake.exe used here does not ship
# cmcldeps.exe, so the tool prefix expands to empty and the RC rule degrades
# to a bare `RC <args>` call that dies with RC1107. Disabling the cmcldeps
# path for Ninja yields a plain `rc.exe ... /fo <out> <in>` rule (verified:
# RC1107 without this, clean compile with it). VS generator unaffected
# (guarded on Ninja); the generated .rc changes rarely so the lost dep-scan
# precision is irrelevant.
if(CMAKE_GENERATOR MATCHES "^Ninja")
    set(CMAKE_NINJA_CMCLDEPS_RC OFF)
endif()

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
        target_compile_options(${name} PRIVATE $<$<NOT:$<CONFIG:Debug>>:/UNDEBUG>)
    else ()
        target_compile_options(${name} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-UNDEBUG>)
    endif ()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_dsp_test(ToneTargetsTest tests/ToneTargetsTest.cpp)

add_dsp_test(ToneBankTest tests/ToneBankTest.cpp)
target_compile_definitions(ToneBankTest PRIVATE
    TONE_CSV_PATH="${CMAKE_CURRENT_SOURCE_DIR}/analysis/u5_tone_curves_from_claude.csv")

add_dsp_test(GainStageTest tests/GainStageTest.cpp)

add_dsp_test(HighCutTest tests/HighCutTest.cpp)

add_dsp_test(ColorStageTest tests/ColorStageTest.cpp)

add_dsp_test(ChainTest tests/ChainTest.cpp)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE
    GIT_TAG 8.0.15
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AbaloneW5
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Sakdinat2548"
    PLUGIN_MANUFACTURER_CODE SA48
    PLUGIN_CODE AbW5
    FORMATS VST3 Standalone
    PRODUCT_NAME "Abalone-W5"
    PLUGIN_NAME "Abalone W5"
)

target_sources(AbaloneW5 PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
)

# UI skins + layout data, embedded as BinaryData (decoded once on the message
# thread in the editor constructor, never on the audio thread).
# - V110Bakedbackground.png: v110 faceplate (ui/v110ui/, 1969x799) drawn 1:1;
#   knob bodies, captions, and button/LED slots all baked in. The dial rings
#   print ticks/numerals on an exact 30-degree clock grid, so boost/tone
#   needles use exact per-detent tables (see PluginEditor.cpp) instead of a
#   linear sweep.
# - V110big/small_pointer.png: straight-up 12 o'clock needles (18x61 /
#   6x20), drawn at natural size with the tip at the face rim, rotated about
#   the measured axle (see PluginEditor.cpp). Never stretched.
# - V110button_on/off.png (103x48), V110led_off.png (48px core),
#   V110led_on.png (79px: 48px core + spill): slot art drawn 1:1 at
#   float-exact slot rects (see FloatArtButton/FloatArtLed in PluginEditor.h).
# - v110ui/positions.csv: all live-element rects as texture ratios
#   (positions are data, not magic numbers; the editor parses this at
#   construction). OS/TRIM are small baked knobs flanking the UUV oval, with
#   in-code readouts below them.
# - abalone.svg: user-supplied ABALONE masthead artwork (exact filename,
#   single-e spelling), drawn centered in the header slot via a BinaryData
#   SVG drawable; static black in every OS state. (A previous CinzelBlack
#   fallback face + OFL.txt were removed: the embedded SVG cannot
#   realistically fail to parse, so nothing falls back to it.)
# Superseded (kept on disk, no longer embedded): ui/u5_front_clean.png,
# ui/knob_boost_no_pointer.png, ui/knob_tone_no_pointer.png,
# ui/pointer_boost.png, ui/pointer_tone.png, ui/pointer_trim.png,
# ui/button_on.png, ui/button_off.png, ui/led_on.png, ui/led_off.png,
# ui/component_positions.csv.
juce_add_binary_data(AbaloneW5Assets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        ui/v110ui/V110Bakedbackground.png
        ui/v110ui/V110big_pointer.png
        ui/v110ui/V110small_pointer.png
        ui/v110ui/V110button_on.png
        ui/v110ui/V110button_off.png
        ui/v110ui/V110led_on.png
        ui/v110ui/V110led_off.png
        ui/v110ui/positions.csv
        ui/abalone.svg
)

target_compile_definitions(AbaloneW5
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(AbaloneW5
    PRIVATE
        AbaloneW5Assets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Task 11 headless channel-layout test: drives the REAL processor. Linking the
# juce:: modules compiles their sources into this target (JUCE INTERFACE
# sources), so src/PluginProcessor.cpp + src/PluginEditor.cpp build here too;
# only JucePlugin_Name/VersionString are needed (the sole JucePlugin_* macros used in src/).
# Deliberately NOT via add_dsp_test(): that helper undefines NDEBUG, which
# would enable JUCE_DEBUG (leak-checker statics, extra debug paths) inside a
# headless pass/fail binary. The checks in ProcessorIOTest.cpp are explicit
# and NDEBUG-independent, so they stay live under ctest either way.
add_executable(ProcessorIOTest tests/ProcessorIOTest.cpp src/PluginProcessor.cpp src/PluginEditor.cpp)
target_include_directories(ProcessorIOTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(ProcessorIOTest PRIVATE JucePlugin_Name="Abalone W5"
    JucePlugin_VersionString="${PROJECT_VERSION}"
    # Same as the plugin target below: no web browser in the test binary, so
    # juce_gui_extra compiles without its gtk/webkit Linux system deps.
    JUCE_WEB_BROWSER=0
    # Same as the plugin target: no curl networking in the test binary, so
    # juce_core links without libcurl on Linux (JUCE loads WinHTTP on
    # Windows, which is why this only ever bites the Ubuntu leg).
    JUCE_USE_CURL=0)
target_link_libraries(ProcessorIOTest PRIVATE
    AbaloneW5Assets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
)
add_test(NAME ProcessorIOTest COMMAND ProcessorIOTest)

# Task 11 ASIO hookup (Standalone device list; the VST3 never touches ASIO,
# but the flag lives on the shared-code target because juce_audio_devices
# compiles there). Empty default = build exactly as today. When pointed at a
# Steinberg ASIO SDK root, the build uses the REAL SDK headers
# (JUCE_ASIO_USE_EXTERNAL_SDK=1 makes juce_audio_devices.cpp #include
# <iasiodrv.h> from the SDK instead of JUCE's bundled copy) and fails LOUDLY
# if the SDK layout is wrong. Never commit the SDK.
set(ABALONEW5_ASIO_SDK_DIR "" CACHE PATH "Steinberg ASIO SDK root (dir containing common/iasiodrv.h); empty builds without ASIO")
if (ABALONEW5_ASIO_SDK_DIR)
    if (NOT EXISTS "${ABALONEW5_ASIO_SDK_DIR}/common/iasiodrv.h")
        message(FATAL_ERROR "ABALONEW5_ASIO_SDK_DIR='${ABALONEW5_ASIO_SDK_DIR}' has no common/iasiodrv.h — point it at the Steinberg ASIO SDK root (Steinberg developer downloads, accept the license; never commit the SDK).")
    endif ()
    target_include_directories(AbaloneW5 PRIVATE "${ABALONEW5_ASIO_SDK_DIR}/common")
    target_compile_definitions(AbaloneW5 PRIVATE JUCE_ASIO=1 JUCE_ASIO_USE_EXTERNAL_SDK=1)
endif ()
