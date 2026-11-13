#include "PluginEditor.h"

namespace
{
using namespace holdfast::ui::colours;
using holdfast::ui::drawLed;

constexpr int kFaceHeight = 206;

// Control sections on the face: x, width, legend under the section.
struct Section { int x, w; const char* legend; };
const Section kSections[] = {
    { 214, 172, "PROGRAM" },
    { 392, 278, "DELAY (ms)" },
    { 676, 222, "REGENERATION" },
    { 904, 268, "TIME BASE MODULATOR" },
};
constexpr int kSectionTop = 14, kSectionBottom = 192;

void legend (juce::Graphics& g, const juce::String& text, int x, int y, int w, juce::Colour c, float size = 10.0f,
             juce::Justification j = juce::Justification::centred)
{
    g.setColour (c);
    g.setFont (juce::FontOptions (size, juce::Font::bold));
    g.drawText (text, x, y, w, 12, j);
}
} // namespace

//==============================================================================
HoldfastEditor::HoldfastEditor (HoldfastProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    setupKnob (input, ParamIDs::input, "INPUT", capBlack, true);
    setupKnob (feedback, ParamIDs::feedback, "FEEDBACK", capWhite);
    setupKnob (delayFactor, ParamIDs::delayFactor, "DELAY FACTOR", capYellow);
    setupKnob (width, ParamIDs::width, "WIDTH", capRed);
    setupKnob (speed, ParamIDs::speed, "SPEED", capGrey);
    setupKnob (shape, ParamIDs::shape, "SHAPE", capGrey);
    setupKnob (mix, ParamIDs::mix, "MIX", capBlack, true);
    setupKnob (pedal, ParamIDs::pedal, "PEDAL", capGrey, true);
    setupKnob (spread, ParamIDs::spread, "SPREAD", capGrey, true);
    setupKnob (character, ParamIDs::character, "VINTAGE", capGrey, true);
    setupKnob (output, ParamIDs::output, "OUT LEVEL", capGrey, true);

    // Two-position levers: position 0 = up.
    auto upWhenOn = [] (float v) { return v > 0.5f ? 0 : 1; };
    auto onWhenUp = [] (int pos) { return pos == 0 ? 1.0f : 0.0f; };
    auto downWhenOn = [] (float v) { return v > 0.5f ? 1 : 0; };
    auto onWhenDown = [] (int pos) { return pos == 1 ? 1.0f : 0.0f; };

    setupLever (repeatLever, ParamIDs::repeat, upWhenOn, onWhenUp);           // ON up
    setupLever (programLever, ParamIDs::program, downWhenOn, onWhenDown);     // MANUAL up, AUTO down
    setupLever (scaleLever, ParamIDs::scale,                                  // 4X up, 2X, 1X down
                [] (float v) { return 2 - juce::roundToInt (v); }, [] (int pos) { return (float) (2 - pos); });
    setupLever (loLever, ParamIDs::loEq, downWhenOn, onWhenDown);             // FLAT up, CUT down
    setupLever (hiLever, ParamIDs::hiEq, downWhenOn, onWhenDown);
    setupLever (fbPhaseLever, ParamIDs::fbPhase, downWhenOn, onWhenDown);     // IN up, OUT down
    setupLever (delayPhaseLever, ParamIDs::delayPhase, downWhenOn, onWhenDown);
    setupLever (memoryLever, ParamIDs::memory, upWhenOn, onWhenUp);           // EXT up
    setupLever (pedalLever, ParamIDs::pedalOn, upWhenOn, onWhenUp);           // ON up

    // SELECT: momentary, left = shorter, right = longer, within the SCALE window.
    selectLever.onFlick = [this] (int dir)
    {
        if (getInt (ParamIDs::program) == 0)
            setParam (ParamIDs::select, juce::jlimit (0, holdfast::kWindow - 1, getInt (ParamIDs::select) + dir));
    };
    addAndMakeVisible (selectLever);

    stereoBox.addItemList ({ "Mono", "Linked Pair" }, 1);
    stereoBox.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (stereoBox);
    stereoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ParamIDs::stereoMode, stereoBox);

    for (auto* id : { ParamIDs::scale, ParamIDs::memory, ParamIDs::repeat })
        proc.apvts.addParameterListener (id, this);

    setSize (1320, 300);
    startTimerHz (30);
}

HoldfastEditor::~HoldfastEditor()
{
    for (auto* id : { ParamIDs::scale, ParamIDs::memory, ParamIDs::repeat })
        proc.apvts.removeParameterListener (id, this);
    setLookAndFeel (nullptr);
}

void HoldfastEditor::setupKnob (Knob& k, const char* paramId, const juce::String& text, juce::Colour cap, bool onFace)
{
    k.legend = text;
    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 14);
    k.slider.getProperties().set ("cap", (juce::int64) cap.getARGB());
    k.slider.getProperties().set ("onFace", onFace);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
    k.slider.lookAndFeelChanged();
}

void HoldfastEditor::setupLever (holdfast::ui::Lever& l, const char* paramId,
                                 std::function<int (float)> toPos, std::function<float (int)> toValue)
{
    addAndMakeVisible (l);
    if (auto* param = proc.apvts.getParameter (paramId))
        l.attach (*param, std::move (toPos), std::move (toValue));
}

void HoldfastEditor::setParam (const char* paramId, int value)
{
    if (auto* param = proc.apvts.getParameter (paramId))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 ((float) value));
        param->endChangeGesture();
    }
}

int HoldfastEditor::getInt (const char* paramId) const
{
    return juce::roundToInt (proc.apvts.getRawParameterValue (paramId)->load());
}

void HoldfastEditor::mouseDown (const juce::MouseEvent& e)
{
    // Clicking a lever's printed legend (e.g. "4X 512" above SCALE) sets the lever there.
    for (auto* l : { &repeatLever, &programLever, &scaleLever, &loLever, &hiLever, &fbPhaseLever,
                     &delayPhaseLever, &memoryLever, &pedalLever })
    {
        const auto b = l->getBounds().toFloat();
        const juce::Rectangle<float> top (b.getX() - 14.0f, b.getY() - 15.0f, b.getWidth() + 28.0f, 15.0f);
        const juce::Rectangle<float> bottom (b.getX() - 14.0f, b.getBottom(), b.getWidth() + 28.0f, 15.0f);
        if (top.contains (e.position)) { l->selectPosition (0); return; }
        if (bottom.contains (e.position)) { l->selectPosition (l->getNumPositions() - 1); return; }
    }
    {
        const auto sb = scaleLever.getBounds().toFloat();
        if (juce::Rectangle<float> (sb.getRight(), sb.getCentreY() - 8.0f, 40.0f, 16.0f).contains (e.position))
        {
            scaleLever.selectPosition (1); // "2X 256"
            return;
        }
    }

    // Clicking a tap LED inside the SCALE window selects it (a shortcut for the SELECT lever).
    if (getInt (ParamIDs::program) != 0)
        return;
    const int scale = getInt (ParamIDs::scale);
    for (int t = scale; t < scale + holdfast::kWindow; ++t)
        if (tapLed (t).expanded (5.0f, 10.0f).contains (e.position))
            setParam (ParamIDs::select, t - scale);
}

void HoldfastEditor::parameterChanged (const juce::String&, float)
{
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<HoldfastEditor> (this)]
    {
        if (safe != nullptr)
            safe->repaint();
    });
}

void HoldfastEditor::timerCallback()
{
    const float ms = proc.displayDelayMs.load();
    const float pk = proc.inputPeak.load();
    const float sl = proc.slewPeak.load();
    const int tap = proc.displayTap.load();
    if (tap != shownTap)
    {
        shownTap = tap;
        repaint (tapArea);
    }
    if (std::abs (ms - shownDelayMs) > 0.005f)
    {
        shownDelayMs = ms;
        repaint (displayArea);
    }
    if (std::abs (pk - shownPeak) > 0.005f || std::abs (sl - shownSlew) > 0.005f)
    {
        shownPeak = pk;
        shownSlew = sl;
        repaint (headroomArea.getUnion (activityArea));
    }
}

juce::Rectangle<float> HoldfastEditor::tapLed (int tap) const
{
    const float pitch = (float) tapArea.getWidth() / holdfast::kNumTaps;
    return { tapArea.getX() + (tap + 0.5f) * pitch - 4.5f, (float) tapArea.getY() + 30.0f, 9.0f, 9.0f };
}

void HoldfastEditor::drawKnobLegend (juce::Graphics& g, const Knob& k, bool onFace) const
{
    const auto b = k.slider.getBounds();
    legend (g, k.legend, b.getX() - 20, b.getY() - 13, b.getWidth() + 40, onFace ? legendLight : legendDark);
}

void HoldfastEditor::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();

    // Face and the lower strip with the plugin's extra controls.
    g.setGradientFill (juce::ColourGradient (face.brighter (0.12f), 0, 0, face.darker (0.15f), 0, (float) kFaceHeight, false));
    g.fillRect (r.withHeight ((float) kFaceHeight));
    g.setColour (faceDark);
    g.fillRect (r.withTrimmedTop ((float) kFaceHeight));
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawHorizontalLine (kFaceHeight, 0, r.getWidth());

    // Rack screws.
    for (float sx : { 14.0f, r.getWidth() - 14.0f })
        for (float sy : { 30.0f, (float) kFaceHeight - 30.0f })
        {
            g.setColour (juce::Colour (0xff15171a));
            g.fillEllipse (sx - 6.0f, sy - 6.0f, 12.0f, 12.0f);
            g.setColour (juce::Colour (0xff8a8f96));
            g.drawLine (sx - 4.0f, sy, sx + 4.0f, sy, 1.5f);
            g.drawLine (sx, sy - 4.0f, sx, sy + 4.0f, 1.5f);
        }

    // Light sections.
    for (const auto& s : kSections)
    {
        const juce::Rectangle<float> box ((float) s.x, (float) kSectionTop, (float) s.w, (float) (kSectionBottom - kSectionTop));
        g.setColour (section);
        g.fillRoundedRectangle (box, 8.0f);
        g.setColour (sectionEdge);
        g.drawRoundedRectangle (box.reduced (0.5f), 8.0f, 1.5f);
        legend (g, s.legend, s.x, kSectionBottom - 16, s.w, legendDark, 10.5f);
    }

    // INPUT: headroom ladder (PEAK and SLEW) and the input knob.
    {
        const auto h = headroomArea;
        const char* marks[4] = { "0", "10", "20", "40" };
        const float thresholds[4] = { 0.0f, -10.0f, -20.0f, -40.0f };
        const juce::Colour ladder[4] = { ledRed, ledYellow, ledGreen, ledGreen };
        const float peakDb = juce::Decibels::gainToDecibels (shownPeak, -90.0f);
        const float slewDb = juce::Decibels::gainToDecibels (shownSlew, -90.0f);
        legend (g, "dB", h.getX(), h.getY(), h.getWidth(), legendLight);
        for (int i = 0; i < 4; ++i)
        {
            const float y = (float) h.getY() + 16.0f + i * 22.0f;
            drawLed (g, { (float) h.getX() + 6.0f, y, 9.0f, 9.0f }, ladder[i], peakDb > thresholds[i] - 0.5f);
            drawLed (g, { (float) h.getRight() - 15.0f, y, 9.0f, 9.0f }, ladder[i], slewDb > thresholds[i] - 0.5f);
            legend (g, marks[i], h.getX() + 14, (int) y - 1, h.getWidth() - 28, legendLight, 9.5f);
        }
        legend (g, "PEAK  SLEW", h.getX() - 6, h.getY() + 104, h.getWidth() + 12, legendLight, 9.5f);
        legend (g, "HEADROOM", h.getX() - 6, h.getY() + 118, h.getWidth() + 12, legendLight, 9.5f);
        drawKnobLegend (g, input, true);
        legend (g, "0", input.slider.getX() - 4, input.slider.getBottom() - 26, 20, legendLight, 9.0f);
        legend (g, "MAX", input.slider.getRight() - 22, input.slider.getBottom() - 26, 30, legendLight, 9.0f);
    }

    // PROGRAM: lever legends.
    {
        auto lev = [&] (const juce::Component& c, const char* top, const char* bottom, const char* name)
        {
            const auto b = c.getBounds();
            legend (g, top, b.getX() - 14, b.getY() - 13, b.getWidth() + 28, legendDark, 9.5f);
            legend (g, bottom, b.getX() - 14, b.getBottom() + 1, b.getWidth() + 28, legendDark, 9.5f);
            legend (g, name, b.getX() - 18, b.getBottom() + 16, b.getWidth() + 36, legendDark, 10.0f);
        };
        lev (repeatLever, "ON", "OFF", "REPEAT");
        lev (programLever, "MANUAL", "AUTO", "DELAY");
        lev (scaleLever, "4X 512", "1X 128", "SCALE");
        const auto sb = scaleLever.getBounds();
        legend (g, "2X 256", sb.getRight() - 2, sb.getCentreY() - 6, 40, legendDark, 9.0f, juce::Justification::centredLeft);
        const auto rb = repeatLever.getBounds();
        drawLed (g, { (float) rb.getRight() + 4.0f, (float) rb.getCentreY() - 4.0f, 8.0f, 8.0f }, ledGreen, getInt (ParamIDs::repeat) != 0);
    }

    // DELAY: select lever, tap LEDs with group bars, readout.
    {
        const auto sb = selectLever.getBounds();
        legend (g, "SELECT", sb.getX() - 10, sb.getBottom() + 4, sb.getWidth() + 20, legendDark);
        legend (g, "<", sb.getX() - 12, sb.getCentreY() - 6, 10, legendDark, 11.0f);
        legend (g, ">", sb.getRight() + 2, sb.getCentreY() - 6, 10, legendDark, 11.0f);

        const auto ms = holdfast::tapMs (proc.isMemoryExpanded());
        const int scale = getInt (ParamIDs::scale);
        for (int t = 0; t < holdfast::kNumTaps; ++t)
        {
            const auto led = tapLed (t);
            const bool inWindow = t >= scale && t < scale + holdfast::kWindow;
            drawLed (g, led, t < 7 ? ledGreen : ledRed, t == shownTap);
            const double v = ms[(size_t) t];
            legend (g, v >= 1000.0 ? juce::String (v / 1000.0, 1) + "s" : juce::String ((int) v),
                    (int) led.getCentreX() - 14, (int) led.getBottom() + 4, 28,
                    inWindow ? legendDark : legendDark.withAlpha (0.4f), 9.0f);
        }
        // FLANGE / DOUBLE / ECHO bars above, the AUTO window below.
        auto bar = [&] (int first, int last, int lines, const char* name)
        {
            const float x0 = tapLed (first).getX() - 3.0f, x1 = tapLed (last).getRight() + 3.0f;
            const float y = (float) tapArea.getY() + 22.0f;
            g.setColour (legendDark);
            for (int i = 0; i < lines; ++i)
                g.drawHorizontalLine ((int) y - i * 3, x0, x1);
            legend (g, name, (int) x0, (int) y - 17, (int) (x1 - x0), legendDark, 9.0f);
        };
        bar (0, 3, 1, "FLANGE");
        bar (4, 6, 2, "DOUBLE");
        bar (7, 9, 3, "ECHO");
        const float ax0 = tapLed (scale).getX() - 3.0f, ax1 = tapLed (scale + holdfast::kWindow - 1).getRight() + 3.0f;
        const float ay = (float) tapArea.getY() + 58.0f;
        g.setColour (legendDark);
        g.drawHorizontalLine ((int) ay, ax0, ax1);
        legend (g, "AUTO", (int) ax0, (int) ay + 2, (int) (ax1 - ax0), legendDark, 9.0f);

        g.setColour (juce::Colour (0xff160806));
        g.fillRoundedRectangle (displayArea.toFloat(), 3.0f);
        g.setColour (legendDark);
        g.drawRoundedRectangle (displayArea.toFloat(), 3.0f, 1.5f);
        g.setColour (ledRed);
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 20.0f, juce::Font::bold));
        juce::String txt;
        if (shownDelayMs <= 0.0f)            txt = "--";
        else if (shownDelayMs < 10.0f)       txt = juce::String (shownDelayMs, 2);
        else if (shownDelayMs < 100.0f)      txt = juce::String (shownDelayMs, 1);
        else                                 txt = juce::String (juce::roundToInt (shownDelayMs));
        g.drawText (txt, displayArea.reduced (6, 0).withTrimmedRight (22), juce::Justification::centredRight);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("ms", displayArea.reduced (5, 0), juce::Justification::centredRight);
    }

    // REGENERATION.
    {
        auto lev = [&] (const juce::Component& c, const char* top, const char* bottom)
        {
            const auto b = c.getBounds();
            legend (g, top, b.getX() - 12, b.getY() - 13, b.getWidth() + 24, legendDark, 9.5f);
            legend (g, bottom, b.getX() - 12, b.getBottom() + 1, b.getWidth() + 24, legendDark, 9.5f);
        };
        lev (loLever, "FLAT", "CUT");
        lev (hiLever, "FLAT", "CUT");
        lev (fbPhaseLever, "IN", "OUT");
        legend (g, "LO - EQ - HI", loLever.getX() - 10, loLever.getBottom() + 16, hiLever.getRight() - loLever.getX() + 20, legendDark);
        legend (g, "PHASE", fbPhaseLever.getX() - 12, fbPhaseLever.getBottom() + 16, fbPhaseLever.getWidth() + 24, legendDark);
        drawKnobLegend (g, feedback, false);
    }

    // TIME BASE MODULATOR.
    for (auto* k : { &delayFactor, &width, &speed, &shape })
        drawKnobLegend (g, *k, false);
    legend (g, "(AUTO)", speed.slider.getX(), speed.slider.getBottom() + 1, speed.slider.getWidth(), legendDark, 9.0f);
    legend (g, "TRI SIN SQR", shape.slider.getX(), shape.slider.getBottom() + 1, shape.slider.getWidth(), legendDark, 9.0f);

    // OUTPUT (on the face).
    {
        const auto b = delayPhaseLever.getBounds();
        legend (g, "IN", b.getX() - 10, b.getY() - 13, b.getWidth() + 20, legendLight, 9.5f);
        legend (g, "OUT", b.getX() - 10, b.getBottom() + 1, b.getWidth() + 20, legendLight, 9.5f);
        legend (g, "DELAY", b.getX() - 14, b.getBottom() + 16, b.getWidth() + 28, legendLight, 9.5f);
        legend (g, "PHASE", b.getX() - 14, b.getBottom() + 28, b.getWidth() + 28, legendLight, 9.5f);
        drawKnobLegend (g, mix, true);
        legend (g, "OUTPUT", 1178, kSectionBottom - 16, 128, legendLight, 10.5f);

        const auto a = activityArea.toFloat();
        const bool active = shownPeak > 0.01f && getInt (ParamIDs::repeat) == 0;
        drawLed (g, { a.getX(), a.getY(), 8.0f, 8.0f }, ledOrange, active);
        legend (g, "ACTIVE", (int) a.getX() + 12, (int) a.getY() - 2, 50, legendLight, 9.0f, juce::Justification::centredLeft);
        drawLed (g, { a.getX() + 62.0f, a.getY(), 8.0f, 8.0f }, ledRed, getInt (ParamIDs::repeat) != 0);
        legend (g, "HOLD", (int) a.getX() + 74, (int) a.getY() - 2, 40, legendLight, 9.0f, juce::Justification::centredLeft);
    }

    // Lower strip: name and the plugin's extra controls.
    {
        const int y = kFaceHeight;
        g.setColour (legendLight);
        g.setFont (juce::FontOptions (26.0f, juce::Font::bold | juce::Font::italic));
        g.drawText ("HOLDFAST", 40, y + 22, 150, 30, juce::Justification::centredLeft);
        g.setColour (capYellow);
        g.setFont (juce::FontOptions (26.0f, juce::Font::italic));
        g.drawText ("512", 184, y + 22, 60, 30, juce::Justification::centredLeft);
        legend (g, "ADM DIGITAL DELAY", 40, y + 54, 200, legendLight.withAlpha (0.7f), 10.0f, juce::Justification::centredLeft);

        auto lev = [&] (const juce::Component& c, const char* top, const char* bottom, const char* name)
        {
            const auto b = c.getBounds();
            legend (g, top, b.getX() - 14, b.getY() - 13, b.getWidth() + 28, legendLight, 9.0f);
            legend (g, bottom, b.getX() - 14, b.getBottom() + 1, b.getWidth() + 28, legendLight, 9.0f);
            legend (g, name, b.getX() - 24, b.getBottom() + 14, b.getWidth() + 48, legendLight, 9.5f);
        };
        lev (memoryLever, "EXT", "INT", "MEMORY");
        lev (pedalLever, "ON", "OFF", "PEDAL");
        legend (g, "STEREO", stereoBox.getX(), stereoBox.getY() - 14, stereoBox.getWidth(), legendLight, 9.5f);
        legend (g, proc.isMemoryExpanded() ? "MEMORY 2.5 s" : "MEMORY 512 ms", 1120, y + 70, 170, legendLight.withAlpha (0.7f), 9.5f,
                juce::Justification::centredRight);
        for (auto* k : { &pedal, &spread, &character, &output })
            drawKnobLegend (g, *k, true);
    }
}

void HoldfastEditor::resized()
{
    // Input.
    headroomArea = { 36, 30, 66, 132 };
    input.slider.setBounds (106, 44, 100, 118);

    // Program.
    repeatLever.setBounds (228, 46, 26, 40);
    programLever.setBounds (280, 46, 26, 40);
    scaleLever.setBounds (318, 46, 26, 40);

    // Delay.
    selectLever.setBounds (406, 64, 44, 26);
    displayArea = { 404, 126, 64, 26 };
    tapArea = { 468, 34, 196, 100 };

    // Regeneration.
    loLever.setBounds (690, 52, 26, 40);
    hiLever.setBounds (732, 52, 26, 40);
    feedback.slider.setBounds (770, 40, 76, 100);
    fbPhaseLever.setBounds (856, 52, 26, 40);

    // Time base modulator.
    delayFactor.slider.setBounds (912, 40, 66, 100);
    width.slider.setBounds (978, 40, 66, 100);
    speed.slider.setBounds (1044, 40, 64, 100);
    shape.slider.setBounds (1108, 40, 62, 100);

    // Output.
    delayPhaseLever.setBounds (1190, 54, 24, 38);
    mix.slider.setBounds (1224, 34, 82, 104);
    activityArea = { 1190, 146, 110, 12 };

    // Lower strip.
    const int y = kFaceHeight;
    memoryLever.setBounds (300, y + 26, 24, 36);
    pedalLever.setBounds (380, y + 26, 24, 36);
    pedal.slider.setBounds (430, y + 18, 60, 74);
    stereoBox.setBounds (530, y + 38, 120, 24);
    spread.slider.setBounds (670, y + 18, 60, 74);
    character.slider.setBounds (770, y + 18, 60, 74);
    output.slider.setBounds (870, y + 18, 60, 74);
}
