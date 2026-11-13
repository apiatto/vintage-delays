// Holdfast 512 panel: a blue 1U rack face with light-blue control sections, coloured knob caps,
// bat-handle toggle switches and LED ladders. Everything is drawn in code.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace holdfast::ui
{

namespace colours
{
inline const juce::Colour face (0xff1d3f86);       // rack face
inline const juce::Colour faceDark (0xff142e66);
inline const juce::Colour section (0xffa9cdee);    // control sections
inline const juce::Colour sectionEdge (0xffe6f2fc);
inline const juce::Colour legendDark (0xff16306a); // print on the sections
inline const juce::Colour legendLight (0xffeef4fb);// print on the face
inline const juce::Colour ledGreen (0xff3df05a);
inline const juce::Colour ledRed (0xffff3426);
inline const juce::Colour ledYellow (0xffffc828);
inline const juce::Colour ledOrange (0xffff8a1e);
inline const juce::Colour capBlack (0xff1b1b1d);
inline const juce::Colour capWhite (0xfff3f3ef);
inline const juce::Colour capYellow (0xfff2d22a);
inline const juce::Colour capRed (0xffc4241c);
inline const juce::Colour capGrey (0xffa3a8ae);
} // namespace colours

inline void drawLed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool on)
{
    if (on)
    {
        g.setColour (c.withAlpha (0.35f));
        g.fillEllipse (r.expanded (r.getWidth() * 0.45f));
    }
    g.setColour (on ? c : c.darker (2.4f).withSaturation (0.6f));
    g.fillEllipse (r);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.55f : 0.15f));
    g.fillEllipse (r.reduced (r.getWidth() * 0.3f).translated (-r.getWidth() * 0.12f, -r.getHeight() * 0.12f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (r, 0.8f);
}

class PanelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PanelLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, colours::legendDark);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, colours::legendDark);
        setColour (juce::ComboBox::textColourId, colours::legendLight);
        setColour (juce::ComboBox::backgroundColourId, colours::faceDark);
        setColour (juce::ComboBox::outlineColourId, colours::sectionEdge.withAlpha (0.5f));
        setColour (juce::ComboBox::arrowColourId, colours::legendLight);
        setColour (juce::PopupMenu::backgroundColourId, colours::faceDark);
        setColour (juce::PopupMenu::textColourId, colours::legendLight);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::section.withAlpha (0.35f));
    }

    // Knob: skirt with a pointer line, coloured cap from the slider's "cap" property (ARGB).
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const auto cap = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("cap", (juce::int64) colours::capBlack.getARGB()));
        const bool onFace = s.getProperties().getWithDefault ("onFace", false);
        const auto tick = onFace ? colours::legendLight : colours::legendDark;

        // Scale ticks.
        g.setColour (tick.withAlpha (0.8f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
            g.drawLine ({ c.getPointOnCircumference (radius, a), c.getPointOnCircumference (radius - (i % 5 == 0 ? 5.0f : 3.0f), a) },
                        i % 5 == 0 ? 1.4f : 1.0f);
        }

        // Shadow and body.
        const float bodyR = radius - 7.0f;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (c.x - bodyR + 2.0f, c.y - bodyR + 3.0f, bodyR * 2.0f, bodyR * 2.0f);
        g.setGradientFill (juce::ColourGradient (cap.brighter (0.35f), c.x - bodyR * 0.6f, c.y - bodyR,
                                                 cap.darker (0.5f), c.x + bodyR * 0.6f, c.y + bodyR, false));
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

        // Knurled edge.
        g.setColour (cap.darker (0.7f).withAlpha (0.8f));
        for (int i = 0; i < 36; ++i)
        {
            const float a = (float) i / 36.0f * juce::MathConstants<float>::twoPi;
            g.drawLine ({ c.getPointOnCircumference (bodyR, a), c.getPointOnCircumference (bodyR - 2.5f, a) }, 1.0f);
        }

        // Top face.
        const float topR = bodyR * 0.72f;
        g.setGradientFill (juce::ColourGradient (cap.brighter (0.2f), c.x, c.y - topR, cap.darker (0.15f), c.x, c.y + topR, false));
        g.fillEllipse (c.x - topR, c.y - topR, topR * 2.0f, topR * 2.0f);

        // Pointer.
        const auto pointer = cap.getPerceivedBrightness() > 0.55f ? juce::Colour (0xff202020) : juce::Colours::white;
        g.setColour (pointer);
        g.drawLine ({ c.getPointOnCircumference (bodyR * 0.25f, angle), c.getPointOnCircumference (bodyR - 1.0f, angle) }, 2.4f);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (juce::FontOptions (10.5f));
        const bool onFace = s.getProperties().getWithDefault ("onFace", false);
        l->setColour (juce::Label::textColourId, onFace ? colours::legendLight : colours::legendDark);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        return l;
    }
};

// Bat-handle toggle switch with 2 or 3 positions, vertical (up = first position) or horizontal.
// Position 0 is up/left. Momentary mode springs back to the middle and calls onFlick (-1/+1).
class Lever : public juce::Component
{
public:
    Lever (int numPositions, bool horizontal, bool momentary = false)
        : positions (numPositions), isHorizontal (horizontal), isMomentary (momentary)
    {
        if (momentary) position = 1;
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void attach (juce::RangedAudioParameter& param, std::function<int (float)> toPosition,
                 std::function<float (int)> toValue)
    {
        valueToPosition = std::move (toPosition);
        positionToValue = std::move (toValue);
        attachment = std::make_unique<juce::ParameterAttachment> (param, [this] (float v)
        {
            position = valueToPosition (v);
            repaint();
        });
        attachment->sendInitialUpdate();
    }

    std::function<void (int)> onFlick;

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        const auto c = r.getCentre();

        // Nut and bushing.
        const float nutR = juce::jmin (r.getWidth(), r.getHeight()) * 0.26f;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (c.x - nutR + 1.5f, c.y - nutR + 2.0f, nutR * 2.0f, nutR * 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeef0f2), c.x, c.y - nutR, juce::Colour (0xff6d7177), c.x, c.y + nutR, false));
        g.fillEllipse (c.x - nutR, c.y - nutR, nutR * 2.0f, nutR * 2.0f);
        g.setColour (juce::Colour (0xff2a2c30));
        g.fillEllipse (c.x - nutR * 0.45f, c.y - nutR * 0.45f, nutR * 0.9f, nutR * 0.9f);

        // Handle: from the bushing towards the selected end.
        const float frac = positions == 1 ? 0.5f : (float) position / (float) (positions - 1); // 0 = up/left
        const float len = (isHorizontal ? r.getWidth() : r.getHeight()) * 0.46f;
        const float t = (frac - 0.5f) * 2.0f;  // -1 .. 1
        const juce::Point<float> tip = isHorizontal ? juce::Point<float> (c.x + t * len, c.y - 2.0f)
                                                    : juce::Point<float> (c.x + 1.5f, c.y + t * len);
        juce::Path handle;
        handle.startNewSubPath (c);
        handle.lineTo (tip);
        g.setColour (juce::Colour (0xff4d5157));
        g.strokePath (handle, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffd9dce0));
        g.strokePath (handle, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, tip.x - 3.0f, tip.y - 3.0f, juce::Colour (0xff80858c), tip.x + 3.0f, tip.y + 3.0f, false));
        g.fillEllipse (tip.x - 4.0f, tip.y - 4.0f, 8.0f, 8.0f);
    }

    // Sets the lever from outside (a click on one of its printed legends).
    void selectPosition (int p)
    {
        if (! isMomentary)
            setPosition (juce::jlimit (0, positions - 1, p));
    }

    int getNumPositions() const { return positions; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const float along = isHorizontal ? e.position.x / (float) getWidth() : e.position.y / (float) getHeight();
        if (isMomentary)
        {
            position = along < 0.5f ? 0 : 2;
            repaint();
            if (onFlick) onFlick (along < 0.5f ? -1 : 1);
            return;
        }
        dragStartPosition = position;
        dragged = false;
        if (positions == 2)
            setPosition (position == 0 ? 1 : 0);
        else
            // Three positions: a click anywhere in the upper (left) half moves one step up (left),
            // in the lower half one step down, so the small lever never needs an exact hit.
            setPosition (juce::jlimit (0, positions - 1, position + (along < 0.5f ? -1 : 1)));
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (isMomentary || positions < 3)
            return;
        // Dragging also works: one step every 12 px, starting from where the lever was.
        const float d = isHorizontal ? (float) e.getDistanceFromDragStartX() : (float) e.getDistanceFromDragStartY();
        if (std::abs (d) < 6.0f && ! dragged)
            return;
        dragged = true;
        const int target = juce::jlimit (0, positions - 1, dragStartPosition + juce::roundToInt (d / 12.0f));
        if (target != position)
            setPosition (target);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (isMomentary)
        {
            position = 1;
            repaint();
        }
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const float amount = std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : -w.deltaX;
        if (std::abs (amount) < 0.01f)
            return;
        const int step = amount > 0 ? -1 : 1; // wheel up = lever up
        if (isMomentary)
        {
            if (onFlick) onFlick (step);
            return;
        }
        const int target = juce::jlimit (0, positions - 1, position + step);
        if (target != position)
            setPosition (target);
    }

private:
    void setPosition (int p)
    {
        position = p;
        repaint();
        if (attachment != nullptr)
            attachment->setValueAsCompleteGesture (positionToValue (p));
    }

    int positions;
    bool isHorizontal, isMomentary;
    int position = 0;
    int dragStartPosition = 0;
    bool dragged = false;
    std::function<int (float)> valueToPosition;
    std::function<float (int)> positionToValue;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

} // namespace holdfast::ui
