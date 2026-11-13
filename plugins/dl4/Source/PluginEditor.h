#pragma once

#include "PluginProcessor.h"
#include "PanelStyle.h"

class HoldfastEditor : public juce::AudioProcessorEditor,
                       private juce::Timer,
                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit HoldfastEditor (HoldfastProcessor&);
    ~HoldfastEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::String legend;
    };

    void setupKnob (Knob&, const char* paramId, const juce::String& legend, juce::Colour cap, bool onFace = false);
    void setupLever (holdfast::ui::Lever&, const char* paramId, std::function<int (float)>, std::function<float (int)>);
    void setParam (const char* paramId, int value);
    int getInt (const char* paramId) const;
    void timerCallback() override;
    void parameterChanged (const juce::String& id, float value) override;
    juce::Rectangle<float> tapLed (int tap) const;
    void drawKnobLegend (juce::Graphics&, const Knob&, bool onFace) const;

    HoldfastProcessor& proc;
    holdfast::ui::PanelLookAndFeel lnf;

    Knob input, feedback, delayFactor, width, speed, shape, mix, pedal, spread, character, output;

    holdfast::ui::Lever repeatLever { 2, false }, programLever { 2, false }, scaleLever { 3, false },
                        selectLever { 3, true, true }, loLever { 2, false }, hiLever { 2, false },
                        fbPhaseLever { 2, false }, delayPhaseLever { 2, false }, memoryLever { 2, false },
                        pedalLever { 2, false };

    juce::ComboBox stereoBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stereoAttachment;

    float shownDelayMs = -1.0f, shownPeak = -1.0f, shownSlew = -1.0f;
    int shownTap = -1;
    juce::Rectangle<int> displayArea, headroomArea, tapArea, activityArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HoldfastEditor)
};
