cmake_minimum_required(VERSION 3.22)

project(Auralis VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(JUCE)

juce_add_plugin(Auralis
    COMPANY_NAME "Auralis"
    BUNDLE_ID "com.auralis.audio"
    PLUGIN_MANUFACTURER_CODE Aurl
    PLUGIN_CODE Aur1
    FORMATS VST3
    PRODUCT_NAME "Auralis"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(Auralis PRIVATE
    Source/plugin/PluginProcessor.cpp
)

target_compile_definitions(Auralis PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Auralis PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
