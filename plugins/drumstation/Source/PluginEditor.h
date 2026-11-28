#pragma once

#include "PluginProcessor.h"

// Front panel look: black 1U panel, yellow section headers, grey label strips, white knobs
// with a dotted scale, light grey keys, red seven-segment display. Drawn in code, no bitmaps or logos.
class PanelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
};

// Slide switch with 2-3 positions, like the SELECT switches on the panel.
class SlideSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SlideSwitch (int numPositions) : positions (numPositions) {}

    void setPosition (int p, bool notify);
    int getPosition() const { return position; }
    std::function<void (int)> onChange;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    int positionAt (float x) const;
    int positions, position = 0;
};

// The panel, laid out in fixed design coordinates; the editor scales it.
class DrumstationPanel : public juce::Component,
                         private juce::Timer
{
public:
    static constexpr int designWidth = 2000;
    static constexpr int panelHeight = 186;
    static constexpr int stripHeight = 44;   // MIDI setup strip under the rack panel
    static constexpr int designHeight = panelHeight + stripHeight;

    explicit DrumstationPanel (DrumstationProcessor&);
    ~DrumstationPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::String scaleLeft = "0", scaleRight = "10";
    };

    struct SectionControls
    {
        Knob tune, level, b1, b2;
        std::unique_ptr<SlideSwitch> select, select2;   // select2: conga select on the congas section
        int voice = -1;
    };

    enum Mode { modeProgram = 0, modeRx, modeTx, modeUtility, modeSave, numModes };

    void timerCallback() override;
    void setupKnob (Knob&, bool small = false);
    void bind (Knob&, int voice, ds::Control, bool bipolarScale);
    void rebindAll();
    void setKit (int kit);
    void setEditVoice (int voice);
    int voiceForSection (int section) const;
    void keypadPressed (int key);              // 0-9, 10 = '-', 11 = '+'
    int modeValue() const;
    void setModeValue (int v);

    void drawStrip (juce::Graphics&, juce::Rectangle<float>, juce::Colour, const juce::String& text) const;
    void drawLabel (juce::Graphics&, juce::Point<float> centre, const juce::String&, float size = 9.5f,
                    juce::Colour = juce::Colours::white, float width = 90.0f) const;
    void drawKnobScale (juce::Graphics&, const Knob&) const;
    void drawLed (juce::Graphics&, juce::Point<float>, bool on, juce::Colour = juce::Colour (0xffff2a1a)) const;
    void drawSevenSegment (juce::Graphics&, juce::Rectangle<float>, int digit, bool dot) const;

    float sectionX (int section) const { return 597.0f + (float) section * 163.4f; }

    DrumstationProcessor& proc;
    PanelLookAndFeel lnf;

    Knob volume;
    juce::TextButton kit808, kit909;
    juce::TextButton keys[12];
    juce::TextButton auditionButton, modeButton;

    SectionControls sections[ds::numSections];

    // EDIT section (where the original has its logo): utility controllers of the selected sound.
    Knob frontCut, pan, distortion;
    SlideSwitch gmSwitch { 2 };

    // MIDI setup strip.
    void refreshPortLists();
    juce::ComboBox outBox, inBox, rxBox, txBox;
    juce::TextButton sendButton, syncButton, recallButton, saveDumpButton;
    std::unique_ptr<juce::FileChooser> fileChooser;
    int portRefreshCountdown = 0;
    juce::StringArray outIds, inIds;
    juce::String portStatus;

    int mode = modeProgram;
    int firstDigit = -1;
    int inLedHold = 0, outLedHold = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumstationPanel)
};

class DrumstationEditor : public juce::AudioProcessorEditor
{
public:
    explicit DrumstationEditor (DrumstationProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DrumstationPanel panel;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumstationEditor)
};
