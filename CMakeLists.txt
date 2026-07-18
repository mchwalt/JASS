cmake_minimum_required(VERSION 3.22)
project(JASS VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(JASS
    COMPANY_NAME "JASSProject"
    PLUGIN_MANUFACTURER_CODE Synt
    PLUGIN_CODE Syn1
    FORMATS Standalone VST3
    PRODUCT_NAME "JASS"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
)

target_sources(JASS PRIVATE
    # Plugin core
    Source/PluginProcessor.cpp

    # UI
    Source/UI/PluginEditor.cpp

    # UI rack framework
    Source/UI/rack/ModuleFrame.cpp
    Source/UI/rack/Rack.cpp
    Source/UI/rack/SynthyLookAndFeel.cpp

    # Audio engine
    Source/Audio/SynthVoice.cpp
    Source/Audio/SynthSound.cpp

    # DSP modules
    Source/DSP/Oscillator.cpp
    Source/DSP/AdsrEnvelope.cpp
    Source/DSP/BiquadFilter.cpp
    Source/DSP/Effects.cpp
)

juce_generate_juce_header(JASS)

# Embedded resources (Story 6.1): per-module online-help texts, now one Markdown file per
# module inside a per-language folder (Resources/<LANG>/<moduleId>.md), compiled into the
# binary so they are available in both Standalone and VST3 without any external file
# dependency. Markdown (instead of the old flat JSON) lets the texts carry formatting.
#
# One binary-data target PER LANGUAGE, each with its own NAMESPACE and HEADER_NAME. This is
# deliberate: juce_add_binary_data derives its C++ symbol from the file BASENAME only (path is
# dropped), so EN/osc1.md and DE/osc1.md would collide in a single target. Separate namespaces
# keep them apart, and distinct HEADER_NAMEs avoid two "BinaryData.h" headers clashing.
#
# CONFIGURE_DEPENDS re-globs at build time, so dropping in a new module .md is picked up on the
# next build. Adding a LANGUAGE = new Resources/<LANG> folder + a target below + one line in
# HelpTextStore (registerLanguage).
file(GLOB JASS_HELP_EN CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/EN/*.md")
file(GLOB JASS_HELP_DE CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/DE/*.md")
juce_add_binary_data(JASS_HelpEN HEADER_NAME HelpEN.h NAMESPACE HelpEN SOURCES ${JASS_HELP_EN})
juce_add_binary_data(JASS_HelpDE HEADER_NAME HelpDE.h NAMESPACE HelpDE SOURCES ${JASS_HELP_DE})

# Demo presets (shipped): embedded so the app can SEED them into the user's Presets folder on
# first run (see PresetIO::seedDemoPresets). Same one-target-with-own-namespace pattern as help.
file(GLOB JASS_DEMO_PRESETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/DemoPresets/*.synthy")
juce_add_binary_data(JASS_DemoPresets HEADER_NAME DemoPresets.h NAMESPACE DemoPresets SOURCES ${JASS_DEMO_PRESETS})

target_compile_definitions(JASS PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    _USE_MATH_DEFINES
)

target_link_libraries(JASS
    PRIVATE
        JASS_HelpEN
        JASS_HelpDE
        JASS_DemoPresets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
