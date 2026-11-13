#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Dl4Config.h"
#include "UnitHiss.h"

namespace ParamIDs
{
inline constexpr const char* input       = "input";
inline constexpr const char* repeat      = "repeat";
inline constexpr const char* program     = "program";     // Manual / Auto
inline constexpr const char* scale       = "scale";       // x1 / x2 / x4
inline constexpr const char* select      = "select";      // 0..7 inside the scale window
inline constexpr const char* feedback    = "feedback";    // 0..1
inline constexpr const char* fbPhase     = "fbPhase";     // regeneration phase out (inverted)
inline constexpr const char* loEq        = "loEq";        // regeneration LO-EQ cut (175 Hz)
inline constexpr const char* hiEq        = "hiEq";        // regeneration HI-EQ cut (3.3 kHz)
inline constexpr const char* delayFactor = "delayFactor";
inline constexpr const char* width       = "width";
inline constexpr const char* speed       = "speed";
inline constexpr const char* shape       = "shape";
inline constexpr const char* delayPhase  = "delayPhase";  // delayed signal inverted in the mix
inline constexpr const char* mix         = "mix";         // 0 = source .. 1 = delay
inline constexpr const char* memory      = "memory";      // external 2.5 s memory module
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
    std::atomic<float> slewPeak { 0.0f };  // input slope, 1.0 = slew limiting (approximate)
    std::atomic<int> displayTap { 0 };   // tap in use, also while AUTO steps

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
    double lastLfoPhase = 0.0;   // AUTO program: one step per modulator cycle, in sync with the VCO
    long long autoSteps = 0;
    float lastInput = 0.0f;      // for the SLEW meter
    juce::AudioBuffer<float> scratch;
    holdfast::UnitHiss hissA { 0x1234567u }, hissB { 0x7654321u };  // VINTAGE noise floor, one per unit

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HoldfastProcessor)
};
