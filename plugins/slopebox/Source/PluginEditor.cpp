#include "PluginEditor.h"

namespace
{
using namespace vdc::ui::colours;
const char* tapNames[6] = { "HI", "MID", "LO", "DBL", "SHORT", "LONG" };
const char* tapMs[6] = { "1", "4", "16", "64", "256", "1024" };
} // namespace

//==============================================================================
SlopeboxEditor::SlopeboxEditor (SlopeboxProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    setupKnob (input, ParamIDs::input, "LEVEL");
    setupKnob (feedback, ParamIDs::feedback, "FEEDBACK", true);
    setupKnob (delayFactor, ParamIDs::delayFactor, "DELAY FACTOR");
    setupKnob (width, ParamIDs::width, "WIDTH");
    setupKnob (speed, ParamIDs::speed, "SPEED");
    setupKnob (mix, ParamIDs::mix, "DELAY MIX", true);
    setupKnob (stereo, ParamIDs::stereo, "STEREO");
    setupKnob (character, ParamIDs::character, "VINTAGE");
    setupKnob (output, ParamIDs::output, "TRIM", true);

    for (int i = 0; i < 6; ++i)
    {
        auto& b = tapButtons[i];
        b.setButtonText (tapNames[i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i]
        {
            if (auto* param = proc.apvts.getParameter (ParamIDs::tap))
            {
                // Pressing the lit button releases it: all buttons out = memory bypassed.
                const int current = juce::roundToInt (proc.apvts.getRawParameterValue (ParamIDs::tap)->load());
                const int next = current == i + 1 ? 0 : i + 1;
                param->beginChangeGesture();
                param->setValueNotifyingHost (param->convertTo0to1 ((float) next));
                param->endChangeGesture();
            }
        };
        addAndMakeVisible (b);
    }
    proc.apvts.addParameterListener (ParamIDs::tap, this);
    updateTapButtons();

    repeatButton.setClickingTogglesState (true);
    addAndMakeVisible (repeatButton);
    repeatAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::repeat, repeatButton);

    modBox.addItemList ({ "Sine LFO", "Triangle LFO", "Envelope" }, 1);
    modBox.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (modBox);
    modAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamIDs::modSource, modBox);

    setSize (1000, 300);
    sendLookAndFeelChange();
    startTimerHz (30);
}

SlopeboxEditor::~SlopeboxEditor()
{
    proc.apvts.removeParameterListener (ParamIDs::tap, this);
    setLookAndFeel (nullptr);
}

void SlopeboxEditor::setupKnob (Knob& k, const char* paramId, const juce::String& text, bool bipolar)
{
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    k.slider.getProperties().set ("bipolar", bipolar);
    k.slider.setColour (juce::Slider::textBoxTextColourId, ink);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff141517));
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, panelEdge);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (k.slider);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

void SlopeboxEditor::placeKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (16));
    k.slider.setBounds (area);
}

void SlopeboxEditor::parameterChanged (const juce::String&, float)
{
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<SlopeboxEditor> (this)]
    {
        if (safe != nullptr)
            safe->updateTapButtons();
    });
}

void SlopeboxEditor::updateTapButtons()
{
    const int current = juce::roundToInt (proc.apvts.getRawParameterValue (ParamIDs::tap)->load());
    for (int i = 0; i < 6; ++i)
        tapButtons[i].setToggleState (i + 1 == current, juce::dontSendNotification);
}

void SlopeboxEditor::timerCallback()
{
    const float ms = proc.displayDelayMs.load();
    const float pk = proc.inputPeak.load();
    if (std::abs (ms - shownDelayMs) > 0.005f || std::abs (pk - shownPeak) > 0.01f)
    {
        shownDelayMs = ms;
        shownPeak = pk;
        repaint (displayArea.getUnion (ledArea));
    }
}

void SlopeboxEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // Panel.
    g.setGradientFill (juce::ColourGradient (panel.brighter (0.08f), 0, 0, panel.darker (0.3f), 0, r.getBottom(), false));
    g.fillAll();
    g.setColour (panelEdge);
    g.drawRect (r, 2.0f);

    // Rack ears.
    for (float ex : { 0.0f, r.getRight() - 28.0f })
    {
        auto ear = juce::Rectangle<float> (ex, 0, 28.0f, r.getHeight());
        g.setColour (juce::Colour (0xff2a2c30));
        g.fillRect (ear);
        g.setColour (juce::Colour (0xff0c0c0d));
        for (float hy : { 40.0f, r.getHeight() - 54.0f })
            g.fillRoundedRectangle (ear.getCentreX() - 5.0f, hy, 10.0f, 14.0f, 5.0f);
    }

    // Title.
    g.setColour (ink);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("SLOPEBOX", 46, 12, 140, 26, juce::Justification::centredLeft);
    g.setColour (accent);
    g.setFont (juce::FontOptions (22.0f));
    g.drawText ("1024", 162, 12, 70, 26, juce::Justification::centredLeft);
    g.setColour (inkDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("ADAPTIVE DELTA MODULATION DELAY", 240, 18, 260, 16, juce::Justification::centredLeft);

    // Section frames.
    g.setColour (panelEdge);
    g.drawRoundedRectangle (232.5f, 52.5f, 320.0f, 232.0f, 4.0f, 1.0f); // digital delay
    g.drawRoundedRectangle (652.5f, 52.5f, 186.0f, 232.0f, 4.0f, 1.0f); // modulator

    g.setColour (inkDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("DIGITAL DELAY (ms)", 244, 56, 200, 14, juce::Justification::centredLeft);
    g.drawText ("MODULATOR", 664, 56, 160, 14, juce::Justification::centredLeft);
    g.drawText ("INPUT", 46, 56, 84, 14, juce::Justification::centred);

    // Button legends: nominal ms above, groups below.
    g.setFont (juce::FontOptions (10.0f));
    for (int i = 0; i < 6; ++i)
    {
        const auto b = tapButtons[i].getBounds();
        g.drawText (tapMs[i], b.getX(), b.getY() - 14, b.getWidth(), 12, juce::Justification::centred);
    }
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    const auto b0 = tapButtons[0].getBounds(), b2 = tapButtons[2].getBounds(), b3 = tapButtons[3].getBounds();
    const auto b4 = tapButtons[4].getBounds(), b5 = tapButtons[5].getBounds();
    g.drawText ("FLANGE", b0.getX(), b0.getBottom() + 3, b2.getRight() - b0.getX(), 12, juce::Justification::centred);
    g.drawText ("DOUBLE", b3.getX() - 6, b3.getBottom() + 3, b3.getWidth() + 12, 12, juce::Justification::centred);
    g.drawText ("ECHO", b4.getX(), b4.getBottom() + 3, b5.getRight() - b4.getX(), 12, juce::Justification::centred);

    // Delay readout.
    g.setColour (displayBg);
    g.fillRoundedRectangle (displayArea.toFloat(), 3.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (displayArea.toFloat(), 3.0f, 1.5f);
    g.setColour (ledRed);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 24.0f, juce::Font::bold));
    juce::String txt;
    if (shownDelayMs <= 0.0f)            txt = "--";
    else if (shownDelayMs < 10.0f)       txt = juce::String (shownDelayMs, 2);
    else if (shownDelayMs < 100.0f)      txt = juce::String (shownDelayMs, 1);
    else                                 txt = juce::String (juce::roundToInt (shownDelayMs));
    g.drawText (txt, displayArea.reduced (8, 0).withTrimmedRight (26), juce::Justification::centredRight);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("ms", displayArea.reduced (6, 0), juce::Justification::centredRight);

    // LIMIT (red, at the 0 dB reference) and ACTIVE (green, above -20 dB) LEDs.
    const float pkDb = juce::Decibels::gainToDecibels (shownPeak, -80.0f);
    auto drawLed = [&] (juce::Rectangle<float> led, bool on, juce::Colour col, const char* label)
    {
        if (on)
        {
            g.setColour (col.withAlpha (0.25f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setColour (on ? col : col.darker (2.2f));
        g.fillEllipse (led);
        g.setColour (inkDim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (label, (int) led.getRight() + 5, (int) led.getY() - 3, 50, 14, juce::Justification::centredLeft);
    };
    const auto la = ledArea.toFloat();
    drawLed ({ la.getX(), la.getY(), 9.0f, 9.0f }, pkDb > -0.5f, ledRed, "LIMIT");
    drawLed ({ la.getX(), la.getY() + 18.0f, 9.0f, 9.0f }, pkDb > -20.0f, juce::Colour (0xff47e05a), "ACTIVE");
}

void SlopeboxEditor::resized()
{
    const int top = 72;

    placeKnob (input, { 46, top, 84, 110 });
    ledArea = { 62, top + 122, 60, 30 };
    placeKnob (feedback, { 138, top, 86, 110 });

    displayArea = { 244, 76, 150, 40 };
    for (int i = 0; i < 6; ++i)
        tapButtons[i].setBounds (244 + i * 50, 150, 48, 28);
    repeatButton.setBounds (244, 214, 100, 28);

    placeKnob (delayFactor, { 560, top, 88, 120 });

    placeKnob (width, { 660, top + 8, 84, 110 });
    placeKnob (speed, { 748, top + 8, 84, 110 });
    modBox.setBounds (672, top + 134, 150, 24);

    placeKnob (mix, { 846, top, 92, 120 });

    placeKnob (stereo, { 560, 200, 62, 84 });
    placeKnob (character, { 846, 200, 46, 84 });
    placeKnob (output, { 894, 200, 46, 84 });
}
