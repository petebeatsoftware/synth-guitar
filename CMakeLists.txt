cmake_minimum_required(VERSION 3.22)

project(PeteBeatSynthGuitar VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(FetchContent)

if(MSVC AND WIN32 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR
        "This project only supports 64-bit (x64) MSVC builds, but a 32-bit "
        "compiler was selected (${CMAKE_CXX_COMPILER}).\n"
        "This usually happens when CMake is configured from a plain shell "
        "without the x64 Visual Studio environment loaded. Fix this by "
        "either:\n"
        "  * Opening this folder in Visual Studio and using the 'dev' or "
        "'ci' CMake preset (Visual Studio applies the x64 toolset "
        "automatically), or\n"
        "  * Running 'scripts/configure.ps1' from PowerShell, or\n"
        "  * Launching 'x64 Native Tools Command Prompt for VS' (or running "
        "vcvars64.bat) before invoking cmake, then re-running your cmake "
        "command.\n"
        "Delete the build directory and reconfigure after switching "
        "environments.")
endif()

if(MSVC AND WIN32)
    get_filename_component(_instrument_msvc_bin_dir "${CMAKE_LINKER}" DIRECTORY)
    get_filename_component(_instrument_msvc_host_dir "${_instrument_msvc_bin_dir}" DIRECTORY)
    get_filename_component(_instrument_msvc_bin_root "${_instrument_msvc_host_dir}" DIRECTORY)
    get_filename_component(_instrument_msvc_root "${_instrument_msvc_bin_root}" DIRECTORY)

    set(_instrument_msvc_x64_libdir "${_instrument_msvc_root}/lib/x64")

    set(_instrument_sdk_um_x64_libdir "")
    set(_instrument_sdk_ucrt_x64_libdir "")
    if(CMAKE_MT)
        get_filename_component(_instrument_sdk_x64_bin_dir "${CMAKE_MT}" DIRECTORY)
        get_filename_component(_instrument_sdk_version_bin_dir "${_instrument_sdk_x64_bin_dir}" DIRECTORY)
        get_filename_component(_instrument_sdk_bin_root "${_instrument_sdk_version_bin_dir}" DIRECTORY)
        get_filename_component(_instrument_windows_kits_root "${_instrument_sdk_bin_root}" DIRECTORY)
        get_filename_component(_instrument_windows_sdk_version "${_instrument_sdk_version_bin_dir}" NAME)

        set(_instrument_sdk_um_x64_libdir "${_instrument_windows_kits_root}/Lib/${_instrument_windows_sdk_version}/um/x64")
        set(_instrument_sdk_ucrt_x64_libdir "${_instrument_windows_kits_root}/Lib/${_instrument_windows_sdk_version}/ucrt/x64")
    endif()

    foreach(_instrument_libdir IN ITEMS
            "${_instrument_msvc_x64_libdir}"
            "${_instrument_sdk_um_x64_libdir}"
            "${_instrument_sdk_ucrt_x64_libdir}")
        if(_instrument_libdir AND IS_DIRECTORY "${_instrument_libdir}")
            add_link_options("/LIBPATH:${_instrument_libdir}")
        endif()
    endforeach()
endif()

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
        PRODUCT_NAME "Pete Beat Synth Guitar"
        Audio passthrough and transient-triggered MIDI
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
        "src/SynthGuitarAudioProcessor.cpp"
        "src/SynthGuitarUserInterface.cpp"
        src/FilmstripKnob.cpp
        src/FilmstripControl.cpp
        src/RenderedUserInterface.cpp
        src/MidiSplitter.cpp
        src/MidiTransposer.cpp
        src/TransientDetector.cpp
        src/TriggeredPlayer.cpp
    )
    target_include_directories(instrument PRIVATE src)
    target_compile_features(instrument PUBLIC cxx_std_17)
    target_compile_definitions(instrument PRIVATE
        SYNTHGUITAR_RENDERED_UI_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src/Rendered UI"
    )
    target_compile_definitions(instrument PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    target_link_libraries(instrument PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )

    include(cmake/Packaging.cmake)
endif()

if(INSTRUMENT_BUILD_TESTS)
    juce_add_console_app(instrument_tests
        PRODUCT_NAME "Pete Beat Synth Guitar Tests"
    )
    juce_generate_juce_header(instrument_tests)
    target_sources(instrument_tests PRIVATE
        tests/TestMain.cpp
        tests/MidiTransposerTests.cpp
        tests/MidiSplitterTests.cpp
        tests/TransientDetectorTests.cpp
        tests/TriggeredPlayerTests.cpp
        tests/SynthGuitarAudioProcessorTests.cpp
        tests/SynthGuitarUserInterfaceTests.cpp
        "src/SynthGuitarAudioProcessor.cpp"
        "src/SynthGuitarUserInterface.cpp"
        src/FilmstripKnob.cpp
        src/FilmstripControl.cpp
        src/RenderedUserInterface.cpp
        src/MidiSplitter.cpp
        src/MidiTransposer.cpp
        src/TransientDetector.cpp
        src/TriggeredPlayer.cpp
    )
    target_include_directories(instrument_tests PRIVATE src)
    target_compile_features(instrument_tests PRIVATE cxx_std_17)
    target_compile_definitions(instrument_tests PRIVATE
        SYNTHGUITAR_RENDERED_UI_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src/Rendered UI"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    target_link_libraries(instrument_tests PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
    add_test(NAME instrument_midi_transposer COMMAND instrument_tests MidiTransposer)
    add_test(NAME instrument_midi_splitter COMMAND instrument_tests MidiSplitter)
    add_test(NAME instrument_transient_detector COMMAND instrument_tests TransientDetector)
    add_test(NAME instrument_triggered_player COMMAND instrument_tests TriggeredPlayer)
    add_test(NAME instrument_audio_processor COMMAND instrument_tests SynthGuitarAudioProcessor)
    add_test(NAME instrument_user_interface COMMAND instrument_tests SynthGuitarUserInterface)
endif()

if(NOT INSTRUMENT_BUILD_PLUGIN AND NOT INSTRUMENT_BUILD_TESTS)
    message(FATAL_ERROR "Enable at least one of INSTRUMENT_BUILD_PLUGIN or INSTRUMENT_BUILD_TESTS.")
endif()
