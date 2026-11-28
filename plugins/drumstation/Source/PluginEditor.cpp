#include "PluginEditor.h"

namespace
{
const juce::Colour panelBlack (0xff111111);
const juce::Colour earBlack (0xff0a0a0a);
const juce::Colour yellow (0xffe9c414);
const juce::Colour stripGrey (0xffb4b4b4);
const juce::Colour keyGrey (0xffd9d9d9);
const juce::Colour keyText (0xff3c3c3c);
const juce::Colour ledRed (0xffff2a1a);
const juce::Colour segOn (0xffff2a12);
const juce::Colour segOff (0xff3a0906);
const juce::Colour setupBg (0xff1c1c1c);
const juce::Colour setupText (0xffd8d8d8);

constexpr float topKnobY = 50.0f, bottomKnobY = 131.0f;
constexpr float knobSize = 62.0f, smallKnobSize = 44.0f;
constexpr float midStripY = 84.0f, bottomStripY = 166.0f, stripH = 13.0f;

const char* sectionTitles[ds::numSections] = {
    "BASS DRUM", "SNARE DRUM", "TOM TOMS", "R'SHOT  H'CLAP  C'BELL", "HI HAT", "CYMBALS", "C'NGAS  M'CAS  CLAVES"
};

const char* keyCaps[12] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "-", "+" };
const char* keyLabels[12] = { "TRIGGER\nSTART STOP", "DRUM SELECT", "FRONT CUT", "CONT / VELOCITY",
                              "NOTE-OFF REC", "GEN MIDI SET", "OUTPUT SET",
                              "ASSIGN BANK", "DISTORTION", "DEMO", "SAVE TYPE\nCOMPARE", "SAVE\nWRITE" };
// Keypad grid: row by row, as on the panel.
const int keyGrid[4][3] = { { 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 }, { 10, 0, 11 } };
juce::Point<float> keyCentre (int row, int col) { return { 299.0f + (float) col * 52.5f, 32.0f + (float) row * 34.0f }; }

const char* modeNames[] = { "PROGRAM", "MIDI RX CH", "MIDI TX CH", "UTILITY", "SAVE" };

// Seven-segment patterns, bits a..g.
const int segmentMasks[12] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f, 0x40, 0x00 };
constexpr int segDash = 10, segBlank = 11;
} // namespace

//==============================================================================
PanelLookAndFeel::PanelLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2a2a));
    setColour (juce::ComboBox::textColourId, setupText);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff4a4a4a));
    setColour (juce::ComboBox::arrowColourId, yellow);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff222222));
    setColour (juce::PopupMenu::textColourId, setupText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, yellow.withAlpha (0.35f));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff202020));
    setColour (juce::TooltipWindow::textColourId, setupText);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xff202020));
    setColour (juce::BubbleComponent::outlineColourId, yellow);
    setColour (juce::TooltipWindow::outlineColourId, yellow);
}

void PanelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const auto c = bounds.getCentre();
    const float outer = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float capR = outer * 0.62f;
    const float alpha = slider.isEnabled() ? 1.0f : 0.3f;

    // Dotted scale, as printed around the knobs.
    g.setColour (juce::Colours::white.withAlpha (0.9f * alpha));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const float d = (i == 0 || i == 5 || i == 10) ? 2.6f : 1.8f;
        const auto p = c.getPointOnCircumference (outer * 0.86f, a);
        g.fillEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d);
    }

    // White cap with a black pointer line.
    g.setColour (juce::Colours::black.withAlpha (0.6f * alpha));
    g.fillEllipse (c.x - capR + 1.0f, c.y - capR + 2.0f, capR * 2.0f, capR * 2.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffafafa).withAlpha (alpha), c.x - capR * 0.5f, c.y - capR,
                                             juce::Colour (0xffbdbdbd).withAlpha (alpha), c.x + capR * 0.5f, c.y + capR, false));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colour (0xff6a6a6a).withAlpha (alpha));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);

    const float angle = startAngle + pos * (endAngle - startAngle);
    g.setColour (juce::Colour (0xff141414).withAlpha (alpha));
    g.drawLine ({ c.getPointOnCircumference (capR * 0.2f, angle), c.getPointOnCircumference (capR * 0.95f, angle) }, 2.6f);
}

void PanelLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float alpha = b.isEnabled() ? 1.0f : 0.4f;

    if (b.getProperties()["setup"])
    {
        g.setColour (juce::Colour (down ? 0xff141414 : 0xff2e2e2e).brighter (highlighted ? 0.12f : 0.0f).withAlpha (alpha));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colour (0xff505050).withAlpha (alpha));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        if (b.getClickingTogglesState())
        {
            const auto led = juce::Rectangle<float> (r.getX() + 7.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
            g.setColour (b.getToggleState() ? ledRed : juce::Colour (0xff3d1512));
            g.fillEllipse (led);
        }
        return;
    }

    // Light grey rubber key with a slight shadow.
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 2.5f);
    g.setColour ((down ? keyGrey.darker (0.25f) : keyGrey.brighter (highlighted ? 0.08f : 0.0f)).withAlpha (alpha));
    g.fillRoundedRectangle (r, 2.5f);
}

void PanelLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const float alpha = b.isEnabled() ? 1.0f : 0.4f;
    if (b.getProperties()["setup"])
    {
        g.setColour (setupText.withAlpha (alpha));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        auto area = b.getLocalBounds();
        if (b.getClickingTogglesState())
            area.removeFromLeft (14);
        g.drawText (b.getButtonText(), area, juce::Justification::centred);
        return;
    }
    g.setColour (keyText.withAlpha (alpha));
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

//==============================================================================
void SlideSwitch::setPosition (int p, bool notify)
{
    p = juce::jlimit (0, positions - 1, p);
    if (p == position)
        return;
    position = p;
    repaint();
    if (notify && onChange)
        onChange (position);
}

int SlideSwitch::positionAt (float x) const
{
    return juce::jlimit (0, positions - 1, (int) (x / (float) getWidth() * (float) positions));
}

void SlideSwitch::mouseDown (const juce::MouseEvent& e) { setPosition (positionAt ((float) e.x), true); }
void SlideSwitch::mouseDrag (const juce::MouseEvent& e) { setPosition (positionAt ((float) e.x), true); }

void SlideSwitch::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff050505));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colour (0xff8a8a8a));
    g.drawRoundedRectangle (r, 3.0f, 1.2f);

    // Ridged white slider cap.
    const float slot = (r.getWidth() - 4.0f) / (float) positions;
    auto cap = juce::Rectangle<float> (r.getX() + 2.0f + slot * (float) position, r.getY() + 2.0f,
                                       slot, r.getHeight() - 4.0f).reduced (1.0f, 0.0f);
    g.setColour (juce::Colour (0xffe8e8e8));
    g.fillRoundedRectangle (cap, 1.5f);
    g.setColour (juce::Colour (0xff7a7a7a));
    for (float x = cap.getX() + 3.0f; x < cap.getRight() - 2.0f; x += 3.0f)
        g.drawVerticalLine ((int) x, cap.getY() + 2.0f, cap.getBottom() - 2.0f);
}

//==============================================================================
DrumstationPanel::DrumstationPanel (DrumstationProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);
    auto& state = proc.apvts.state;

    setupKnob (volume);
    volume.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamIDs::volume, volume.slider);
    volume.slider.setTooltip ("Master volume (CC 7)");

    for (auto* b : { &kit808, &kit909 })
    {
        addAndMakeVisible (*b);
        b->setButtonText (b == &kit808 ? "808" : "909");
        b->onClick = [this, b] { setKit (b == &kit808 ? ds::kit808 : ds::kit909); };
    }

    for (int k = 0; k < 12; ++k)
    {
        addAndMakeVisible (keys[k]);
        keys[k].setButtonText (keyCaps[k]);
        keys[k].onClick = [this, k] { keypadPressed (k); };
    }

    addAndMakeVisible (auditionButton);
    auditionButton.setButtonText (juce::String::fromUTF8 ("\xe2\x99\xaa"));
    auditionButton.setTooltip ("Audition: plays the selected sound on the DrumStation");
    auditionButton.onClick = [this] { proc.requestAudition ((int) proc.apvts.state.getProperty (StateIDs::editVoice, -1)); };

    addAndMakeVisible (modeButton);
    modeButton.setButtonText ("SELECT");
    modeButton.setTooltip ("Mode: program, MIDI receive channel, MIDI transmit channel");
    modeButton.onClick = [this] { mode = (mode + 1) % numModes; firstDigit = -1; repaint(); };

    for (int s = 0; s < ds::numSections; ++s)
    {
        auto& sec = sections[s];
        for (auto* k : { &sec.tune, &sec.level, &sec.b1, &sec.b2 })
        {
            setupKnob (*k);
            k->slider.onDragStart = [this, s] { setEditVoice (voiceForSection (s)); };
        }
        sec.level.slider.setEnabled (false);
        sec.level.slider.setValue (100.0, juce::dontSendNotification);
        sec.level.slider.setTooltip ("Level: not a MIDI controller on the DrumStation (SysEx, coming with phase 2)");

        auto makeSwitch = [this, s] (int n, const juce::Identifier& prop)
        {
            auto sw = std::make_unique<SlideSwitch> (n);
            addAndMakeVisible (*sw);
            sw->setPosition ((int) proc.apvts.state.getProperty (prop, 0), false);
            sw->onChange = [this, s, prop] (int pos)
            {
                proc.apvts.state.setProperty (prop, pos, nullptr);
                rebindAll();
                const int v = voiceForSection (s);
                if (v >= 0)
                    setEditVoice (v);
            };
            return sw;
        };

        const juce::Identifier selectProp (StateIDs::selectPrefix + juce::String (s));
        if (s == ds::secToms || s == ds::secRimClapBell || s == ds::secCongas)
            sec.select = makeSwitch (3, selectProp);
        else if (s == ds::secHiHat || s == ds::secCymbals)
            sec.select = makeSwitch (2, selectProp);
        if (s == ds::secCongas)
            sec.select2 = makeSwitch (3, StateIDs::congaSelect);
    }

    for (auto* k : { &frontCut, &pan, &distortion })
        setupKnob (*k, true);

    addAndMakeVisible (gmSwitch);
    gmSwitch.setPosition ((int) state.getProperty (StateIDs::gmSet, 0), false);
    gmSwitch.setTooltip ("Note map used by Audition: match the DrumStation's GEN MIDI SET (808 or 909)");
    gmSwitch.onChange = [this] (int pos) { proc.apvts.state.setProperty (StateIDs::gmSet, pos, nullptr); };

    // MIDI setup strip.
    for (auto* box : { &outBox, &inBox, &rxBox, &txBox })
        addAndMakeVisible (*box);
    for (int ch = 1; ch <= 16; ++ch)
    {
        rxBox.addItem (juce::String (ch), ch);
        txBox.addItem (juce::String (ch), ch);
    }
    rxBox.setSelectedId (proc.getRxChannel(), juce::dontSendNotification);
    txBox.setSelectedId (proc.getTxChannel(), juce::dontSendNotification);
    rxBox.onChange = [this] { proc.setRxChannel (rxBox.getSelectedId()); repaint(); };
    txBox.onChange = [this] { proc.setTxChannel (txBox.getSelectedId()); repaint(); };
    rxBox.setTooltip ("Channel the DrumStation receives on (MIDI RX CH on the unit)");
    txBox.setTooltip ("Channel the DrumStation transmits on (MIDI TX CH on the unit)");

    outBox.onChange = [this]
    {
        const int i = outBox.getSelectedId() - 2;
        if (outBox.getSelectedId() == 1 || juce::isPositiveAndBelow (i, outIds.size()))
            proc.setOutputPort (outBox.getSelectedId() == 1 ? juce::String() : outIds[i]);
        repaint();
    };
    inBox.onChange = [this]
    {
        const int i = inBox.getSelectedId() - 2;
        if (inBox.getSelectedId() == 1 || juce::isPositiveAndBelow (i, inIds.size()))
            proc.setInputPort (inBox.getSelectedId() == 1 ? juce::String() : inIds[i]);
        repaint();
    };
    refreshPortLists();

    for (auto* b : { &sendButton, &syncButton, &recallButton, &saveDumpButton })
    {
        addAndMakeVisible (*b);
        b->getProperties().set ("setup", true);
    }
    sendButton.setButtonText ("SEND TO UNIT");
    sendButton.setTooltip ("Sends program, volume and every sound controller to the DrumStation");
    sendButton.onClick = [this] { proc.requestSendAll(); };

    recallButton.setButtonText ("RECALL ON LOAD");
    recallButton.setClickingTogglesState (true);
    recallButton.setToggleState ((bool) state.getProperty (StateIDs::recallOnLoad, true), juce::dontSendNotification);
    recallButton.setTooltip ("When the project opens, send the stored sound to the DrumStation (total recall)");
    recallButton.onClick = [this] { proc.apvts.state.setProperty (StateIDs::recallOnLoad, recallButton.getToggleState(), nullptr); };

    syncButton.setButtonText ("SYNC FROM UNIT");
    syncButton.setEnabled (false);
    syncButton.setTooltip ("Reads the patch from the DrumStation (press SAVE on the unit): coming with phase 2, "
                           "once the dump format is decoded");

    saveDumpButton.setButtonText ("SAVE DUMP .SYX");
    saveDumpButton.setTooltip ("Saves the SysEx dumps received from the DrumStation to a .syx file");
    saveDumpButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Save DrumStation dump",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("drumstation-dump.syx"), "*.syx");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto file = fc.getResult();
                                      if (file == juce::File())
                                          return;
                                      const auto data = proc.takeReceivedSysex();
                                      file.replaceWithData (data.getData(), data.getSize());
                                      repaint();
                                  });
    };

    rebindAll();
    setEditVoice ((int) state.getProperty (StateIDs::editVoice, 0));
    startTimerHz (20);
}

DrumstationPanel::~DrumstationPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DrumstationPanel::setupKnob (Knob& k, bool)
{
    addAndMakeVisible (k.slider);
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    k.slider.setPopupDisplayEnabled (true, true, this);
    k.slider.setRange (0.0, 127.0, 1.0);
    k.slider.setDoubleClickReturnValue (true, 64.0);
}

void DrumstationPanel::bind (Knob& k, int voice, ds::Control c, bool bipolarScale)
{
    k.attachment.reset();
    const bool exists = voice >= 0 && ds::controllerFor (ds::voices[voice], c) >= 0;
    k.slider.setEnabled (exists);
    k.scaleLeft = bipolarScale ? "-5" : "0";
    k.scaleRight = bipolarScale ? "+5" : "10";
    if (exists)
    {
        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            proc.apvts, DrumstationProcessor::paramId (voice, c), k.slider);
        const auto& v = ds::voices[voice];
        k.slider.setTooltip (juce::String (v.name) + " " + ds::controlName (v, c)
                             + " (CC " + juce::String (ds::controllerFor (v, c)) + ")");
        const bool startsAtZero = c == ds::ctlFrontCut || c == ds::ctlDistortion;
        k.slider.setDoubleClickReturnValue (true, startsAtZero ? 0.0 : 64.0);
    }
    else
    {
        k.slider.setRange (0.0, 127.0, 1.0);
        k.slider.setValue (0.0, juce::dontSendNotification);
        k.slider.setTooltip ({});
    }
}

int DrumstationPanel::voiceForSection (int s) const
{
    const auto& state = proc.apvts.state;
    const auto kit = (ds::Kit) (int) state.getProperty (StateIDs::kit, 1);
    int sel = (int) state.getProperty (StateIDs::selectPrefix + juce::String (s), 0);
    if (s == ds::secCongas)
        sel = sel == 0 ? (int) state.getProperty (StateIDs::congaSelect, 0) : sel + 2;
    return ds::findVoice (kit, (ds::Section) s, sel);
}

void DrumstationPanel::rebindAll()
{
    for (int s = 0; s < ds::numSections; ++s)
    {
        auto& sec = sections[s];
        sec.voice = voiceForSection (s);
        const bool tuneIsTone = sec.voice >= 0 && juce::String (ds::voices[sec.voice].tuneName) == "Tone";
        bind (sec.tune, sec.voice, ds::ctlTune, ! tuneIsTone);
        bind (sec.b1, sec.voice, ds::ctlB1, false);
        bind (sec.b2, sec.voice, ds::ctlB2, false);
        sec.level.slider.setEnabled (false);
    }
    const int kit = (int) proc.apvts.state.getProperty (StateIDs::kit, 1);
    kit808.setToggleState (kit == ds::kit808, juce::dontSendNotification);
    kit909.setToggleState (kit == ds::kit909, juce::dontSendNotification);
    repaint();
}

void DrumstationPanel::setKit (int kit)
{
    const int previous = (int) proc.apvts.state.getProperty (StateIDs::editVoice, -1);
    proc.apvts.state.setProperty (StateIDs::kit, kit, nullptr);
    rebindAll();

    // Keep editing the same section if the new kit has that sound, else its bass drum.
    int v = previous >= 0 ? voiceForSection (ds::voices[previous].section) : -1;
    if (v < 0)
        v = ds::findVoice ((ds::Kit) kit, ds::secBass, 0);
    setEditVoice (v);
}

void DrumstationPanel::setEditVoice (int voice)
{
    if (! juce::isPositiveAndBelow (voice, ds::numVoices))
        return;
    proc.apvts.state.setProperty (StateIDs::editVoice, voice, nullptr);
    bind (frontCut, voice, ds::ctlFrontCut, false);
    bind (pan, voice, ds::ctlPan, false);
    bind (distortion, voice, ds::ctlDistortion, false);
    pan.scaleLeft = "L";
    pan.scaleRight = "R";
    repaint();
}

int DrumstationPanel::modeValue() const
{
    switch (mode)
    {
        case modeProgram: return juce::roundToInt (proc.apvts.getRawParameterValue (ParamIDs::program)->load());
        case modeRx:      return proc.getRxChannel();
        case modeTx:      return proc.getTxChannel();
        default:          return -1;
    }
}

void DrumstationPanel::setModeValue (int v)
{
    if (mode == modeProgram)
    {
        if (auto* p = proc.apvts.getParameter (ParamIDs::program))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (0, ds::numPrograms - 1, v)));
            p->endChangeGesture();
        }
    }
    else if (mode == modeRx)
    {
        proc.setRxChannel (v);
        rxBox.setSelectedId (proc.getRxChannel(), juce::dontSendNotification);
    }
    else if (mode == modeTx)
    {
        proc.setTxChannel (v);
        txBox.setSelectedId (proc.getTxChannel(), juce::dontSendNotification);
    }
    repaint();
}

void DrumstationPanel::keypadPressed (int key)
{
    const int value = modeValue();
    if (value < 0)
        return;   // UTILITY and SAVE: phase 2

    if (key == 10 || key == 11)
    {
        firstDigit = -1;
        setModeValue (value + (key == 11 ? 1 : -1));
        return;
    }

    // Two-digit entry, as on the unit.
    if (firstDigit < 0)
    {
        firstDigit = key;
        repaint();
        return;
    }
    const int entered = firstDigit * 10 + key;
    firstDigit = -1;
    const bool valid = mode == modeProgram ? entered < ds::numPrograms : (entered >= 1 && entered <= 16);
    if (valid)
        setModeValue (entered);
    repaint();
}

void DrumstationPanel::refreshPortLists()
{
    auto fill = [] (juce::ComboBox& box, juce::StringArray& ids, const juce::Array<juce::MidiDeviceInfo>& devices,
                    const juce::String& currentId, const juce::String& savedName)
    {
        box.clear (juce::dontSendNotification);
        ids.clear();
        box.addItem ("(none)", 1);
        int selected = 1;
        for (const auto& d : devices)
        {
            ids.add (d.identifier);
            box.addItem (d.name, ids.size() + 1);
            if (d.identifier == currentId)
                selected = ids.size() + 1;
        }
        if (selected == 1 && savedName.isNotEmpty())
        {
            box.addItem (savedName + " (not connected)", 1000);
            selected = 1000;
        }
        box.setSelectedId (selected, juce::dontSendNotification);
    };

    const auto& s = proc.apvts.state;
    fill (outBox, outIds, juce::MidiOutput::getAvailableDevices(),
          proc.isOutputOpen() ? s.getProperty (StateIDs::outPort).toString() : juce::String(), s.getProperty (StateIDs::outPortName).toString());
    fill (inBox, inIds, juce::MidiInput::getAvailableDevices(),
          proc.isInputOpen() ? s.getProperty (StateIDs::inPort).toString() : juce::String(), s.getProperty (StateIDs::inPortName).toString());
}

void DrumstationPanel::timerCallback()
{
    if (proc.midiInActivity.exchange (false)) inLedHold = 3;
    if (proc.midiOutActivity.exchange (false)) outLedHold = 3;
    if (inLedHold > 0) --inLedHold;
    if (outLedHold > 0) --outLedHold;

    // Pick up MIDI devices that were plugged in or removed.
    if (--portRefreshCountdown <= 0)
    {
        portRefreshCountdown = 40;
        if (! outBox.isPopupActive() && ! inBox.isPopupActive()
            && (juce::MidiOutput::getAvailableDevices().size() != outIds.size()
                || juce::MidiInput::getAvailableDevices().size() != inIds.size()))
            refreshPortLists();
    }

    rxBox.setSelectedId (proc.getRxChannel(), juce::dontSendNotification);
    txBox.setSelectedId (proc.getTxChannel(), juce::dontSendNotification);
    saveDumpButton.setEnabled (proc.sysexReceived.load() > 0);
    repaint();
}

//==============================================================================
void DrumstationPanel::drawStrip (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, const juce::String& text) const
{
    g.setColour (c);
    g.fillRect (r);
    if (text.isNotEmpty())
    {
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (text, r, juce::Justification::centred);
    }
}

void DrumstationPanel::drawLabel (juce::Graphics& g, juce::Point<float> c, const juce::String& text, float size,
                                  juce::Colour colour, float width) const
{
    g.setColour (colour);
    g.setFont (juce::FontOptions (size, juce::Font::bold));
    const int lines = juce::StringArray::fromLines (text).size();
    const float h = size * 1.15f * (float) lines;
    g.drawFittedText (text, juce::Rectangle<float> (c.x - width * 0.5f, c.y - h * 0.5f, width, h).toNearestInt(),
                      juce::Justification::centred, lines, 0.7f);
}

void DrumstationPanel::drawKnobScale (juce::Graphics& g, const Knob& k) const
{
    const auto b = k.slider.getBounds().toFloat();
    const auto c = b.getCentre();
    const float r = b.getWidth() * 0.5f;
    const float alpha = k.slider.isEnabled() ? 1.0f : 0.35f;
    drawLabel (g, c + juce::Point<float> (-r * 0.78f, r * 0.82f), k.scaleLeft, 8.5f, juce::Colours::white.withAlpha (alpha), 24.0f);
    drawLabel (g, c + juce::Point<float> (r * 0.78f, r * 0.82f), k.scaleRight, 8.5f, juce::Colours::white.withAlpha (alpha), 24.0f);
}

void DrumstationPanel::drawLed (juce::Graphics& g, juce::Point<float> c, bool on, juce::Colour colour) const
{
    if (on)
    {
        g.setColour (colour.withAlpha (0.3f));
        g.fillEllipse (c.x - 6.5f, c.y - 6.5f, 13.0f, 13.0f);
    }
    g.setColour (on ? colour : colour.darker (2.5f));
    g.fillEllipse (c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f);
}

void DrumstationPanel::drawSevenSegment (juce::Graphics& g, juce::Rectangle<float> r, int digit, bool dot) const
{
    const int mask = segmentMasks[juce::jlimit (0, 11, digit)];
    const float t = r.getWidth() * 0.16f;          // segment thickness
    const float w = r.getWidth() - t * 1.2f, h = (r.getHeight() - t) * 0.5f;
    const float x = r.getX(), y = r.getY();
    const float skew = 0.08f * r.getHeight();      // slight italic, as on LED displays

    auto seg = [&] (int bit, float x1, float y1, float x2, float y2)
    {
        juce::Path p;
        p.addRoundedRectangle (juce::jmin (x1, x2) - t * 0.5f, juce::jmin (y1, y2) - t * 0.5f,
                               std::abs (x2 - x1) + t, std::abs (y2 - y1) + t, t * 0.45f);
        p.applyTransform (juce::AffineTransform::shear (-skew / r.getHeight(), 0.0f).translated (skew, 0.0f));
        g.setColour ((mask >> bit) & 1 ? segOn : segOff);
        g.fillPath (p);
    };
    const float l = x + t, rr = x + t + w - t * 0.6f;
    const float top = y + t * 0.5f, mid = y + t * 0.5f + h, bot = y + t * 0.5f + 2.0f * h;
    const float in = t * 0.75f;
    seg (0, l + in, top, rr - in, top);         // a
    seg (1, rr, top + in, rr, mid - in);        // b
    seg (2, rr, mid + in, rr, bot - in);        // c
    seg (3, l + in, bot, rr - in, bot);         // d
    seg (4, l, mid + in, l, bot - in);          // e
    seg (5, l, top + in, l, mid - in);          // f
    seg (6, l + in, mid, rr - in, mid);         // g

    g.setColour (dot ? segOn : segOff);
    g.fillEllipse (rr + t * 0.9f, bot - t * 0.5f, t, t);
}

void DrumstationPanel::paint (juce::Graphics& g)
{
    using R = juce::Rectangle<float>;
    const float W = (float) designWidth;
    const auto& state = proc.apvts.state;

    // Rack body and ears.
    g.setColour (panelBlack);
    g.fillRect (R (0.0f, 0.0f, W, (float) panelHeight));
    g.setColour (earBlack);
    g.fillRect (R (0.0f, 0.0f, 64.0f, (float) panelHeight));
    g.fillRect (R (W - 64.0f, 0.0f, 64.0f, (float) panelHeight));
    g.setColour (juce::Colours::white);
    for (float x : { 26.0f, W - 26.0f })
        for (float y : { 22.0f, (float) panelHeight - 22.0f })
            g.fillRoundedRectangle (R (x - 15.0f, y - 7.0f, 30.0f, 14.0f), 7.0f);

    // Left block: master volume, kit, data entry.
    drawStrip (g, R (80.0f, 4.0f, 511.0f, stripH), yellow, {});
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("DRUMSTATION  EDITOR", R (100.0f, 4.0f, 300.0f, stripH), juce::Justification::centredLeft);
    drawStrip (g, R (80.0f, midStripY, 190.0f, stripH), stripGrey, {});
    drawLabel (g, { 147.0f, midStripY + 6.5f }, "VOLUME", 10.0f);
    drawLabel (g, { 221.0f, midStripY + 6.5f }, "DRUM KIT", 10.0f);
    drawKnobScale (g, volume);

    const int kit = (int) state.getProperty (StateIDs::kit, 1);
    drawLed (g, { 255.0f, 32.0f }, kit == ds::kit808);
    drawLed (g, { 255.0f, 66.0f }, kit == ds::kit909);

    // Headphone socket and write switch (decorative: writing comes with phase 2).
    g.setColour (juce::Colour (0xff2a2a2a));
    g.fillEllipse (133.0f, 117.0f, 28.0f, 28.0f);
    g.setColour (juce::Colour (0xffcfcfcf));
    g.drawEllipse (133.0f, 117.0f, 28.0f, 28.0f, 3.0f);
    g.setColour (juce::Colours::black);
    g.fillEllipse (141.0f, 125.0f, 12.0f, 12.0f);
    {
        auto sw = R (203.0f, 124.0f, 36.0f, 16.0f);
        g.setColour (juce::Colour (0xff050505));
        g.fillRoundedRectangle (sw, 3.0f);
        g.setColour (juce::Colour (0xff8a8a8a));
        g.drawRoundedRectangle (sw, 3.0f, 1.2f);
        g.setColour (juce::Colour (0xffe8e8e8));
        g.fillRoundedRectangle (sw.withWidth (17.0f).translated (2.0f, 2.0f).withHeight (12.0f), 1.5f);
    }
    drawLed (g, { 255.0f, 108.0f }, false);
    drawLabel (g, { 221.0f, 150.0f }, "PROTECT   ENABLE", 7.5f);

    // Keypad captions.
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 3; ++col)
        {
            const int k = keyGrid[row][col];
            const auto c = keyCentre (row, col);
            drawLabel (g, { c.x, c.y + 17.0f }, keyLabels[k], 7.0f, juce::Colours::white, 56.0f);
        }

    // Display: two digits and the mode LEDs.
    {
        const auto box = R (437.0f, 22.0f, 150.0f, 96.0f);
        g.setColour (juce::Colour (0xff050505));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (juce::Colour (0xffcfcfcf));
        g.drawRoundedRectangle (box, 4.0f, 2.0f);

        const int value = modeValue();
        int d1 = segDash, d2 = segDash;
        if (firstDigit >= 0)            { d1 = firstDigit; d2 = segBlank; }
        else if (value >= 0)            { d1 = (value / 10) % 10; d2 = value % 10; }
        drawSevenSegment (g, R (447.0f, 32.0f, 30.0f, 52.0f), d1, inLedHold > 0);
        drawSevenSegment (g, R (482.0f, 32.0f, 30.0f, 52.0f), d2, outLedHold > 0);

        for (int m = 0; m < numModes; ++m)
        {
            const float y = 34.0f + (float) m * 14.0f;
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (7.0f, juce::Font::bold));
            g.drawText (modeNames[m], R (520.0f, y - 5.0f, 52.0f, 10.0f), juce::Justification::centredLeft);
            drawLed (g, { 577.0f, y }, mode == m, m == modeProgram ? juce::Colour (0xffffa020) : ledRed);
        }
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (7.0f, juce::Font::bold));
        g.drawText ("IN .   OUT .", R (447.0f, 92.0f, 70.0f, 10.0f), juce::Justification::centredLeft);
    }
    drawLabel (g, { 475.0f, 152.0f }, "AUDITION", 7.5f);

    drawStrip (g, R (80.0f, bottomStripY, 511.0f, stripH), stripGrey, {});
    drawLabel (g, { 147.0f, bottomStripY + 6.5f }, "HEADPHONES", 10.0f);
    drawLabel (g, { 221.0f, bottomStripY + 6.5f }, "WRITE", 10.0f);
    drawLabel (g, { 351.0f, bottomStripY + 6.5f }, "DATA ENTRY", 10.0f);
    drawLabel (g, { 475.0f, bottomStripY + 6.5f }, "AUTO TRIGGER", 10.0f);
    drawLabel (g, { 548.0f, bottomStripY + 6.5f }, "MODE", 10.0f);

    // Sound sections.
    const int editVoice = (int) state.getProperty (StateIDs::editVoice, -1);
    for (int s = 0; s < ds::numSections; ++s)
    {
        const auto& sec = sections[s];
        const float x0 = sectionX (s), L = x0 + 37.0f, Rx = x0 + 120.0f;
        const bool editing = juce::isPositiveAndBelow (editVoice, ds::numVoices) && ds::voices[editVoice].section == s;

        drawStrip (g, R (x0, 4.0f, 156.0f, stripH), yellow, sectionTitles[s]);
        if (editing)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillRect (R (x0, 17.0f, 156.0f, 1.5f));
        }
        drawStrip (g, R (x0, midStripY, 156.0f, stripH), stripGrey, {});
        drawLabel (g, { L, midStripY + 6.5f }, s == ds::secCymbals ? "TUNE / TONE" : "TUNE", 10.0f);
        drawLabel (g, { Rx, midStripY + 6.5f }, "LEVEL", 10.0f);

        drawStrip (g, R (x0, bottomStripY, 156.0f, stripH), stripGrey, {});
        const char* bl[] = { "ATTACK / TONE", "TONE", "DECAY", "", "DECAY", "DECAY", "CONGA SELECT" };
        const char* br[] = { "DECAY", "SNAPPY", "SELECT", "SELECT", "SELECT", "SELECT", "SELECT" };
        drawLabel (g, { L, bottomStripY + 6.5f }, bl[s], 10.0f, juce::Colours::white, 82.0f);
        drawLabel (g, { Rx, bottomStripY + 6.5f }, br[s], 10.0f, juce::Colours::white, 82.0f);

        for (auto* k : { &sec.tune, &sec.level, &sec.b1, &sec.b2 })
            if (k->slider.isVisible())
                drawKnobScale (g, *k);

        auto switchLabels = [&] (const SlideSwitch* sw, const char* top, const char* bottom)
        {
            if (sw == nullptr) return;
            const auto b = sw->getBounds().toFloat();
            drawLabel (g, { b.getCentreX(), b.getBottom() + 6.0f }, top, 7.0f, juce::Colours::white, 74.0f);
            if (bottom != nullptr)
                drawLabel (g, { b.getCentreX(), b.getBottom() + 14.0f }, bottom, 7.0f, juce::Colours::white, 74.0f);
        };
        if (s == ds::secToms)        switchLabels (sec.select.get(), "LOW   MID   HIGH", "1        2        3");
        if (s == ds::secRimClapBell) switchLabels (sec.select.get(), "R'SHT  H'CLP  C'BLL", nullptr);
        if (s == ds::secHiHat)       switchLabels (sec.select.get(), "CLOSED      OPEN", nullptr);
        if (s == ds::secCymbals)     switchLabels (sec.select.get(), "CRASH      RIDE", nullptr);
        if (s == ds::secCongas)
        {
            switchLabels (sec.select2.get(), "LOW   MID   HIGH", "1        2        3");
            switchLabels (sec.select.get(), "C'NGAS M'CAS CL'VES", nullptr);
        }

        // Sounds the kit does not have (909: cowbell, congas; 808: ride).
        if (sec.voice < 0)
            drawLabel (g, { x0 + 78.0f, 112.0f }, kit == ds::kit909 ? "808 ONLY" : "909 ONLY", 8.0f, yellow, 80.0f);
    }

    // EDIT section, where the original has its logo.
    {
        const float x0 = 1741.0f, w = 179.0f;
        drawStrip (g, R (x0, 4.0f, w, stripH), yellow, "EDIT");
        const auto name = R (x0 + 8.0f, 22.0f, w - 16.0f, 16.0f);
        g.setColour (juce::Colour (0xff050505));
        g.fillRoundedRectangle (name, 2.0f);
        g.setColour (segOn);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (juce::isPositiveAndBelow (editVoice, ds::numVoices) ? juce::String (ds::voices[editVoice].name).toUpperCase() : "-",
                    name, juce::Justification::centred);

        for (auto* k : { &frontCut, &pan, &distortion })
            drawKnobScale (g, *k);
        drawStrip (g, R (x0, midStripY, w, stripH), stripGrey, {});
        drawLabel (g, { frontCut.slider.getBounds().toFloat().getCentreX(), midStripY + 6.5f }, "FRONT CUT", 9.0f);
        drawLabel (g, { pan.slider.getBounds().toFloat().getCentreX(), midStripY + 6.5f }, "PAN", 9.0f);
        drawLabel (g, { distortion.slider.getBounds().toFloat().getCentreX(), midStripY + 6.5f }, "DIST", 9.0f);

        const auto gb = gmSwitch.getBounds().toFloat();
        drawLabel (g, { gb.getCentreX(), gb.getBottom() + 6.0f }, "808        909", 7.0f);
        drawLed (g, { x0 + 132.0f, 118.0f }, inLedHold > 0);
        drawLed (g, { x0 + 160.0f, 118.0f }, outLedHold > 0);
        drawLabel (g, { x0 + 132.0f, 132.0f }, "IN", 7.5f);
        drawLabel (g, { x0 + 160.0f, 132.0f }, "OUT", 7.5f);

        drawStrip (g, R (x0, bottomStripY, w, stripH), stripGrey, {});
        drawLabel (g, { gb.getCentreX(), bottomStripY + 6.5f }, "AUDITION MAP", 9.0f);
        drawLabel (g, { x0 + 146.0f, bottomStripY + 6.5f }, "MIDI", 9.0f);
    }

    // MIDI setup strip.
    {
        const auto strip = R (0.0f, (float) panelHeight, W, (float) stripHeight);
        g.setColour (setupBg);
        g.fillRect (strip);
        g.setColour (juce::Colour (0xff333333));
        g.drawHorizontalLine (panelHeight, 0.0f, W);

        g.setColour (setupText);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        auto label = [&] (const juce::Component& c, const juce::String& text, float width)
        {
            g.drawText (text, R ((float) c.getX() - width - 6.0f, (float) c.getY(), width, (float) c.getHeight()),
                        juce::Justification::centredRight);
        };
        label (outBox, "MIDI OUT", 70.0f);
        label (rxBox, "RX CH", 44.0f);
        label (inBox, "MIDI IN", 60.0f);
        label (txBox, "TX CH", 44.0f);

        juce::String status = proc.isOutputOpen() ? "Output open" : "No MIDI output selected";
        const int sysex = proc.sysexReceived.load();
        if (sysex > 0)
            status << "  |  SysEx received: " << sysex;
        g.setColour (proc.isOutputOpen() ? juce::Colour (0xff9a9a9a) : yellow);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText (status, R (1560.0f, (float) panelHeight, 380.0f, (float) stripHeight), juce::Justification::centredRight);
    }
}

void DrumstationPanel::resized()
{
    auto knobAt = [] (Knob& k, float cx, float cy, float size)
    {
        k.slider.setBounds (juce::Rectangle<float> (cx - size * 0.5f, cy - size * 0.5f, size, size).toNearestInt());
    };

    knobAt (volume, 147.0f, topKnobY, knobSize);
    kit808.setBounds (203, 24, 36, 16);
    kit909.setBounds (203, 58, 36, 16);

    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 3; ++col)
        {
            const auto c = keyCentre (row, col);
            keys[keyGrid[row][col]].setBounds (juce::Rectangle<float> (c.x - 17.0f, c.y - 8.0f, 34.0f, 16.0f).toNearestInt());
        }
    auditionButton.setBounds (457, 128, 36, 16);
    modeButton.setBounds (526, 128, 44, 16);

    for (int s = 0; s < ds::numSections; ++s)
    {
        auto& sec = sections[s];
        const float x0 = sectionX (s), L = x0 + 37.0f, Rx = x0 + 120.0f;
        knobAt (sec.tune, L, topKnobY, knobSize);
        knobAt (sec.level, Rx, topKnobY, knobSize);

        const bool hasB1 = s != ds::secRimClapBell && s != ds::secCongas;
        const bool hasB2 = s == ds::secBass || s == ds::secSnare;
        sec.b1.slider.setVisible (hasB1);
        sec.b2.slider.setVisible (hasB2);
        if (hasB1) knobAt (sec.b1, L, bottomKnobY, knobSize);
        if (hasB2) knobAt (sec.b2, Rx, bottomKnobY, knobSize);

        auto placeSwitch = [] (SlideSwitch* sw, float cx, int positions)
        {
            if (sw != nullptr)
                sw->setBounds (juce::Rectangle<float> (cx - (positions == 2 ? 20.0f : 26.0f), 118.0f,
                                                       positions == 2 ? 40.0f : 52.0f, 18.0f).toNearestInt());
        };
        if (s == ds::secCongas)
        {
            placeSwitch (sec.select2.get(), L, 3);
            placeSwitch (sec.select.get(), Rx, 3);
        }
        else if (sec.select != nullptr)
        {
            placeSwitch (sec.select.get(), Rx, (s == ds::secHiHat || s == ds::secCymbals) ? 2 : 3);
        }
    }

    knobAt (frontCut, 1741.0f + 32.0f, 58.0f, smallKnobSize);
    knobAt (pan, 1741.0f + 89.5f, 58.0f, smallKnobSize);
    knobAt (distortion, 1741.0f + 147.0f, 58.0f, smallKnobSize);
    gmSwitch.setBounds (1741 + 30, 110, 40, 18);

    // Setup strip.
    const int y = panelHeight + 11, h = 22;
    outBox.setBounds (156, y, 300, h);
    rxBox.setBounds (512, y, 56, h);
    inBox.setBounds (650, y, 300, h);
    txBox.setBounds (1006, y, 56, h);
    recallButton.setBounds (1086, y, 132, h);
    sendButton.setBounds (1226, y, 106, h);
    syncButton.setBounds (1340, y, 116, h);
    saveDumpButton.setBounds (1464, y, 120, h);
}

//==============================================================================
DrumstationEditor::DrumstationEditor (DrumstationProcessor& p)
    : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);
    panel.setBounds (0, 0, DrumstationPanel::designWidth, DrumstationPanel::designHeight);

    const double aspect = (double) DrumstationPanel::designWidth / DrumstationPanel::designHeight;
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio (aspect);
        c->setSizeLimits (1000, (int) (1000 / aspect), 2600, (int) (2600 / aspect));
    }
    setSize (1600, juce::roundToInt (1600 / aspect));
}

void DrumstationEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff111111));
}

void DrumstationEditor::resized()
{
    const float scale = (float) getWidth() / (float) DrumstationPanel::designWidth;
    panel.setTransform (juce::AffineTransform::scale (scale));
}
