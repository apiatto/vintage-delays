#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int senderIntervalMs = 4;
constexpr int maxMessagesPerTick = 3;           // ~750 messages/s, below the 1000/s a DIN cable carries
constexpr double programHoldOffMs = 150.0;      // let the DrumStation load the program first
constexpr double auditionMs = 200.0;
constexpr int auditionVelocity = 100;

juce::String twoDigits (int v) { return juce::String (v).paddedLeft ('0', 2); }

// Panel scales: tune knobs read -5..+5, the others 0..10, pan L/C/R.
juce::String formatUnipolar (int v) { return juce::String (v / 12.7f, 1); }
juce::String formatBipolar (int v)
{
    const float x = (v - 64) / 12.7f;
    return (x > 0.04f ? "+" : "") + juce::String (x, 1);
}
juce::String formatPan (int v)
{
    if (v == 64) return "C";
    return v < 64 ? "L" + juce::String (64 - v) : "R" + juce::String (v - 64);
}

juce::String findDevice (const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& id, const juce::String& name)
{
    for (const auto& d : devices)
        if (id.isNotEmpty() && d.identifier == id)
            return d.identifier;
    for (const auto& d : devices)
        if (name.isNotEmpty() && d.name == name)
            return d.identifier;
    // Nothing chosen yet: a port named after the unit in Audio MIDI Setup (e.g. "DrumStation").
    if (id.isEmpty() && name.isEmpty())
        for (const auto& d : devices)
            if (d.name.containsIgnoreCase ("drumstation") || d.name.containsIgnoreCase ("drum station"))
                return d.identifier;
    return {};
}
} // namespace

DrumstationProcessor::DrumstationProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DrumstationState", createLayout())
{
    for (auto& p : pendingIncoming)
        p.store (-1);

    for (int v = 0; v < ds::numVoices; ++v)
        for (int c = 0; c < ds::numControls; ++c)
        {
            const int cc = ds::controllerFor (ds::voices[v], (ds::Control) c);
            if (cc < 0)
                continue;
            jassert (byController[(size_t) cc].param == nullptr); // every controller belongs to one sound
            const auto id = paramId (v, (ds::Control) c);
            byController[(size_t) cc] = { apvts.getRawParameterValue (id), apvts.getParameter (id) };
        }
    byController[ds::ccVolume] = { apvts.getRawParameterValue (ParamIDs::volume), apvts.getParameter (ParamIDs::volume) };

    juce::PropertiesFile::Options options;
    options.applicationName = "Drumstation Editor";
    options.folderName = "Slopebox Audio";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    appProperties.setStorageParameters (options);

    // Defaults; a new instance starts on the ports used last time.
    auto& s = apvts.state;
    auto* settings = appProperties.getUserSettings();
    s.setProperty (StateIDs::outPort, settings->getValue (StateIDs::outPort), nullptr);
    s.setProperty (StateIDs::outPortName, settings->getValue (StateIDs::outPortName), nullptr);
    s.setProperty (StateIDs::inPort, settings->getValue (StateIDs::inPort), nullptr);
    s.setProperty (StateIDs::inPortName, settings->getValue (StateIDs::inPortName), nullptr);
    s.setProperty (StateIDs::rxChannel, settings->getIntValue (StateIDs::rxChannel, 10), nullptr);
    s.setProperty (StateIDs::txChannel, settings->getIntValue (StateIDs::txChannel, 10), nullptr);
    s.setProperty (StateIDs::recallOnLoad, true, nullptr);
    s.setProperty (StateIDs::gmSet, 0, nullptr);
    s.setProperty (StateIDs::kit, 1, nullptr);
    s.setProperty (StateIDs::editVoice, ds::findVoice (ds::kit909, ds::secBass, 0), nullptr);
    for (int i = 0; i < ds::numSections; ++i)
        s.setProperty (StateIDs::selectPrefix + juce::String (i), 0, nullptr);
    s.setProperty (StateIDs::congaSelect, 0, nullptr);
    rxChannelCached = juce::jlimit (1, 16, (int) s.getProperty (StateIDs::rxChannel));
    txChannelCached = juce::jlimit (1, 16, (int) s.getProperty (StateIDs::txChannel));

    markAllSent();
    openPortsFromState();

    juce::Timer::startTimerHz (30);
    juce::HighResolutionTimer::startTimer (senderIntervalMs);
}

DrumstationProcessor::~DrumstationProcessor()
{
    juce::HighResolutionTimer::stopTimer();
    juce::Timer::stopTimer();
    if (midiIn != nullptr)
        midiIn->stop();
    const juce::SpinLock::ScopedLockType lock (portLock);
    midiIn.reset();
    midiOut.reset();
}

juce::String DrumstationProcessor::paramId (int voice, ds::Control c)
{
    return juce::String (ds::voices[voice].id) + "_" + ds::controlSuffix (c);
}

juce::AudioProcessorValueTreeState::ParameterLayout DrumstationProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto attrs = [] (String (*fmt) (int))
    {
        return AudioParameterIntAttributes().withStringFromValueFunction ([fmt] (int v, int) { return fmt (v); });
    };

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::volume, 1 }, "Volume", 0, 127, 127,
                                                     attrs (formatUnipolar)));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::program, 1 }, "Program", 0, ds::numPrograms - 1, 0,
                                                     attrs (twoDigits)));

    for (int v = 0; v < ds::numVoices; ++v)
    {
        const auto& voice = ds::voices[v];
        auto group = std::make_unique<AudioProcessorParameterGroup> (voice.id, voice.name, " ");

        for (int c = 0; c < ds::numControls; ++c)
        {
            const auto ctl = (ds::Control) c;
            if (ds::controllerFor (voice, ctl) < 0)
                continue;

            const bool isTune = ctl == ds::ctlTune && String (voice.tuneName) == "Tune";
            const int def = (ctl == ds::ctlFrontCut || ctl == ds::ctlDistortion) ? 0 : 64;
            auto fmt = ctl == ds::ctlPan ? formatPan : (isTune ? formatBipolar : formatUnipolar);

            group->addChild (std::make_unique<AudioParameterInt> (
                ParameterID { paramId (v, ctl), 1 },
                String (voice.name) + " " + ds::controlName (voice, ctl),
                0, 127, def, attrs (fmt)));
        }
        layout.add (std::move (group));
    }
    return layout;
}

bool DrumstationProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void DrumstationProcessor::prepareToPlay (double, int) {}

//==============================================================================
// MIDI ports

void DrumstationProcessor::setOutputPort (const juce::String& identifier)
{
    std::unique_ptr<juce::MidiOutput> port;
    juce::String name;
    if (identifier.isNotEmpty())
    {
        port = juce::MidiOutput::openDevice (identifier);
        if (port != nullptr)
            name = port->getName();
    }
    {
        const juce::SpinLock::ScopedLockType lock (portLock);
        std::swap (midiOut, port);
    }
    apvts.state.setProperty (StateIDs::outPort, identifier, nullptr);
    apvts.state.setProperty (StateIDs::outPortName, name, nullptr);
    rememberPortsAsDefault();
}

void DrumstationProcessor::setInputPort (const juce::String& identifier)
{
    if (midiIn != nullptr)
        midiIn->stop();
    {
        const juce::SpinLock::ScopedLockType lock (portLock);
        midiIn.reset();
    }

    juce::String name;
    if (identifier.isNotEmpty())
    {
        auto port = juce::MidiInput::openDevice (identifier, this);
        if (port != nullptr)
        {
            name = port->getName();
            port->start();
            const juce::SpinLock::ScopedLockType lock (portLock);
            midiIn = std::move (port);
        }
    }
    apvts.state.setProperty (StateIDs::inPort, identifier, nullptr);
    apvts.state.setProperty (StateIDs::inPortName, name, nullptr);
    rememberPortsAsDefault();
}

bool DrumstationProcessor::isOutputOpen() const { return midiOut != nullptr || testSink != nullptr; }
bool DrumstationProcessor::isInputOpen() const { return midiIn != nullptr; }

void DrumstationProcessor::openPortsFromState()
{
    const auto& s = apvts.state;
    const auto out = findDevice (juce::MidiOutput::getAvailableDevices(),
                                 s.getProperty (StateIDs::outPort).toString(), s.getProperty (StateIDs::outPortName).toString());
    const auto in = findDevice (juce::MidiInput::getAvailableDevices(),
                                s.getProperty (StateIDs::inPort).toString(), s.getProperty (StateIDs::inPortName).toString());
    const auto outName = s.getProperty (StateIDs::outPortName);
    const auto inName = s.getProperty (StateIDs::inPortName);

    setOutputPort (out);
    setInputPort (in);

    // Keep the saved names when the interface is not connected, so it is found again next time.
    if (out.isEmpty()) apvts.state.setProperty (StateIDs::outPortName, outName, nullptr);
    if (in.isEmpty())  apvts.state.setProperty (StateIDs::inPortName, inName, nullptr);
}

void DrumstationProcessor::rememberPortsAsDefault()
{
    auto* settings = appProperties.getUserSettings();
    for (auto* key : { StateIDs::outPort, StateIDs::outPortName, StateIDs::inPort, StateIDs::inPortName,
                       StateIDs::rxChannel, StateIDs::txChannel })
        settings->setValue (key, apvts.state.getProperty (key));
    settings->saveIfNeeded();
}

void DrumstationProcessor::setRxChannel (int ch)
{
    ch = juce::jlimit (1, 16, ch);
    apvts.state.setProperty (StateIDs::rxChannel, ch, nullptr);
    rxChannelCached = ch;
    rememberPortsAsDefault();
}

void DrumstationProcessor::setTxChannel (int ch)
{
    ch = juce::jlimit (1, 16, ch);
    apvts.state.setProperty (StateIDs::txChannel, ch, nullptr);
    txChannelCached = ch;
    rememberPortsAsDefault();
}

void DrumstationProcessor::setMidiOutputForTest (std::function<void (const juce::MidiMessage&)> sink)
{
    const juce::SpinLock::ScopedLockType lock (portLock);
    testSink = std::move (sink);
}

void DrumstationProcessor::send (const juce::MidiMessage& m)
{
    const juce::SpinLock::ScopedLockType lock (portLock);
    if (testSink != nullptr)
        testSink (m);
    else if (midiOut != nullptr)
        midiOut->sendMessageNow (m);
    else
        return;
    midiOutActivity = true;
}

//==============================================================================

void DrumstationProcessor::requestSendAll() { sendAllRequested = true; }
void DrumstationProcessor::requestAudition (int voice) { auditionRequested = voice; }

void DrumstationProcessor::markAllSent()
{
    for (size_t cc = 0; cc < 128; ++cc)
        lastSent[cc] = byController[cc].value != nullptr ? juce::roundToInt (byController[cc].value->load()) : -1;
    lastProgramSent = juce::roundToInt (apvts.getRawParameterValue (ParamIDs::program)->load());
}

void DrumstationProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    // No audio: the DrumStation's outputs go to the audio interface.
    audio.clear();

    // Notes and other channel messages from the track go to the DrumStation on its receive channel.
    const int rx = rxChannelCached.load();
    for (const auto meta : midi)
    {
        auto m = meta.getMessage();
        if (m.getChannel() > 0)
            m.setChannel (rx);
        send (m);
    }
    midi.clear();
}

void DrumstationProcessor::hiResTimerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const int rx = rxChannelCached.load();

    if (resyncRequested.exchange (false))
        markAllSent();

    if (! isOutputOpen())
        return;

    if (sendAllRequested.exchange (false))
    {
        lastProgramSent = -1;
        for (auto& v : lastSent)
            v = -1;
    }

    // Audition: one note, released after a short time.
    const int audition = auditionRequested.exchange (-1);
    if (audition >= 0 && audition < ds::numVoices)
    {
        if (auditionNoteOn >= 0)
            send (juce::MidiMessage::noteOff (rx, auditionNoteOn));
        const auto& v = ds::voices[audition];
        const int note = (int) apvts.state.getProperty (StateIDs::gmSet, 0) == 0 ? v.noteGm808 : v.noteGm909;
        auditionNoteOn = -1;
        if (note >= 0)
        {
            send (juce::MidiMessage::noteOn (rx, note, (juce::uint8) auditionVelocity));
            auditionNoteOn = note;
            auditionOffAtMs = now + auditionMs;
        }
    }
    else if (auditionNoteOn >= 0 && now >= auditionOffAtMs)
    {
        send (juce::MidiMessage::noteOff (rx, auditionNoteOn));
        auditionNoteOn = -1;
    }

    // A plain program change does not resend the controllers: the DrumStation loads its stored kit.
    // "Send" and total recall resend everything after it, so the editor's sound is restored.
    const int program = juce::roundToInt (apvts.getRawParameterValue (ParamIDs::program)->load());
    if (program != lastProgramSent.load() && pendingIncomingProgram.load() < 0)
    {
        send (juce::MidiMessage::programChange (rx, program));
        lastProgramSent = program;
        holdOffUntilMs = now + programHoldOffMs;
    }

    if (now < holdOffUntilMs)
        return;

    int budget = maxMessagesPerTick;
    for (size_t cc = 0; cc < 128 && budget > 0; ++cc)
    {
        const auto& slot = byController[cc];
        if (slot.value == nullptr || pendingIncoming[cc].load() >= 0)
            continue;
        const int v = juce::roundToInt (slot.value->load());
        if (v == lastSent[cc].load())
            continue;
        send (juce::MidiMessage::controllerEvent (rx, (int) cc, v));
        lastSent[cc] = v;
        --budget;
    }
}

void DrumstationProcessor::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (m.isSysEx())
    {
        // Dump decoding: phase 2, once the format is known. Keep the raw bytes (F0 ... F7).
        const juce::ScopedLock sl (sysexLock);
        sysexLog.append (m.getRawData(), (size_t) m.getRawDataSize());
        ++sysexReceived;
        midiInActivity = true;
        return;
    }
    if (m.getChannel() != txChannelCached.load())
        return;

    if (m.isController())
    {
        const int cc = m.getControllerNumber();
        if (byController[(size_t) cc].param != nullptr)
        {
            lastSent[(size_t) cc] = m.getControllerValue();
            pendingIncoming[(size_t) cc] = m.getControllerValue();
            midiInActivity = true;
        }
    }
    else if (m.isProgramChange() && m.getProgramChangeNumber() < ds::numPrograms)
    {
        lastProgramSent = m.getProgramChangeNumber();
        pendingIncomingProgram = m.getProgramChangeNumber();
        midiInActivity = true;
    }
}

juce::MemoryBlock DrumstationProcessor::takeReceivedSysex()
{
    const juce::ScopedLock sl (sysexLock);
    juce::MemoryBlock out;
    out.swapWith (sysexLog);
    sysexReceived = 0;
    return out;
}

void DrumstationProcessor::timerCallback()
{
    // Apply values that came from the DrumStation. The sender holds these controllers back
    // until the parameter has the new value, so nothing is echoed.
    for (size_t cc = 0; cc < 128; ++cc)
    {
        const int v = pendingIncoming[cc].load();
        if (v < 0)
            continue;
        if (auto* p = byController[cc].param)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) v));
            p->endChangeGesture();
        }
        pendingIncoming[cc] = -1;
    }

    const int prog = pendingIncomingProgram.load();
    if (prog >= 0)
    {
        if (auto* p = apvts.getParameter (ParamIDs::program))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) prog));
            p->endChangeGesture();
        }
        pendingIncomingProgram = -1;
    }
}

void DrumstationProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DrumstationProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    const auto& s = apvts.state;
    rxChannelCached = juce::jlimit (1, 16, (int) s.getProperty (StateIDs::rxChannel, 10));
    txChannelCached = juce::jlimit (1, 16, (int) s.getProperty (StateIDs::txChannel, 10));

    openPortsFromState();
    // Total recall: the project's sound goes to the DrumStation when the project opens.
    if ((bool) s.getProperty (StateIDs::recallOnLoad, true))
        requestSendAll();
    else
        resyncRequested = true;
}

juce::AudioProcessorEditor* DrumstationProcessor::createEditor()
{
    return new DrumstationEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrumstationProcessor();
}
