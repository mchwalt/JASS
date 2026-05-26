cmake_minimum_required(VERSION 3.22)
project(Synthy VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(Synthy
    COMPANY_NAME "SynthyProject"
    PLUGIN_MANUFACTURER_CODE Synt
    PLUGIN_CODE Syn1
    FORMATS Standalone VST3
    PRODUCT_NAME "Synthy"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
)

target_sources(Synthy PRIVATE
    # Plugin core
    Source/PluginProcessor.cpp

    # UI
    Source/UI/PluginEditor.cpp

    # Audio engine
    Source/Audio/SynthVoice.cpp
    Source/Audio/SynthSound.cpp

    # DSP modules
    Source/DSP/Oscillator.cpp
    Source/DSP/AdsrEnvelope.cpp
    Source/DSP/BiquadFilter.cpp
    Source/DSP/Effects.cpp
)

juce_generate_juce_header(Synthy)

target_compile_definitions(Synthy PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    _USE_MATH_DEFINES
)

target_link_libraries(Synthy
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
