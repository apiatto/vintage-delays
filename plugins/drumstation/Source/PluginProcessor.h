#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include "DrumstationMap.h"

namespace ParamIDs
{
inline constexpr const char* volume  = "volume";
inline constexpr const char* program = "program";
} // namespace ParamIDs

// Editor state that is saved with the project but is not a host parameter.
namespace StateIDs
{
inline constexpr const char* outPort      = "outPort";      // MIDI output device identifier
inline constexpr const char* outPortName  = "outPortName";  // its name, to find it again if the identifier changes
inline constexpr const char* inPort       = "inPort";
inline constexpr const char* inPortName   = "inPortName";
inline constexpr const char* rxChannel    = "rxChannel";    // channel the DrumStation receives on (we send on it)
inline constexpr const char* txChannel    = "txChannel";    // channel the DrumStation transmits on (we listen on it)
inline constexpr const char* recallOnLoad = "recallOnLoad"; // send the stored sound when the project opens
inline constexpr const char* gmSet        = "gmSet";        // 0 = 808, 1 = 909: note map used by audition
inline constexpr const char* kit          = "kit";          // kit shown on the panel
inline constexpr const char* editVoice    = "editVoice";    // sound shown in the EDIT section
inline constexpr const char* selectPrefix = "select";       // + section index: SELECT switch positions
inline constexpr const char* congaSelect  = "congaSelect";
} // namespace StateIDs

// Hardware editor for the Novation DrumStation, in the style of the usual hardware editors:
// it opens the MIDI ports itself (no External Instrument needed), so it works whether or not the
// host is playing. Every sound parameter is a host parameter holding the 0-127 controller value;
// when it changes (panel, automation, preset) the matching CC goes to the DrumStation.
// Controllers coming back from the DrumStation's knobs move the parameters without being echoed.
// Notes played on the host track are forwarded to the DrumStation on its receive channel.
class DrumstationProcessor : public juce::AudioProcessor,
                             private juce::Timer,
                             private juce::HighResolutionTimer,
                             private juce::MidiInputCallback
{
public:
    DrumstationProcessor();
    ~DrumstationProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Parameter id for a sound control, e.g. "k808_bd_tune".
    static juce::String paramId (int voice, ds::Control);

    // MIDI ports (message thread). An empty identifier closes the port.
    void setOutputPort (const juce::String& identifier);
    void setInputPort (const juce::String& identifier);
    bool isOutputOpen() const;
    bool isInputOpen() const;

    int getRxChannel() const { return rxChannelCached.load(); }
    int getTxChannel() const { return txChannelCached.load(); }
    void setRxChannel (int channel);
    void setTxChannel (int channel);

    // Panel requests (message thread).
    void requestSendAll();              // program change, then volume and every sound controller
    void requestAudition (int voice);   // note on/off for one sound

    // SysEx received from the DrumStation since the last save (dump decoding comes with phase 2;
    // until then the editor can save the raw dumps to a .syx file).
    std::atomic<int> sysexReceived { 0 };
    juce::MemoryBlock takeReceivedSysex();

    juce::AudioProcessorValueTreeState apvts;

    // Set when MIDI arrives from / leaves for the DrumStation (for the LEDs).
    std::atomic<bool> midiInActivity { false }, midiOutActivity { false };

    // For tests: stop the sender thread and drive it by hand.
    void stopSenderThreadForTest() { juce::HighResolutionTimer::stopTimer(); }
    void runSenderTick() { hiResTimerCallback(); }
    void setMidiOutputForTest (std::function<void (const juce::MidiMessage&)> sink);
    void receiveForTest (const juce::MidiMessage& m) { handleIncomingMidiMessage (nullptr, m); }
    void applyIncomingForTest() { timerCallback(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;                 // message thread: apply incoming values
    void hiResTimerCallback() override;            // sender thread: outgoing controllers
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void send (const juce::MidiMessage&);
    void markAllSent();
    void openPortsFromState();
    void rememberPortsAsDefault();

    struct Slot
    {
        std::atomic<float>* value = nullptr;      // raw parameter value 0..127
        juce::RangedAudioParameter* param = nullptr;
    };

    std::array<Slot, 128> byController {};                 // controller number -> parameter
    std::array<std::atomic<int>, 128> lastSent;            // last value sent or received per controller
    std::atomic<int> lastProgramSent { -1 };

    // Incoming values waiting to be applied to the parameters on the message thread.
    std::array<std::atomic<int>, 128> pendingIncoming;
    std::atomic<int> pendingIncomingProgram { -1 };

    std::atomic<bool> sendAllRequested { false }, resyncRequested { false };
    std::atomic<int> auditionRequested { -1 };
    int auditionNoteOn = -1;
    double auditionOffAtMs = 0.0, holdOffUntilMs = 0.0;

    std::atomic<int> rxChannelCached { 10 }, txChannelCached { 10 };

    juce::CriticalSection sysexLock;
    juce::MemoryBlock sysexLog;

    juce::SpinLock portLock;
    std::unique_ptr<juce::MidiOutput> midiOut;
    std::unique_ptr<juce::MidiInput> midiIn;
    std::function<void (const juce::MidiMessage&)> testSink;

    juce::ApplicationProperties appProperties;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumstationProcessor)
};
