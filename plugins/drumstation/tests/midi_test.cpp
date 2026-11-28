// MIDI behaviour of the Drumstation Editor, without hardware: the output port is replaced by a sink.
#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <vector>

namespace
{
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok)
        ++failures;
}

void setParam (DrumstationProcessor& p, const juce::String& id, int value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 ((float) value));
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::vector<juce::MidiMessage> sent;

    DrumstationProcessor proc;
    proc.stopSenderThreadForTest();
    proc.setMidiOutputForTest ([&sent] (const juce::MidiMessage& m) { sent.push_back (m); });
    proc.setRxChannel (10);
    proc.setTxChannel (10);

    // Every controller of the manual's map (20-119) belongs to exactly one parameter.
    int mapped = 0;
    for (int v = 0; v < ds::numVoices; ++v)
        for (int c = 0; c < ds::numControls; ++c)
            if (ds::controllerFor (ds::voices[v], (ds::Control) c) >= 0)
                ++mapped;
    check (mapped == 100, "100 controllers mapped (CC 20-119)");

    // Nothing is sent on creation.
    proc.runSenderTick();
    check (sent.empty(), "no messages at start-up");

    // A knob change sends its controller on the receive channel.
    const int bd909 = ds::findVoice (ds::kit909, ds::secBass, 0);
    setParam (proc, DrumstationProcessor::paramId (bd909, ds::ctlB1), 100);   // 909 BD attack = CC 77
    proc.runSenderTick();
    check (sent.size() == 1 && sent[0].isController() && sent[0].getControllerNumber() == 77
               && sent[0].getControllerValue() == 100 && sent[0].getChannel() == 10,
           "909 bass drum attack -> CC 77 = 100 on channel 10");
    sent.clear();

    // A controller from the unit's own knob moves the parameter and is not echoed back.
    proc.receiveForTest (juce::MidiMessage::controllerEvent (10, 29, 20));    // 808 SD tune
    proc.runSenderTick();
    proc.applyIncomingForTest();
    proc.runSenderTick();
    const int sd808 = ds::findVoice (ds::kit808, ds::secSnare, 0);
    const int sdTune = juce::roundToInt (proc.apvts.getRawParameterValue (DrumstationProcessor::paramId (sd808, ds::ctlTune))->load());
    check (sdTune == 20, "incoming CC 29 sets 808 snare tune");
    check (sent.empty(), "incoming controller not echoed");

    // Controllers on another channel are ignored.
    proc.receiveForTest (juce::MidiMessage::controllerEvent (3, 29, 90));
    proc.applyIncomingForTest();
    const int sdTune2 = juce::roundToInt (proc.apvts.getRawParameterValue (DrumstationProcessor::paramId (sd808, ds::ctlTune))->load());
    check (sdTune2 == 20, "controller on another channel ignored");

    // Send all: program change first, then volume and all 100 sound controllers, spread over ticks.
    proc.requestSendAll();
    for (int i = 0; i < 200; ++i)
    {
        proc.runSenderTick();
        juce::Thread::sleep (2);
    }
    int controllers = 0;
    for (const auto& m : sent)
        if (m.isController())
            ++controllers;
    check (! sent.empty() && sent[0].isProgramChange(), "send all starts with a program change");
    check (controllers == 101, "send all sends volume + 100 sound controllers");
    sent.clear();

    // Program change alone does not resend the controllers.
    setParam (proc, ParamIDs::program, 25);
    for (int i = 0; i < 100; ++i)
    {
        proc.runSenderTick();
        juce::Thread::sleep (2);
    }
    check (sent.size() == 1 && sent[0].isProgramChange() && sent[0].getProgramChangeNumber() == 25,
           "program 25 -> program change 25 only");
    sent.clear();

    // Audition: note on, then note off.
    proc.requestAudition (bd909);
    proc.runSenderTick();
    juce::Thread::sleep (250);
    proc.runSenderTick();
    check (sent.size() == 2 && sent[0].isNoteOn() && sent[0].getNoteNumber() == 24 && sent[1].isNoteOff(),
           "audition 909 bass drum: note 24 on/off (GM set 808)");

    // SysEx from the unit is kept for saving.
    const juce::uint8 dump[] = { 0xf0, 0x00, 0x20, 0x29, 0x01, 0x02, 0xf7 };
    proc.receiveForTest (juce::MidiMessage::createSysExMessage (dump + 1, 5));
    check (proc.sysexReceived.load() == 1 && proc.takeReceivedSysex().getSize() == sizeof (dump), "sysex kept with F0/F7");

    std::printf ("%s\n", failures == 0 ? "all passed" : "FAILED");
    return failures == 0 ? 0 : 1;
}
