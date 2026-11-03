cmake_minimum_required(VERSION 3.22)

project(Instrument VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(FetchContent)

option(INSTRUMENT_BUILD_PLUGIN "Build the JUCE plugin targets" ON)
option(INSTRUMENT_BUILD_TESTS "Build processor unit tests" ${BUILD_TESTING})
option(INSTRUMENT_ENABLE_AAX "Build the AAX format (requires the Avid AAX SDK)" OFF)
set(JUCE_AAX_SDK_PATH "" CACHE PATH "Path to the Avid AAX SDK")

if(INSTRUMENT_ENABLE_AAX)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows" AND NOT APPLE)
        message(FATAL_ERROR "AAX is supported only on Windows and macOS.")
    endif()
    if(NOT JUCE_AAX_SDK_PATH OR NOT IS_DIRECTORY "${JUCE_AAX_SDK_PATH}")
        message(FATAL_ERROR
            "INSTRUMENT_ENABLE_AAX requires JUCE_AAX_SDK_PATH to point to an "
            "installed Avid AAX SDK.")
    endif()
    set(JUCE_AAX_SDK_PATH "${JUCE_AAX_SDK_PATH}" CACHE PATH
        "Path to the Avid AAX SDK" FORCE)
endif()

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(JUCE)

if(INSTRUMENT_BUILD_PLUGIN)
    set(INSTRUMENT_PLUGIN_FORMATS VST3)
    if(APPLE)
        list(APPEND INSTRUMENT_PLUGIN_FORMATS AU)
    endif()
    if(INSTRUMENT_ENABLE_AAX)
        list(APPEND INSTRUMENT_PLUGIN_FORMATS AAX)
    endif()

    juce_add_plugin(instrument
        COMPANY_NAME "Petebeat Software"
        BUNDLE_ID "com.petebeatsoftware.instrument"
        PLUGIN_MANUFACTURER_CODE PBTS
        PLUGIN_CODE IN01
        PRODUCT_NAME "Instrument"
        DESCRIPTION "Audio passthrough and MIDI transposition"
        VERSION ${PROJECT_VERSION}
        IS_SYNTH FALSE
        IS_MIDI_EFFECT TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        FORMATS ${INSTRUMENT_PLUGIN_FORMATS}
    )

    juce_generate_juce_header(instrument)
    target_sources(instrument PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/MidiTransposer.cpp
    )
    target_include_directories(instrument PRIVATE src)
    target_compile_features(instrument PUBLIC cxx_std_17)
    target_compile_definitions(instrument PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    target_link_libraries(instrument PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )

    include(cmake/Packaging.cmake)
endif()

if(INSTRUMENT_BUILD_TESTS)
    juce_add_console_app(instrument_tests
        PRODUCT_NAME "Instrument Tests"
    )
    juce_generate_juce_header(instrument_tests)
    target_sources(instrument_tests PRIVATE
        tests/MidiTransposerTests.cpp
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/MidiTransposer.cpp
    )
    target_include_directories(instrument_tests PRIVATE src)
    target_compile_features(instrument_tests PRIVATE cxx_std_17)
    target_compile_definitions(instrument_tests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    target_link_libraries(instrument_tests PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
    add_test(NAME instrument_midi_transposer COMMAND instrument_tests)
endif()

if(NOT INSTRUMENT_BUILD_PLUGIN AND NOT INSTRUMENT_BUILD_TESTS)
    message(FATAL_ERROR "Enable at least one of INSTRUMENT_BUILD_PLUGIN or INSTRUMENT_BUILD_TESTS.")
endif()
