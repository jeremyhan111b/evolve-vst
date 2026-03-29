cmake_minimum_required(VERSION 3.22)
project(EVOLVE VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(EVOLVE
    COMPANY_NAME                "EVOLVE"
    PLUGIN_MANUFACTURER_CODE    EVLV
    PLUGIN_CODE                 Evlv
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "EVOLVE"
    BUNDLE_ID                   "com.evolve.plugin"
    PLUGIN_DESCRIPTION          "AI Melody Evolution"
    IS_MIDI_EFFECT              FALSE
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES             "Fx" "Tools"
    AU_MAIN_TYPE                "kAudioUnitType_MusicEffect"
    COPY_PLUGIN_AFTER_BUILD     FALSE
    HARDENED_RUNTIME_ENABLED    TRUE
    APP_SANDBOX_ENABLED         FALSE
    HARDENED_RUNTIME_OPTIONS    "com.apple.security.network.client,com.apple.security.cs.disable-library-validation"
)

target_sources(EVOLVE PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PianoRoll.cpp
    Source/GenCard.cpp
    Source/EvolveEngine.cpp
)

target_link_libraries(EVOLVE PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_core
    juce::juce_data_structures
)

target_compile_definitions(EVOLVE PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
)

juce_generate_juce_header(EVOLVE)
