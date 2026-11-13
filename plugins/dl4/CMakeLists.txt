juce_add_plugin(Holdfast
    COMPANY_NAME "Slopebox Audio"
    BUNDLE_ID "audio.slopebox.holdfast512"
    PLUGIN_MANUFACTURER_CODE Slpb
    PLUGIN_CODE Hf51
    PRODUCT_NAME "Holdfast 512"
    FORMATS ${VDC_PLUGIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Delay Modulation
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Holdfast PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Holdfast PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Holdfast
    PRIVATE
        vdc::core
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline checks (delay steps, repeat, VCO, stability) and demo renders; no JUCE needed.
add_executable(holdfast_test tools/dl4_test.cpp)
target_link_libraries(holdfast_test PRIVATE vdc::core)
target_include_directories(holdfast_test PRIVATE Source)

add_test(NAME holdfast_checks COMMAND holdfast_test)
