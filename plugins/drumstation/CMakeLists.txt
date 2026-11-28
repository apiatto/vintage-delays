# Hardware editor for the Novation DrumStation rack (MIDI controllers; SysEx dumps in phase 2).
# Built as an instrument with a silent output so Logic can host it on a software instrument track;
# the plugin opens the MIDI ports itself, like the usual hardware editors.

set(DRUMSTATION_FORMATS AU VST3 Standalone)
if(NOT APPLE)
    set(DRUMSTATION_FORMATS VST3 Standalone)
endif()

juce_add_plugin(DrumstationEditor
    COMPANY_NAME "Slopebox Audio"
    BUNDLE_ID "audio.slopebox.drumstationeditor"
    PLUGIN_MANUFACTURER_CODE Slpb
    PLUGIN_CODE Dsed
    PRODUCT_NAME "Drumstation Editor"
    FORMATS ${DRUMSTATION_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Drum
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

set(DRUMSTATION_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_sources(DrumstationEditor PRIVATE ${DRUMSTATION_SOURCES})

set(DRUMSTATION_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_definitions(DrumstationEditor PUBLIC ${DRUMSTATION_DEFINITIONS})

target_link_libraries(DrumstationEditor
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# MIDI behaviour test: controllers out on parameter changes, no echo of the unit's own knobs,
# program change before controllers, every controller of the manual's map present.
juce_add_console_app(drumstation_midi_test PRODUCT_NAME "drumstation_midi_test")
target_sources(drumstation_midi_test PRIVATE tests/midi_test.cpp ${DRUMSTATION_SOURCES})
target_compile_definitions(drumstation_midi_test PRIVATE ${DRUMSTATION_DEFINITIONS}
    JucePlugin_Name="Drumstation Editor")
target_link_libraries(drumstation_midi_test PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags)
add_test(NAME drumstation_midi COMMAND drumstation_midi_test)

# Panel snapshot for the manual (needs a display; on Linux run it under xvfb-run).
if(VDC_BUILD_DOC_TOOLS)
    juce_add_console_app(drumstation_snapshot PRODUCT_NAME "drumstation_snapshot")
    target_sources(drumstation_snapshot PRIVATE tools/snapshot.cpp ${DRUMSTATION_SOURCES})
    target_compile_definitions(drumstation_snapshot PRIVATE ${DRUMSTATION_DEFINITIONS}
        JUCE_MODAL_LOOPS_PERMITTED=1 JucePlugin_Name="Drumstation Editor")
    target_link_libraries(drumstation_snapshot PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
endif()
