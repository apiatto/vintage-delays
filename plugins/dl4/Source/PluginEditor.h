#pragma once

#include "PluginProcessor.h"
#include <vdc/ui/RackLookAndFeel.h>

class HoldfastEditor : public juce::AudioProcessorEditor,
                       private juce::Timer,
                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit HoldfastEditor (HoldfastProcessor&);
    ~HoldfastEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void setupKnob (Knob&, const char* paramId, const juce::String& text, bool bipolar = false);
    void placeKnob (Knob&, juce::Rectangle<int> area);
    void setupToggle (juce::TextButton&, const char* paramId, std::unique_ptr<ButtonAttachment>&);
    void setChoice (const char* paramId, int index);
    void timerCallback() override;
    void parameterChanged (const juce::String& id, float value) override;
    void updateDelayButtons();

    HoldfastProcessor& proc;
    vdc::ui::RackLookAndFeel lnf;

    Knob input, delayFactor, depth, rate, shape, feedback, mix, character, spread, pedal, output;

    juce::TextButton rangeButtons[holdfast::kNumRanges];
    juce::TextButton stepButtons[holdfast::kStepsPerRange];

    juce::TextButton repeatButton { "REPEAT" }, memoryButton { "MEM x4" }, pedalButton { "PEDAL" };
    std::unique_ptr<ButtonAttachment> repeatAttachment, memoryAttachment, pedalAttachment;

    juce::ComboBox stereoBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stereoAttachment;

    float shownDelayMs = -1.0f;
    float shownPeak = -1.0f;
    juce::Rectangle<int> displayArea, ledArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HoldfastEditor)
};
