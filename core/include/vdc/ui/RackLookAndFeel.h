// vintage_delay_core UI: shared rack-panel look (knobs with value arcs, latching buttons with
// a status LED, dark combo boxes). Header-only; needs the JUCE GUI modules.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace vdc::ui
{

namespace colours
{
inline const juce::Colour panel (0xff1d1f22);
inline const juce::Colour panelEdge (0xff3a3d42);
inline const juce::Colour ink (0xffe8e2d4);
inline const juce::Colour inkDim (0xff9a958a);
inline const juce::Colour accent (0xffe8a33d);
inline const juce::Colour ledRed (0xffff3b2f);
inline const juce::Colour displayBg (0xff120806);
} // namespace colours

using namespace colours;

class RackLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RackLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
};


//==============================================================================
inline RackLookAndFeel::RackLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, ink);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, inkDim);
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::ComboBox::arrowColourId, accent);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.3f));
}

inline void RackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                            float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool bipolar = slider.getProperties()["bipolar"];

    // Scale ticks.
    g.setColour (inkDim.withAlpha (0.6f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const juce::Point<float> p1 = c.getPointOnCircumference (radius, a);
        const juce::Point<float> p2 = c.getPointOnCircumference (radius - (i % 5 == 0 ? 5.0f : 3.0f), a);
        g.drawLine ({ p1, p2 }, 1.0f);
    }

    // Value arc.
    const float arcR = radius - 7.0f;
    juce::Path arc;
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (accent);
    g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knob cap.
    const float capR = radius - 11.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4d52), c.x, c.y - capR,
                                             juce::Colour (0xff17181a), c.x, c.y + capR, false));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);

    // Pointer.
    g.setColour (ink);
    g.drawLine ({ c.getPointOnCircumference (capR * 0.25f, angle), c.getPointOnCircumference (capR * 0.92f, angle) }, 2.5f);
}

inline void RackLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (down ? juce::Colour (0xff101113) : juce::Colour (0xff2b2e33).brighter (highlighted ? 0.1f : 0.0f));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    // Status LED.
    const auto led = juce::Rectangle<float> (r.getX() + 5.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.setColour (on ? ledRed : juce::Colour (0xff3d1512));
    g.fillEllipse (led);
    if (on)
    {
        g.setColour (ledRed.withAlpha (0.25f));
        g.fillEllipse (led.expanded (3.0f));
    }
}

inline void RackLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? ink : inkDim);
    g.setFont (juce::FontOptions (b.getWidth() < 60 ? 10.0f : 12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (14).withTrimmedRight (2), juce::Justification::centred);
}

inline void RackLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff2b2e33));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    juce::Path arrow;
    arrow.addTriangle ((float) w - 16.0f, (float) h * 0.4f, (float) w - 8.0f, (float) h * 0.4f, (float) w - 12.0f, (float) h * 0.62f);
    g.setColour (accent);
    g.fillPath (arrow);
}

} // namespace vdc::ui
