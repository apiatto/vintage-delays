#include "PluginEditor.h"

namespace
{
using namespace vdc::ui::colours;
const juce::Colour stripe (0xff2f6f8f); // panel accent for this model
} // namespace

//==============================================================================
HoldfastEditor::HoldfastEditor (HoldfastProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    setupKnob (input, ParamIDs::input, "LEVEL");
    setupKnob (delayFactor, ParamIDs::delayFactor, "DELAY FACTOR");
    setupKnob (depth, ParamIDs::vcoDepth, "DEPTH");
    setupKnob (rate, ParamIDs::vcoRate, "RATE");
    setupKnob (shape, ParamIDs::vcoShape, "SHAPE");
    setupKnob (pedal, ParamIDs::pedal, "PEDAL");
    setupKnob (feedback, ParamIDs::feedback, "FEEDBACK", true);
    setupKnob (mix, ParamIDs::mix, "DELAY MIX", true);
    setupKnob (character, ParamIDs::character, "VINTAGE");
    setupKnob (spread, ParamIDs::spread, "SPREAD");
    setupKnob (output, ParamIDs::output, "TRIM", true);

    for (int i = 0; i < holdfast::kNumRanges; ++i)
    {
        rangeButtons[i].setButtonText (holdfast::rangeName (i));
        rangeButtons[i].onClick = [this, i] { setChoice (ParamIDs::range, i); };
        addAndMakeVisible (rangeButtons[i]);
    }
    for (int i = 0; i < holdfast::kStepsPerRange; ++i)
    {
        stepButtons[i].onClick = [this, i] { setChoice (ParamIDs::step, i); };
        addAndMakeVisible (stepButtons[i]);
    }
    for (auto* id : { ParamIDs::range, ParamIDs::step, ParamIDs::memory })
        proc.apvts.addParameterListener (id, this);

    setupToggle (repeatButton, ParamIDs::repeat, repeatAttachment);
    setupToggle (memoryButton, ParamIDs::memory, memoryAttachment);
    setupToggle (pedalButton, ParamIDs::pedalOn, pedalAttachment);

    stereoBox.addItemList ({ "Mono", "Linked Pair" }, 1);
    stereoBox.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (stereoBox);
    stereoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamIDs::stereoMode, stereoBox);

    updateDelayButtons();
    setSize (1080, 310);
    sendLookAndFeelChange();
    startTimerHz (30);
}

HoldfastEditor::~HoldfastEditor()
{
    for (auto* id : { ParamIDs::range, ParamIDs::step, ParamIDs::memory })
        proc.apvts.removeParameterListener (id, this);
    setLookAndFeel (nullptr);
}

void HoldfastEditor::setupKnob (Knob& k, const char* paramId, const juce::String& text, bool bipolar)
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

void HoldfastEditor::placeKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (16));
    k.slider.setBounds (area);
}

void HoldfastEditor::setupToggle (juce::TextButton& b, const char* paramId, std::unique_ptr<ButtonAttachment>& attachment)
{
    b.setClickingTogglesState (true);
    addAndMakeVisible (b);
    attachment = std::make_unique<ButtonAttachment> (proc.apvts, paramId, b);
}

void HoldfastEditor::setChoice (const char* paramId, int index)
{
    if (auto* param = proc.apvts.getParameter (paramId))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
        param->endChangeGesture();
    }
}

void HoldfastEditor::parameterChanged (const juce::String&, float)
{
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<HoldfastEditor> (this)]
    {
        if (safe != nullptr)
            safe->updateDelayButtons();
    });
}

void HoldfastEditor::updateDelayButtons()
{
    auto get = [this] (const char* id) { return juce::roundToInt (proc.apvts.getRawParameterValue (id)->load()); };
    const int range = juce::jlimit (0, holdfast::kNumRanges - 1, get (ParamIDs::range));
    const int step = juce::jlimit (0, holdfast::kStepsPerRange - 1, get (ParamIDs::step));
    const auto ms = holdfast::tapMs (get (ParamIDs::memory) != 0);

    for (int i = 0; i < holdfast::kNumRanges; ++i)
        rangeButtons[i].setToggleState (i == range, juce::dontSendNotification);
    for (int i = 0; i < holdfast::kStepsPerRange; ++i)
    {
        stepButtons[i].setButtonText (juce::String (ms[(size_t) holdfast::tapIndex (range, i)]));
        stepButtons[i].setToggleState (i == step, juce::dontSendNotification);
    }
    repaint();
}

void HoldfastEditor::timerCallback()
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

void HoldfastEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // Panel with a coloured stripe under the title.
    g.setGradientFill (juce::ColourGradient (panel.brighter (0.08f), 0, 0, panel.darker (0.3f), 0, r.getBottom(), false));
    g.fillAll();
    g.setColour (stripe);
    g.fillRect (28.0f, 44.0f, r.getWidth() - 56.0f, 3.0f);
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
    g.drawText ("HOLDFAST", 46, 12, 130, 26, juce::Justification::centredLeft);
    g.setColour (accent);
    g.setFont (juce::FontOptions (22.0f));
    g.drawText ("512", 162, 12, 60, 26, juce::Justification::centredLeft);
    g.setColour (inkDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("STEPPED ADM DIGITAL DELAY", 220, 18, 260, 16, juce::Justification::centredLeft);
    g.drawText (proc.isMemoryExpanded() ? "MEMORY 2048 ms" : "MEMORY 512 ms", 900, 18, 140, 16, juce::Justification::centredRight);

    // Section frames and titles.
    g.setColour (panelEdge);
    g.drawRoundedRectangle (140.5f, 56.5f, 382.0f, 240.0f, 4.0f, 1.0f); // delay
    g.drawRoundedRectangle (530.5f, 56.5f, 272.0f, 240.0f, 4.0f, 1.0f); // VCO
    g.setColour (inkDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("DELAY", 152, 60, 100, 14, juce::Justification::centredLeft);
    g.drawText ("VCO", 542, 60, 100, 14, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("RANGE", 152, rangeButtons[0].getY() - 14, 120, 12, juce::Justification::centredLeft);
    g.drawText ("STEP (ms at x1)", 152, stepButtons[0].getY() - 14, 160, 12, juce::Justification::centredLeft);
    g.drawText ("TRI - SIN - SQR", shape.slider.getX(), shape.slider.getBottom() + 2, shape.slider.getWidth(), 12, juce::Justification::centred);

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

    // LIMIT (red, at the 0 dB reference) and SIGNAL (green, above -20 dB) LEDs.
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
    drawLed ({ la.getX(), la.getY() + 18.0f, 9.0f, 9.0f }, pkDb > -20.0f, juce::Colour (0xff47e05a), "SIGNAL");
}

void HoldfastEditor::resized()
{
    // Input.
    placeKnob (input, { 46, 76, 84, 110 });
    ledArea = { 62, 200, 60, 30 };

    // Delay section.
    displayArea = { 152, 78, 150, 40 };
    memoryButton.setBounds (312, 84, 96, 28);
    repeatButton.setBounds (414, 84, 96, 28);
    for (int i = 0; i < holdfast::kNumRanges; ++i)
        rangeButtons[i].setBounds (152 + i * 78, 156, 74, 28);
    for (int i = 0; i < holdfast::kStepsPerRange; ++i)
        stepButtons[i].setBounds (152 + i * 78, 222, 74, 28);
    placeKnob (delayFactor, { 392, 140, 120, 140 });

    // VCO section.
    placeKnob (depth, { 542, 76, 84, 110 });
    placeKnob (rate, { 626, 76, 84, 110 });
    placeKnob (shape, { 710, 76, 84, 110 });
    pedalButton.setBounds (552, 226, 90, 28);
    placeKnob (pedal, { 660, 200, 62, 84 });

    // Output section.
    placeKnob (feedback, { 812, 66, 84, 116 });
    placeKnob (mix, { 898, 66, 84, 116 });
    placeKnob (character, { 984, 66, 62, 90 });
    stereoBox.setBounds (816, 214, 120, 24);
    placeKnob (spread, { 940, 196, 52, 84 });
    placeKnob (output, { 994, 196, 52, 84 });
}
