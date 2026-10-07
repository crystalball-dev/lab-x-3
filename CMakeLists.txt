cmake_minimum_required(VERSION 3.22)

project(LABX3 VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime: the plugin then needs no VC++ redistributable on other machines.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(LABX3_JUCE_PATH "F:/_DEV/tools/JUCE" CACHE PATH "Path to a JUCE 9 checkout")
if(NOT EXISTS "${LABX3_JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${LABX3_JUCE_PATH}. Configure with -DLABX3_JUCE_PATH=<path to JUCE 9>.")
endif()
add_subdirectory("${LABX3_JUCE_PATH}" JUCE)

juce_add_plugin(LabX3
    PRODUCT_NAME "LAB X-3"
    VERSION 0.2.0
    COMPANY_NAME "OPERATION FAIRWAY, LLC"
    COMPANY_WEBSITE "https://github.com/crystalball-dev/lab-x-3"
    BUNDLE_ID com.operationfairway.labx3
    PLUGIN_MANUFACTURER_CODE Opfw
    PLUGIN_CODE Lbx3
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

set(LABX3_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SpecimenLibrary.cpp
    Source/FactoryPresets.cpp)

set(LABX3_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_sources(LabX3 PRIVATE ${LABX3_SOURCES})
target_include_directories(LabX3 PRIVATE Source)
target_compile_definitions(LabX3 PUBLIC ${LABX3_DEFINITIONS})
target_compile_options(LabX3 PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_link_libraries(LabX3
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline render and test harness: same sources, driven from the command line.
juce_add_console_app(LabX3Render PRODUCT_NAME "LabX3Render")
target_sources(LabX3Render PRIVATE Tests/RenderHarness.cpp ${LABX3_SOURCES})
target_include_directories(LabX3Render PRIVATE Source)
target_compile_definitions(LabX3Render PRIVATE ${LABX3_DEFINITIONS} LABX3_HARNESS=1)
target_compile_options(LabX3Render PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
target_link_libraries(LabX3Render
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
