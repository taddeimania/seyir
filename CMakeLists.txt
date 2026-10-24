cmake_minimum_required(VERSION 3.22)

# macOS 11+. Builds for the machine you're on by default; for a universal
# binary pass -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64". Set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(Seyir VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE ---------------------------------------------------------------------
# By default JUCE is downloaded on first configure. To use a local checkout:
#   cmake -B build -DSEYIR_JUCE_DIR=/path/to/JUCE
set(SEYIR_JUCE_DIR "" CACHE PATH "Optional local JUCE checkout")

if(SEYIR_JUCE_DIR)
    add_subdirectory(${SEYIR_JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# --- Sound and engine (no JUCE) ------------------------------------------------
# Mutable Instruments' Plaits, Rings and stmlib (MIT licence, vendored in
# ThirdParty/mutable). The module code needs TEST defined to build for a
# desktop instead of the STM32. Seyir's synth wraps it and is built alongside;
# nothing outside this library sees the Mutable headers.
file(GLOB_RECURSE SEYIR_MUTABLE_SOURCES CONFIGURE_DEPENDS ThirdParty/mutable/*.cc)
add_library(SeyirDSP STATIC
    ${SEYIR_MUTABLE_SOURCES}
    Source/Synth/Synth.cpp
    Source/Synth/RingsBox.cpp
    Source/Engine/Tuning.cpp
    Source/Engine/Walker.cpp)
target_include_directories(SeyirDSP PUBLIC Source PRIVATE ThirdParty/mutable)
target_compile_definitions(SeyirDSP PRIVATE TEST)
set_target_properties(SeyirDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(SeyirDSP PRIVATE /W0 /wd4244 /wd4305)
else()
    target_compile_options(SeyirDSP PRIVATE -w)
endif()

# --- Plugin -------------------------------------------------------------------
# Copy the built plugin into the system VST3 folder after each build:
#   macOS   ~/Library/Audio/Plug-Ins/VST3
#   Linux   ~/.vst3
#   Windows C:\Program Files\Common Files\VST3  (needs an administrator shell,
#           so it is off by default there)
if(WIN32)
    set(_seyir_copy_default OFF)
else()
    set(_seyir_copy_default ON)
endif()
option(SEYIR_COPY_PLUGIN "Install the plugin into the system VST3 folder after building" ${_seyir_copy_default})

juce_add_plugin(Seyir
    COMPANY_NAME                "Taddei"
    BUNDLE_ID                   com.taddei.seyir
    PLUGIN_MANUFACTURER_CODE    Tadd
    PLUGIN_CODE                 Syir
    PRODUCT_NAME                "Seyir"
    DESCRIPTION                 "Microtonal melody explorer"
    FORMATS                     VST3
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Synth
    COPY_PLUGIN_AFTER_BUILD     ${SEYIR_COPY_PLUGIN})

set(SEYIR_PLUGIN_SOURCES
    Source/Settings.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/Widgets.cpp
    Source/UI/Views.cpp)

target_sources(Seyir PRIVATE ${SEYIR_PLUGIN_SOURCES})
target_include_directories(Seyir PRIVATE Source)
target_compile_definitions(Seyir PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Seyir
    PRIVATE
        SeyirDSP
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Engine tests (no JUCE needed) --------------------------------------------
enable_testing()
add_executable(SeyirEngineTests Tests/EngineTests.cpp)
target_link_libraries(SeyirEngineTests PRIVATE SeyirDSP)
add_test(NAME engine COMMAND SeyirEngineTests)

# --- Optional dev tool --------------------------------------------------------
#   SeyirSnapshot out.png [out.wav]   render the panel (and a minute of sound) headless
option(SEYIR_BUILD_TOOLS "Build the snapshot tool" OFF)
if(SEYIR_BUILD_TOOLS)
    juce_add_console_app(SeyirSnapshot PRODUCT_NAME "SeyirSnapshot")
    target_sources(SeyirSnapshot PRIVATE Tools/Snapshot.cpp ${SEYIR_PLUGIN_SOURCES})
    target_include_directories(SeyirSnapshot PRIVATE Source)
    target_compile_definitions(SeyirSnapshot PRIVATE
        JucePlugin_Name="Seyir"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(SeyirSnapshot PRIVATE
        SeyirDSP
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
endif()
