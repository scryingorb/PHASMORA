cmake_minimum_required(VERSION 3.22)
project(GHOST VERSION 1.0.0 LANGUAGES C CXX)

# Self-contained Windows runtime: no separately installed VC runtime required.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
include(FetchContent)
set(GHOST_JUCE_PATH "" CACHE PATH "Optional path to an existing JUCE 8 source checkout")
if(GHOST_JUCE_PATH)
    add_subdirectory("${GHOST_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(GHOST
    COMPANY_NAME PHASMORA
    BUNDLE_ID com.ghost.audio.ghost
    PLUGIN_MANUFACTURER_CODE Ghst
    PLUGIN_CODE Gh01
    PRODUCT_NAME PHASMORA
    DESCRIPTION "PHASMORA supernatural audio-memory effect"
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx)

# JUCE's generated Windows resource must follow metadata changes on incremental builds.
if(WIN32)
    add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/GHOST_artefacts/JuceLibraryCode/GHOST_resources.rc"
        APPEND DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/GHOST_artefacts/JuceLibraryCode/Info.txt")
endif()

add_custom_target(PHASMORA_VST3 DEPENDS GHOST_VST3)

juce_add_binary_data(GHOST_Assets HEADER_NAME GhostAssets.h NAMESPACE GhostAssets
    SOURCES Assets/PhasmoraLogo.png)

target_sources(GHOST PRIVATE
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp Source/PluginEditor.h
    Source/Presets/GhostPresetManager.cpp Source/Presets/GhostPresetManager.h
    Source/DSP/GhostEngine.cpp Source/DSP/GhostEngine.h
    Source/DSP/GhostBuffer.cpp Source/DSP/GhostBuffer.h
    Source/DSP/GhostVoice.cpp Source/DSP/GhostVoice.h
    Source/DSP/GhostFeedback.cpp Source/DSP/GhostFeedback.h
    Source/DSP/GhostBlur.cpp Source/DSP/GhostBlur.h
    Source/DSP/GhostMacro.cpp Source/DSP/GhostMacro.h
    Source/DSP/GhostFreeze.cpp Source/DSP/GhostFreeze.h
    Source/DSP/GhostReverse.cpp Source/DSP/GhostReverse.h
    Source/UI/GhostLookAndFeel.cpp Source/UI/GhostLookAndFeel.h
    Source/UI/GhostKnob.cpp Source/UI/GhostKnob.h
    Source/UI/GhostVisualizer.cpp Source/UI/GhostVisualizer.h)
target_compile_features(GHOST PUBLIC cxx_std_17)
target_compile_definitions(GHOST PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(GHOST PRIVATE GHOST_Assets juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

option(GHOST_BUILD_TESTS "Build the DSP regression tests" OFF)
if(GHOST_BUILD_TESTS)
    enable_testing()
    add_executable(GHOST_DSPTests Tests/GhostDSPTests.cpp Tests/GhostFreezeTests.cpp Tests/GhostMacroTests.cpp
        Source/DSP/GhostBuffer.cpp Source/DSP/GhostEngine.cpp Source/DSP/GhostVoice.cpp
        Source/DSP/GhostFeedback.cpp Source/DSP/GhostBlur.cpp Source/DSP/GhostReverse.cpp Source/DSP/GhostFreeze.cpp Source/DSP/GhostMacro.cpp)
    target_include_directories(GHOST_DSPTests PRIVATE Source)
    target_compile_features(GHOST_DSPTests PRIVATE cxx_std_17)
    target_compile_definitions(GHOST_DSPTests PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(GHOST_DSPTests PRIVATE juce::juce_audio_basics
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME GHOST_DSPTests COMMAND GHOST_DSPTests)
    add_executable(GHOST_ProcessorTests Tests/GhostProcessorTests.cpp)
    target_include_directories(GHOST_ProcessorTests PRIVATE Source)
    target_link_libraries(GHOST_ProcessorTests PRIVATE GHOST juce::juce_audio_utils)
    add_test(NAME GHOST_ProcessorTests COMMAND GHOST_ProcessorTests)
    add_executable(GHOST_UITests Tests/GhostUITests.cpp Tests/GhostVisualizerTests.cpp Tests/GhostPresetTests.cpp)
    target_include_directories(GHOST_UITests PRIVATE Source)
    target_link_libraries(GHOST_UITests PRIVATE GHOST juce::juce_audio_utils)
    add_test(NAME GHOST_UITests COMMAND GHOST_UITests)
    add_executable(GHOST_ReleaseTests Tests/GhostReleaseTests.cpp)
    target_include_directories(GHOST_ReleaseTests PRIVATE Source)
    target_link_libraries(GHOST_ReleaseTests PRIVATE GHOST juce::juce_audio_utils psapi)
    add_test(NAME GHOST_ReleaseTests COMMAND GHOST_ReleaseTests)
    add_executable(GHOST_BrandingTests Tests/GhostBrandingTests.cpp)
    target_include_directories(GHOST_BrandingTests PRIVATE Source)
    target_link_libraries(GHOST_BrandingTests PRIVATE GHOST juce::juce_audio_utils juce::juce_cryptography)
    target_compile_definitions(GHOST_BrandingTests PRIVATE GHOST_BRANDING_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures")
    add_test(NAME GHOST_BrandingTests COMMAND GHOST_BrandingTests)
endif()
