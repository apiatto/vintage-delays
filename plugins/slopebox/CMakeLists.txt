juce_add_plugin(Slopebox
    COMPANY_NAME "Slopebox Audio"
    BUNDLE_ID "audio.slopebox.slopebox1024"
    PLUGIN_MANUFACTURER_CODE Slpb
    PLUGIN_CODE Sb24
    PRODUCT_NAME "Slopebox 1024"
    FORMATS ${VDC_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Delay Modulation
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Slopebox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Slopebox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Slopebox
    PRIVATE
        vdc::core
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline render tool and regression test (no JUCE needed).
add_executable(slopebox_render tools/render.cpp)
target_link_libraries(slopebox_render PRIVATE vdc::core)
target_include_directories(slopebox_render PRIVATE Source)

add_test(NAME slopebox_regression
    COMMAND slopebox_render --check ${CMAKE_CURRENT_SOURCE_DIR}/tests/reference_levels.txt)

# Panel snapshot for the manual (needs a display; on Linux run it under xvfb-run).
option(VDC_BUILD_DOC_TOOLS "Build the documentation helpers" OFF)
if(VDC_BUILD_DOC_TOOLS)
    juce_add_console_app(slopebox_snapshot PRODUCT_NAME "slopebox_snapshot")
    target_sources(slopebox_snapshot PRIVATE
        tools/snapshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(slopebox_snapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JucePlugin_Name="Slopebox 1024")
    target_link_libraries(slopebox_snapshot PRIVATE
        vdc::core
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
