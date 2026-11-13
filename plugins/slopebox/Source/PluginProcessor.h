#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "SlopeboxConfig.h"

namespace ParamIDs
{
inline constexpr const char* input       = "input";
inline constexpr const char* tap         = "tap";
inline constexpr const char* delayFactor = "delayFactor";
inline constexpr const char* width       = "width";
inline constexpr const char* speed       = "speed";
inline constexpr const char* modSource   = "modSource";
inline constexpr const char* feedback    = "feedback";
inline constexpr const char* repeat      = "repeat";
inline constexpr const char* mix         = "mix";
inline constexpr const char* stereo      = "stereo";
inline constexpr const char* character   = "character";
inline constexpr const char* output      = "output";
} // namespace ParamIDs

class SlopeboxProcessor : public juce::AudioProcessor
{
public:
    SlopeboxProcessor();
    ~SlopeboxProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 8.0; }

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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    vdc::Params readParams() const;

    vdc::AdmDelayEngine engine { slopebox::makeConfig() };
    juce::AudioBuffer<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlopeboxProcessor)
};
