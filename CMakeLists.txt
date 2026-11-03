cmake_minimum_required(VERSION 3.22)

project(Auralis VERSION 0.2.0 LANGUAGES C CXX)

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
    Source/plugin/PluginEditor.cpp
    Source/plugin/ParameterLayout.cpp
    Source/presets/PresetManager.cpp
    Source/ui/SpectrumView.cpp
)

target_compile_definitions(Auralis PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# --- Auralis internal libraries ---
add_library(auralis_audio_core INTERFACE)
target_include_directories(auralis_audio_core INTERFACE modules/audio_core)

add_library(auralis_dsp STATIC)
target_sources(auralis_dsp PRIVATE
    modules/dsp/gain/GainProcessor.cpp
    modules/dsp/equalizer/EqualizerProcessor.cpp
    modules/dsp/bass/BassEnhancerProcessor.cpp
    modules/dsp/clarity/ClarityProcessor.cpp
    modules/dsp/dynamics/CompressorProcessor.cpp
    modules/dsp/ambience/AmbienceProcessor.cpp
    modules/dsp/stereo/StereoEnhancerProcessor.cpp
    modules/dsp/limiter/LimiterProcessor.cpp
)
target_include_directories(auralis_dsp PUBLIC modules/dsp)
target_link_libraries(auralis_dsp PUBLIC auralis_audio_core)

add_library(auralis_analysis STATIC)
target_sources(auralis_analysis PRIVATE
    modules/analysis/Fft.cpp
    modules/analysis/SpectrumAnalyzer.cpp
)
target_include_directories(auralis_analysis PUBLIC modules/analysis)
target_link_libraries(auralis_analysis PUBLIC auralis_audio_core)

add_library(auralis_dsp_pipeline STATIC)
target_sources(auralis_dsp_pipeline PRIVATE modules/dsp_pipeline/DspPipeline.cpp)
target_include_directories(auralis_dsp_pipeline PUBLIC modules/dsp_pipeline)
target_link_libraries(auralis_dsp_pipeline PUBLIC auralis_dsp auralis_analysis)

enable_testing()
add_executable(auralis_tests
    tests/unit/TestMain.cpp
    tests/unit/TestEqualizer.cpp
    tests/unit/TestBass.cpp
    tests/unit/TestClarity.cpp
    tests/unit/TestDynamics.cpp
    tests/unit/TestAmbience.cpp
    tests/unit/TestStereo.cpp
    tests/unit/TestLimiter.cpp
    tests/unit/TestFft.cpp
    tests/unit/TestAnalyzer.cpp
)
target_link_libraries(auralis_tests PRIVATE auralis_dsp_pipeline auralis_analysis)
add_test(NAME auralis_unit COMMAND auralis_tests)

target_link_libraries(Auralis PRIVATE
    auralis_dsp_pipeline
    auralis_analysis
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)