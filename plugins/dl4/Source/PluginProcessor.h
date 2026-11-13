#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Dl4Config.h"

namespace ParamIDs
{
inline constexpr const char* input       = "input";
inline constexpr const char* range       = "range";
inline constexpr const char* step        = "step";
inline constexpr const char* delayFactor = "delayFactor";
inline constexpr const char* vcoDepth    = "vcoDepth";
inline constexpr const char* vcoRate     = "vcoRate";
inline constexpr const char* vcoShape    = "vcoShape";
inline constexpr const char* feedback    = "feedback";
inline constexpr const char* repeat      = "repeat";
inline constexpr const char* mix         = "mix";
inline constexpr const char* memory      = "memory";
inline constexpr const char* stereoMode  = "stereoMode";
inline constexpr const char* spread      = "spread";
inline constexpr const char* character   = "character";
inline constexpr const char* pedalOn     = "pedalOn";
inline constexpr const char* pedal       = "pedal";
inline constexpr const char* output      = "output";
} // namespace ParamIDs

class HoldfastProcessor : public juce::AudioProcessor,
                          private juce::AudioProcessorValueTreeState::Listener,
                          private juce::AsyncUpdater
{
public:
    HoldfastProcessor();
    ~HoldfastProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Read by the editor for the displays.
    std::atomic<float> displayDelayMs { 0.0f };
    std::atomic<float> inputPeak { 0.0f };

    bool isMemoryExpanded() const { return engineExpanded.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    vdc::Params readParams() const;
    bool wantsExpandedMemory() const;

    // MEMORY changes reallocate the engines: done on the message thread with processing suspended.
    void parameterChanged (const juce::String& id, float value) override;
    void handleAsyncUpdate() override;
    void applyMemorySetting();

    // Two units: A alone in mono, A on the left and B on the right when linked as a stereo pair.
    vdc::AdmDelayEngine unitA { holdfast::makeConfig (false) };
    vdc::AdmDelayEngine unitB { holdfast::makeConfig (false) };
    std::atomic<bool> engineExpanded { false };
    double currentSampleRate = 48000.0;
    juce::AudioBuffer<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HoldfastProcessor)
};
