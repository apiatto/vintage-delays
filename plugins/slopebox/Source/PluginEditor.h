#pragma once

#include "PluginProcessor.h"

// Look of the front panel: blue 1U rack panel, knurled black knobs with a blue cap,
// rectangular latching keys in white, black and red. Drawn entirely in code (no bitmaps).
class PanelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override {}
    void drawComboBox (juce::Graphics&, int width, int height, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

// The panel, laid out in fixed design coordinates; the editor scales it.
class SlopeboxPanel : public juce::Component,
                      private juce::Timer,
                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr int designWidth = 1500;
    static constexpr int panelHeight = 172;
    static constexpr int stripHeight = 46;
    static constexpr int designHeight = panelHeight + stripHeight;

    explicit SlopeboxPanel (SlopeboxProcessor&);
    ~SlopeboxPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const char* paramId, bool small = false);
    void timerCallback() override;
    void parameterChanged (const juce::String& id, float value) override;
    void updateTapButtons();

    void drawKnobScale (juce::Graphics&, const Knob&, int ticks, float radiusOut, float radiusIn) const;
    void drawScaleLabel (juce::Graphics&, const Knob&, float position, const juce::String& text, float radius, float width = 60.0f) const;
    void drawLed (juce::Graphics&, juce::Point<float> centre, bool on, juce::Colour colour) const;
    void drawScrew (juce::Graphics&, juce::Point<float> centre) const;

    SlopeboxProcessor& proc;
    PanelLookAndFeel lnf;

    Knob input, feedback, delayFactor, width, speed, mix;
    Knob stereo, character, output;

    juce::TextButton tapButtons[6];
    juce::TextButton repeatButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> repeatAttachment;

    juce::ComboBox modBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modAttachment;

    float shownDelayMs = -1.0f;
    float shownPeak = -1.0f;
    juce::Rectangle<int> displayArea, ledArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlopeboxPanel)
};

class SlopeboxEditor : public juce::AudioProcessorEditor
{
public:
    explicit SlopeboxEditor (SlopeboxProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SlopeboxPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlopeboxEditor)
};
