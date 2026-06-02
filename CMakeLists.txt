cmake_minimum_required(VERSION 3.24)

project(HemerodromosDrone VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")

add_subdirectory(external/JUCE)

juce_add_binary_data(HemerodromosDroneAssets
    SOURCES
        assets/fitted/drone_base1_fit_v001.dronebank.json
        assets/fitted/drone_base2_fit_v001.dronebank.json
        assets/fitted/drone_base3_fit_v001.dronebank.json
        assets/fitted/drone_base4_fit_v001.dronebank.json
        assets/fitted/drone_base5_fit_v001.dronebank.json)

juce_add_plugin(HemerodromosDrone
    COMPANY_NAME "Gijs Brouwer"
    PRODUCT_NAME "Hemerodromos Drone"
    BUNDLE_ID "com.gijsbrouwer.hemerodromosdrone"
    VERSION "0.1.0"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3 Standalone
    VST3_CATEGORIES "Instrument" "Synth")

juce_generate_juce_header(HemerodromosDrone)

target_sources(HemerodromosDrone PRIVATE
    plugin/Source/PluginProcessor.cpp
    plugin/Source/PluginEditor.cpp
    plugin/Source/DroneBank.cpp
    plugin/Source/SynthEngine.cpp
    plugin/Source/PostEffects.cpp
    plugin/Source/UI/Knob.cpp
    plugin/Source/UI/DroneLookAndFeel.cpp)

target_include_directories(HemerodromosDrone PRIVATE
    plugin/Source)

target_link_libraries(HemerodromosDrone PRIVATE
    HemerodromosDroneAssets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra)

target_compile_definitions(HemerodromosDrone PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
