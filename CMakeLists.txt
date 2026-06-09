cmake_minimum_required(VERSION 3.24)

project(HemerodromosDrone VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")

add_subdirectory(external/JUCE)

file(GLOB HEMERODROMOS_BANK_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fitted/*_fit_v*.dronebank.json")
list(SORT HEMERODROMOS_BANK_ASSETS)

set(HEMERODROMOS_UI_ASSETS
    "${CMAKE_CURRENT_SOURCE_DIR}/plugin/Assets/fonts/Audiowide-Regular.ttf")

juce_add_binary_data(HemerodromosDroneAssets
    SOURCES
        ${HEMERODROMOS_BANK_ASSETS}
        ${HEMERODROMOS_UI_ASSETS})

set(HEMERODROMOS_PLUGIN_SOURCES
    plugin/Source/PluginProcessor.cpp
    plugin/Source/PluginEditor.cpp
    plugin/Source/DroneBank.cpp
    plugin/Source/SynthEngine.cpp
    plugin/Source/PostEffects.cpp
    plugin/Source/UI/Knob.cpp
    plugin/Source/UI/DroneLookAndFeel.cpp)

function(add_hemerodromos_drone_plugin target product_name bundle_id plugin_code)
    juce_add_plugin(${target}
        COMPANY_NAME "Gijs Brouwer"
        PRODUCT_NAME "${product_name}"
        PLUGIN_NAME "${product_name}"
        BUNDLE_ID "${bundle_id}"
        PLUGIN_MANUFACTURER_CODE "Manu"
        PLUGIN_CODE "${plugin_code}"
        VERSION "0.1.0"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        FORMATS VST3 Standalone
        VST3_CATEGORIES "Instrument" "Synth")

    juce_generate_juce_header(${target})

    target_sources(${target} PRIVATE
        ${HEMERODROMOS_PLUGIN_SOURCES})

    target_include_directories(${target} PRIVATE
        plugin/Source)

    target_link_libraries(${target} PRIVATE
        HemerodromosDroneAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra)

    target_compile_definitions(${target} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
endfunction()

add_hemerodromos_drone_plugin(
    HemerodromosDrone
    "Hemerodromos Drone"
    "com.gijsbrouwer.hemerodromosdrone"
    "Fldb")
