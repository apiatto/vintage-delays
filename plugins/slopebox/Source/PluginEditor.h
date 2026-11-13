#pragma once

#include "PluginProcessor.h"
#include <vdc/ui/RackLookAndFeel.h>

class SlopeboxEditor : public juce::AudioProcessorEditor,
                       private juce::Timer,
                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit SlopeboxEditor (SlopeboxProcessor&);
    ~SlopeboxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const char* paramId, const juce::String& text, bool bipolar = false);
    void placeKnob (Knob&, juce::Rectangle<int> area);
    void timerCallback() override;
    void parameterChanged (const juce::String& id, float value) override;
    void updateTapButtons();

    SlopeboxProcessor& proc;
    vdc::ui::RackLookAndFeel lnf;

    Knob input, feedback, delayFactor, width, speed, mix, stereo, character, output;

    juce::TextButton tapButtons[6];
    juce::TextButton repeatButton { "REPEAT" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> repeatAttachment;

    juce::ComboBox modBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modAttachment;

    float shownDelayMs = -1.0f;
    float shownPeak = -1.0f;
    juce::Rectangle<int> displayArea, ledArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlopeboxEditor)
};
