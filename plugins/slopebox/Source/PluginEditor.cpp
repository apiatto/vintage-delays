#include "PluginEditor.h"

namespace
{
const juce::Colour panelTop (0xff2a5ea6);
const juce::Colour panelBottom (0xff1b4686);
const juce::Colour silk (0xfff2f4f8);         // white screen printing
const juce::Colour silkDim (0xffc9d5e8);
const juce::Colour stripBg (0xff15171a);
const juce::Colour stripInk (0xffb8b4aa);
const juce::Colour ledRed (0xffff3b2f);
const juce::Colour ledGreen (0xff57e05a);
const juce::Colour ledAmber (0xffffb21e);

const char* tapMs[6] = { "1", "4", "16", "64", "256", "1024" };
const char* tapNames[6] = { "HI", "MID", "LO", "DOUBLE", "SHORT", "LONG" };

constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.25f;
constexpr float kEndAngle = juce::MathConstants<float>::pi * 2.75f;

juce::Font panelFont (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), size, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace

//==============================================================================
PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, stripInk);
    setColour (juce::PopupMenu::backgroundColourId, stripBg);
    setColour (juce::PopupMenu::textColourId, stripInk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, panelBottom);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xf0101114));
    setColour (juce::BubbleComponent::outlineColourId, silkDim);
    setColour (juce::TooltipWindow::textColourId, silk);
}

void PanelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool small = slider.getProperties()["small"];

    // Shadow.
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - radius + 2.0f, c.y - radius + 3.0f, radius * 2.0f, radius * 2.0f);

    // Knurled skirt: a polygon with small ridges.
    juce::Path skirt;
    const int ridges = small ? 18 : 28;
    for (int i = 0; i <= ridges * 2; ++i)
    {
        const float a = (float) i / (float) (ridges * 2) * juce::MathConstants<float>::twoPi + angle;
        const float r = (i % 2 == 0) ? radius : radius * 0.93f;
        const auto pt = c.getPointOnCircumference (r, a);
        if (i == 0) skirt.startNewSubPath (pt);
        else        skirt.lineTo (pt);
    }
    skirt.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3c40), c.x - radius, c.y - radius,
                                             juce::Colour (0xff050506), c.x + radius, c.y + radius, false));
    g.fillPath (skirt);

    // Top of the knob.
    const float topR = radius * 0.82f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2e33), c.x, c.y - topR,
                                             juce::Colour (0xff0b0b0d), c.x, c.y + topR, false));
    g.fillEllipse (c.x - topR, c.y - topR, topR * 2.0f, topR * 2.0f);

    // Blue cap.
    const float capR = radius * 0.5f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4f86d6), c.x - capR * 0.4f, c.y - capR,
                                             juce::Colour (0xff163c78), c.x + capR * 0.4f, c.y + capR, false));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);

    // White pointer line across the knob top.
    g.setColour (silk);
    g.drawLine ({ c.getPointOnCircumference (capR * 0.15f, angle), c.getPointOnCircumference (topR * 0.98f, angle) },
                small ? 2.0f : 3.0f);
}

void PanelLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    // Latching rectangular key. "keyColour": 0 white, 1 black, 2 red.
    const int kind = b.getProperties()["keyColour"];
    const bool pressed = b.getToggleState() || down;
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);

    // Bezel.
    g.setColour (juce::Colour (0xff0d1a30));
    g.fillRect (r);
    r = r.reduced (2.0f);

    juce::Colour top, bottom;
    switch (kind)
    {
        case 1:  top = juce::Colour (0xff3a3b3e); bottom = juce::Colour (0xff111113); break;
        case 2:  top = juce::Colour (0xffff4a40); bottom = juce::Colour (0xffc4140c); break;
        default: top = juce::Colour (0xfffbfbf8); bottom = juce::Colour (0xffd3d4d0); break;
    }
    if (pressed)
    {
        // A latched key sits lower in the bezel and catches less light.
        top = top.darker (kind == 1 ? 0.6f : 0.18f);
        bottom = bottom.darker (kind == 1 ? 0.6f : 0.18f);
        r = r.withTrimmedTop (3.0f);
    }
    else if (highlighted)
    {
        top = top.brighter (0.06f);
    }
    g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRect (r);

    // Edge highlight and shading.
    g.setColour (juce::Colours::white.withAlpha (pressed ? 0.08f : 0.35f));
    g.drawLine (r.getX(), r.getY() + 0.5f, r.getRight(), r.getY() + 0.5f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawLine (r.getX(), r.getBottom() - 0.5f, r.getRight(), r.getBottom() - 0.5f, 1.0f);

    if (pressed)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (r.withHeight (3.0f));
    }
}

void PanelLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (0.5f);
    g.setColour (juce::Colour (0xff222428));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colour (0xff3a3d42));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    juce::Path arrow;
    arrow.addTriangle ((float) w - 15.0f, (float) h * 0.42f, (float) w - 7.0f, (float) h * 0.42f, (float) w - 11.0f, (float) h * 0.62f);
    g.setColour (silkDim);
    g.fillPath (arrow);
}

juce::Font PanelLookAndFeel::getComboBoxFont (juce::ComboBox&) { return panelFont (12.0f, false); }

void PanelLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

//==============================================================================
SlopeboxPanel::SlopeboxPanel (SlopeboxProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);

    setupKnob (input, ParamIDs::input);
    setupKnob (feedback, ParamIDs::feedback);
    setupKnob (delayFactor, ParamIDs::delayFactor);
    setupKnob (width, ParamIDs::width);
    setupKnob (speed, ParamIDs::speed);
    setupKnob (mix, ParamIDs::mix);
    setupKnob (stereo, ParamIDs::stereo, true);
    setupKnob (character, ParamIDs::character, true);
    setupKnob (output, ParamIDs::output, true);

    for (int i = 0; i < 6; ++i)
    {
        auto& b = tapButtons[i];
        b.getProperties().set ("keyColour", i < 4 ? 0 : 1);
        b.setClickingTogglesState (false);
        b.setTooltip (juce::String (tapNames[i]) + " (" + tapMs[i] + " ms at x1). Press the lit key again to release all keys.");
        b.onClick = [this, i]
        {
            if (auto* param = proc.apvts.getParameter (ParamIDs::tap))
            {
                // Pressing the latched key releases it: all keys out = memory bypassed.
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

    repeatButton.getProperties().set ("keyColour", 2);
    repeatButton.setClickingTogglesState (true);
    repeatButton.setTooltip ("Infinite repeat: the whole memory loops, the input is disconnected.");
    addAndMakeVisible (repeatButton);
    repeatAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ParamIDs::repeat, repeatButton);

    modBox.addItemList ({ "Sine LFO", "Triangle LFO", "Envelope" }, 1);
    addAndMakeVisible (modBox);
    modAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamIDs::modSource, modBox);

    startTimerHz (30);
}

SlopeboxPanel::~SlopeboxPanel()
{
    proc.apvts.removeParameterListener (ParamIDs::tap, this);
    setLookAndFeel (nullptr);
}

void SlopeboxPanel::setupKnob (Knob& k, const char* paramId, bool small)
{
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (kStartAngle, kEndAngle, true);
    k.slider.setPopupDisplayEnabled (true, true, this);
    k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (paramId)->convertFrom0to1 (proc.apvts.getParameter (paramId)->getDefaultValue()));
    k.slider.getProperties().set ("small", small);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

void SlopeboxPanel::parameterChanged (const juce::String&, float)
{
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<SlopeboxPanel> (this)]
    {
        if (safe != nullptr)
            safe->updateTapButtons();
    });
}

void SlopeboxPanel::updateTapButtons()
{
    const int current = juce::roundToInt (proc.apvts.getRawParameterValue (ParamIDs::tap)->load());
    for (int i = 0; i < 6; ++i)
        tapButtons[i].setToggleState (i + 1 == current, juce::dontSendNotification);
}

void SlopeboxPanel::timerCallback()
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

void SlopeboxPanel::drawKnobScale (juce::Graphics& g, const Knob& k, int ticks, float radiusOut, float radiusIn) const
{
    const auto c = k.slider.getBounds().toFloat().getCentre();
    g.setColour (silk);
    for (int i = 0; i < ticks; ++i)
    {
        const float a = kStartAngle + (float) i / (float) (ticks - 1) * (kEndAngle - kStartAngle);
        g.drawLine ({ c.getPointOnCircumference (radiusIn, a), c.getPointOnCircumference (radiusOut, a) }, 1.6f);
    }
}

void SlopeboxPanel::drawScaleLabel (juce::Graphics& g, const Knob& k, float position, const juce::String& text, float radius, float w) const
{
    const auto c = k.slider.getBounds().toFloat().getCentre();
    const float a = kStartAngle + position * (kEndAngle - kStartAngle);
    const auto pt = c.getPointOnCircumference (radius, a);
    g.drawText (text, juce::Rectangle<float> (w, 14.0f).withCentre (pt), juce::Justification::centred);
}

void SlopeboxPanel::drawLed (juce::Graphics& g, juce::Point<float> centre, bool on, juce::Colour colour) const
{
    const float r = 4.5f;
    if (on)
    {
        g.setColour (colour.withAlpha (0.3f));
        g.fillEllipse (centre.x - r * 2.2f, centre.y - r * 2.2f, r * 4.4f, r * 4.4f);
    }
    g.setColour (juce::Colour (0xff0a1222));
    g.fillEllipse (centre.x - r - 1.5f, centre.y - r - 1.5f, (r + 1.5f) * 2.0f, (r + 1.5f) * 2.0f);
    g.setGradientFill (juce::ColourGradient (on ? colour.brighter (0.5f) : colour.darker (1.6f), centre.x - r * 0.4f, centre.y - r * 0.6f,
                                             on ? colour : colour.darker (2.6f), centre.x + r, centre.y + r, true));
    g.fillEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
}

void SlopeboxPanel::drawScrew (juce::Graphics& g, juce::Point<float> c) const
{
    const float r = 9.0f;
    g.setColour (juce::Colour (0xff0b1426));
    g.fillEllipse (c.x - r - 2.0f, c.y - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6d7178), c.x - r, c.y - r, juce::Colour (0xff17181b), c.x + r, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colour (0xff0a0a0b));
    g.drawLine (c.x - r * 0.65f, c.y - r * 0.2f, c.x + r * 0.65f, c.y + r * 0.2f, 2.5f);
}

void SlopeboxPanel::paint (juce::Graphics& g)
{
    const auto panel = juce::Rectangle<float> (0.0f, 0.0f, (float) designWidth, (float) panelHeight);

    // Brushed blue aluminium.
    g.setGradientFill (juce::ColourGradient (panelTop, 0.0f, 0.0f, panelBottom, 0.0f, panel.getBottom(), false));
    g.fillRect (panel);
    {
        juce::Random rng (11);
        for (int i = 0; i < 260; ++i)
        {
            const float yy = rng.nextFloat() * panel.getHeight();
            g.setColour (juce::Colours::white.withAlpha (0.015f + rng.nextFloat() * 0.02f));
            g.drawHorizontalLine ((int) yy, 0.0f, panel.getRight());
        }
    }
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawHorizontalLine (1, 0.0f, panel.getRight());
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawHorizontalLine (panelHeight - 1, 0.0f, panel.getRight());

    // Rack screws.
    for (auto pt : { juce::Point<float> (104.0f, 40.0f), juce::Point<float> (104.0f, 128.0f),
                     juce::Point<float> (1418.0f, 40.0f), juce::Point<float> (1418.0f, 128.0f) })
        drawScrew (g, pt);

    // Maker badge at the left (our own name, not a trademark of the original).
    {
        auto badge = juce::Rectangle<float> (16.0f, 66.0f, 70.0f, 36.0f);
        g.setColour (silk);
        g.drawRect (badge, 1.5f);
        g.setFont (panelFont (12.0f));
        g.drawText ("SLOPE", badge.removeFromTop (18.0f), juce::Justification::centredBottom);
        g.drawText ("BOX", badge, juce::Justification::centredTop);
    }

    g.setColour (silk);

    // INPUT section.
    g.setFont (panelFont (15.0f));
    g.drawText ("INPUT", input.slider.getBounds().getCentreX() - 50, 8, 100, 18, juce::Justification::centred);
    g.drawText ("LEVEL", input.slider.getBounds().getCentreX() - 50, 140, 100, 18, juce::Justification::centred);
    g.setFont (panelFont (11.0f));
    g.drawText ("LIMIT", 128, 62, 50, 14, juce::Justification::centred);
    g.drawText ("ACTIVE", 128, 116, 50, 14, juce::Justification::centred);
    drawKnobScale (g, input, 11, 47.0f, 42.0f);
    drawScaleLabel (g, input, 0.0f, "MIN", 58.0f);
    drawScaleLabel (g, input, 1.0f, "MAX", 58.0f);

    // FEEDBACK.
    g.setFont (panelFont (15.0f));
    g.drawText ("FEEDBACK", feedback.slider.getBounds().getCentreX() - 60, 140, 120, 18, juce::Justification::centred);
    drawKnobScale (g, feedback, 11, 47.0f, 42.0f);
    g.setFont (panelFont (11.0f));
    drawScaleLabel (g, feedback, 0.0f, "( - )", 60.0f);
    drawScaleLabel (g, feedback, 1.0f, "( + )", 60.0f);
    drawScaleLabel (g, feedback, 0.5f, "0", 55.0f);

    // Title block with speed lines, like a period front panel.
    {
        const auto title = juce::Rectangle<float> (444.0f, 10.0f, 324.0f, 38.0f);
        g.setColour (silkDim.withAlpha (0.8f));
        for (int i = 0; i < 6; ++i)
            g.drawHorizontalLine ((int) (title.getY() + 6.0f + i * 5.5f), title.getX(), title.getRight());
        g.setColour (panelTop);
        g.fillRect (title.reduced (52.0f, 0.0f));
        g.setColour (silk);
        g.setFont (panelFont (30.0f).italicised().withExtraKerningFactor (0.06f));
        g.drawText ("SLOPEBOX", title, juce::Justification::centred);
    }

    // DIGITAL DELAY bracket and legends.
    {
        const float x0 = (float) tapButtons[0].getX(), x1 = (float) tapButtons[5].getRight();
        g.setFont (panelFont (14.0f));
        g.drawText ("DIGITAL DELAY (ms)", juce::Rectangle<float> (x0, 50.0f, x1 - x0, 16.0f), juce::Justification::centred);
        g.drawLine (x0, 66.0f, x0, 60.0f, 1.2f);
        g.drawLine (x1, 66.0f, x1, 60.0f, 1.2f);
        g.drawLine (x0, 60.0f, x0 + 75.0f, 60.0f, 1.2f);
        g.drawLine (x1 - 75.0f, 60.0f, x1, 60.0f, 1.2f);

        g.setFont (panelFont (11.0f));
        for (int i = 0; i < 6; ++i)
        {
            const auto b = tapButtons[i].getBounds();
            g.drawText (tapMs[i], b.getX(), b.getY() - 15, b.getWidth(), 13, juce::Justification::centred);
            if (i != 3)
                g.drawText (tapNames[i], b.getX(), b.getBottom() + 3, b.getWidth(), 13, juce::Justification::centred);
        }
        const auto b3 = tapButtons[3].getBounds();
        g.setFont (panelFont (12.0f));
        g.drawText ("DOUBLE", b3.getX() - 10, b3.getBottom() + 17, b3.getWidth() + 20, 14, juce::Justification::centred);

        auto bracket = [&] (int from, int to, const char* text)
        {
            const float bx0 = (float) tapButtons[from].getX() + 6.0f, bx1 = (float) tapButtons[to].getRight() - 6.0f;
            const float yy = (float) tapButtons[from].getBottom() + 24.0f;
            g.drawLine (bx0, yy - 5.0f, bx0, yy, 1.2f);
            g.drawLine (bx1, yy - 5.0f, bx1, yy, 1.2f);
            const float mid = (bx0 + bx1) * 0.5f;
            g.drawLine (bx0, yy, mid - 30.0f, yy, 1.2f);
            g.drawLine (mid + 30.0f, yy, bx1, yy, 1.2f);
            g.drawText (text, juce::Rectangle<float> (60.0f, 14.0f).withCentre ({ mid, yy }), juce::Justification::centred);
        };
        g.setFont (panelFont (12.0f));
        bracket (0, 2, "FLANGE");
        bracket (4, 5, "ECHO");

        const auto rb = repeatButton.getBounds();
        g.setFont (panelFont (11.0f));
        g.drawText ("INFINITE", rb.getX() - 10, rb.getBottom() + 3, rb.getWidth() + 20, 13, juce::Justification::centred);
        g.drawText ("REPEAT", rb.getX() - 10, rb.getBottom() + 17, rb.getWidth() + 20, 14, juce::Justification::centred);
    }

    // DELAY FACTOR.
    g.setFont (panelFont (15.0f));
    g.drawText ("DELAY FACTOR", delayFactor.slider.getBounds().getCentreX() - 70, 140, 140, 18, juce::Justification::centred);
    drawKnobScale (g, delayFactor, 16, 47.0f, 42.0f);
    g.setFont (panelFont (11.0f));
    drawScaleLabel (g, delayFactor, 0.0f, ".25", 58.0f);
    drawScaleLabel (g, delayFactor, 1.0f, "1.0", 58.0f);

    // MODULATOR box.
    {
        const auto box = juce::Rectangle<float> (924.0f, 8.0f, 296.0f, 156.0f);
        g.drawRect (box, 2.5f);
        g.setFont (panelFont (15.0f));
        g.drawText ("MODULATOR", box.withHeight (26.0f).translated (0.0f, 6.0f), juce::Justification::centred);
        g.drawText ("WIDTH", width.slider.getBounds().getCentreX() - 50, 140, 100, 18, juce::Justification::centred);
        g.drawText ("SPEED", speed.slider.getBounds().getCentreX() - 50, 140, 100, 18, juce::Justification::centred);
        drawKnobScale (g, width, 11, 44.0f, 39.0f);
        drawKnobScale (g, speed, 11, 44.0f, 39.0f);
        g.setFont (panelFont (11.0f));
        drawScaleLabel (g, width, 0.0f, "0", 54.0f);
        drawScaleLabel (g, width, 1.0f, "100%", 60.0f);
        drawScaleLabel (g, speed, 0.0f, ".1Hz", 60.0f);
        drawScaleLabel (g, speed, 1.0f, "10Hz", 60.0f);
    }

    // DELAY MIX with the centre-tapped scale.
    g.setFont (panelFont (15.0f));
    g.drawText ("DELAY MIX", mix.slider.getBounds().getCentreX() - 60, 140, 120, 18, juce::Justification::centred);
    drawKnobScale (g, mix, 11, 47.0f, 42.0f);
    g.setFont (panelFont (10.5f));
    drawScaleLabel (g, mix, 0.0f, "- 100%", 66.0f);
    drawScaleLabel (g, mix, 0.25f, "- 50%", 72.0f);
    drawScaleLabel (g, mix, 0.5f, "SOURCE", 56.0f);
    drawScaleLabel (g, mix, 0.75f, "+ 50%", 72.0f);
    drawScaleLabel (g, mix, 1.0f, "+ 100%", 66.0f);

    // Model badge and power LED at the right.
    g.setColour (silk);
    g.setFont (panelFont (22.0f).italicised());
    g.drawText ("1024", 1436, 70, 60, 26, juce::Justification::centredLeft);
    g.setFont (panelFont (9.0f));
    g.drawText ("ADAPTIVE", 1436, 96, 60, 11, juce::Justification::centredLeft);
    g.drawText ("DELTA MOD", 1436, 107, 60, 11, juce::Justification::centredLeft);
    drawLed (g, { 1410.0f, 84.0f }, true, ledAmber);
    g.setFont (panelFont (9.0f));
    g.drawText ("PWR", 1395, 92, 30, 12, juce::Justification::centred);

    // LIMIT and ACTIVE LEDs: LIMIT at the 0 dB reference, ACTIVE above -20 dB.
    const float pkDb = juce::Decibels::gainToDecibels (shownPeak, -80.0f);
    drawLed (g, { 153.0f, 84.0f }, pkDb > -0.5f, ledRed);
    drawLed (g, { 153.0f, 106.0f }, pkDb > -20.0f, ledGreen);

    // Lower strip: plugin extras that the hardware did not have.
    const auto strip = juce::Rectangle<float> (0.0f, (float) panelHeight, (float) designWidth, (float) stripHeight);
    g.setColour (stripBg);
    g.fillRect (strip);
    g.setColour (stripInk);
    g.setFont (panelFont (11.0f));
    g.drawText ("DELAY", 40, panelHeight + 8, 50, 30, juce::Justification::centredRight);
    g.drawText ("MODULATOR", 270, panelHeight + 8, 80, 30, juce::Justification::centredRight);
    g.drawText ("STEREO", stereo.slider.getX() - 66, panelHeight + 8, 60, 30, juce::Justification::centredRight);
    g.drawText ("VINTAGE", character.slider.getX() - 66, panelHeight + 8, 60, 30, juce::Justification::centredRight);
    g.drawText ("TRIM", output.slider.getX() - 66, panelHeight + 8, 60, 30, juce::Justification::centredRight);

    // Delay readout, red LED digits.
    g.setColour (juce::Colour (0xff120806));
    g.fillRoundedRectangle (displayArea.toFloat(), 3.0f);
    g.setColour (ledRed);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 20.0f, juce::Font::bold)));
    juce::String txt;
    if (shownDelayMs <= 0.0f)       txt = "--";
    else if (shownDelayMs < 10.0f)  txt = juce::String (shownDelayMs, 2);
    else if (shownDelayMs < 100.0f) txt = juce::String (shownDelayMs, 1);
    else                            txt = juce::String (juce::roundToInt (shownDelayMs));
    g.drawText (txt, displayArea.reduced (8, 0).withTrimmedRight (24), juce::Justification::centredRight);
    g.setFont (panelFont (10.0f, false));
    g.drawText ("ms", displayArea.reduced (6, 0), juce::Justification::centredRight);
}

void SlopeboxPanel::resized()
{
    const int knob = 84;
    auto knobAt = [knob] (int cx, int cy) { return juce::Rectangle<int> (knob, knob).withCentre ({ cx, cy }); };

    input.slider.setBounds (knobAt (228, 84));
    feedback.slider.setBounds (knobAt (352, 84));

    const int keyW = 46, keyH = 30, keyY = 82;
    const int keyX[6] = { 446, 494, 542, 590, 638, 686 };
    for (int i = 0; i < 6; ++i)
        tapButtons[i].setBounds (keyX[i], keyY, keyW, keyH);
    repeatButton.setBounds (734, keyY, keyW, keyH);

    delayFactor.slider.setBounds (knobAt (856, 84));
    width.slider.setBounds (juce::Rectangle<int> (78, 78).withCentre ({ 1000, 90 }));
    speed.slider.setBounds (juce::Rectangle<int> (78, 78).withCentre ({ 1144, 90 }));
    mix.slider.setBounds (knobAt (1300, 84));

    // Lower strip.
    const int sy = panelHeight + 7;
    displayArea = { 96, sy + 1, 130, 30 };
    modBox.setBounds (356, sy + 4, 150, 24);
    stereo.slider.setBounds (1010, sy - 2, 36, 36);
    character.slider.setBounds (1160, sy - 2, 36, 36);
    output.slider.setBounds (1300, sy - 2, 36, 36);

    ledArea = { 140, 70, 30, 50 };
}

//==============================================================================
SlopeboxEditor::SlopeboxEditor (SlopeboxProcessor& p)
    : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    panel.setBounds (0, 0, SlopeboxPanel::designWidth, SlopeboxPanel::designHeight);

    const double aspect = (double) SlopeboxPanel::designWidth / SlopeboxPanel::designHeight;
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio (aspect);
        c->setSizeLimits (750, (int) (750 / aspect), 2250, (int) (2250 / aspect));
    }
    setSize (1200, (int) std::round (1200 / aspect));
}

void SlopeboxEditor::paint (juce::Graphics& g)
{
    g.fillAll (stripBg);
}

void SlopeboxEditor::resized()
{
    const float scale = (float) getWidth() / (float) SlopeboxPanel::designWidth;
    panel.setTransform (juce::AffineTransform::scale (scale));
}
