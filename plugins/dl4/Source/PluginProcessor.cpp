#include "PluginProcessor.h"
#include "PluginEditor.h"

HoldfastProcessor::HoldfastProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HoldfastState", createLayout())
{
    apvts.addParameterListener (ParamIDs::memory, this);
}

HoldfastProcessor::~HoldfastProcessor()
{
    apvts.removeParameterListener (ParamIDs::memory, this);
    cancelPendingUpdate();
}

juce::AudioProcessorValueTreeState::ParameterLayout HoldfastProcessor::createLayout()
{
    using namespace juce;
    using APF = AudioParameterFloat;
    using APC = AudioParameterChoice;
    using APB = AudioParameterBool;
    using APInt = AudioParameterInt;

    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto db = [] (float v, int) { return String (v, 1) + " dB"; };

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::input, 1 }, "Input Level",
        NormalisableRange<float> (-24.0f, 26.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    layout.add (std::make_unique<APB> (ParameterID { ParamIDs::repeat, 1 }, "Repeat", false));
    layout.add (std::make_unique<APC> (ParameterID { ParamIDs::program, 1 }, "Program Delay", StringArray { "Manual", "Auto" }, 0));
    layout.add (std::make_unique<APC> (ParameterID { ParamIDs::scale, 1 }, "Scale", StringArray { "x1", "x2", "x4" }, 1));
    layout.add (std::make_unique<APInt> (ParameterID { ParamIDs::select, 1 }, "Select", 0, holdfast::kWindow - 1, 4));

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::feedback, 1 }, "Feedback",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    layout.add (std::make_unique<APB> (ParameterID { ParamIDs::fbPhase, 1 }, "Feedback Phase Out", false));
    layout.add (std::make_unique<APB> (ParameterID { ParamIDs::loEq, 1 }, "Lo EQ Cut", false));
    layout.add (std::make_unique<APB> (ParameterID { ParamIDs::hiEq, 1 }, "Hi EQ Cut", false));

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::delayFactor, 1 }, "Delay Factor",
        NormalisableRange<float> (0.25f, 1.0f, 0.001f), 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return "x" + String (v, 2); })));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::width, 1 }, "Width",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    NormalisableRange<float> speedRange (0.05f, 10.0f);
    speedRange.setSkewForCentre (1.0f);
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::speed, 1 }, "Speed", speedRange, 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " Hz"; })));

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::shape, 1 }, "Shape",
        NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            if (v < 0.05f) return String ("Triangle");
            if (std::abs (v - 0.5f) < 0.05f) return String ("Sine");
            if (v > 0.95f) return String ("Square");
            return String (v < 0.5f ? "Tri/Sine " : "Sine/Sq ") + String (roundToInt (v * 100.0f));
        })));

    layout.add (std::make_unique<APB> (ParameterID { ParamIDs::delayPhase, 1 }, "Delay Phase Out", false));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 1.0f), 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            if (v < 0.005f) return String ("Source");
            if (v > 0.995f) return String ("Delay");
            return String (roundToInt (v * 100.0f)) + " % delay";
        })));

    layout.add (std::make_unique<APB> (ParameterID { ParamIDs::memory, 1 }, "External Memory", false));

    layout.add (std::make_unique<APC> (ParameterID { ParamIDs::stereoMode, 1 }, "Stereo Mode",
        StringArray { "Mono", "Linked Pair" }, 0));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::spread, 1 }, "Pair Spread",
        NormalisableRange<float> (0.0f, 1.0f), 0.3f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::character, 1 }, "Vintage",
        NormalisableRange<float> (0.0f, 1.0f), 1.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<APB> (ParameterID { ParamIDs::pedalOn, 1 }, "Pedal Active", false));
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::pedal, 1 }, "Pedal",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::output, 1 }, "Output Level",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    return layout;
}

vdc::Params HoldfastProcessor::readParams() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto on = [&] (const char* id) { return get (id) > 0.5f; };

    vdc::Params p;
    p.inputDb = get (ParamIDs::input);
    const int scale = juce::jlimit (0, holdfast::kNumScales - 1, juce::roundToInt (get (ParamIDs::scale)));
    const int select = juce::jlimit (0, holdfast::kWindow - 1, juce::roundToInt (get (ParamIDs::select)));
    p.tap = holdfast::tapIndex (scale, select);
    p.delayFactor = get (ParamIDs::delayFactor);
    p.width = get (ParamIDs::width);
    p.speedHz = get (ParamIDs::speed);
    p.modSource = vdc::ModSource::Shape;
    p.shapeMorph = get (ParamIDs::shape);
    p.useExternalControl = on (ParamIDs::pedalOn);
    p.externalControl = get (ParamIDs::pedal);
    p.feedback = get (ParamIDs::feedback) * (on (ParamIDs::fbPhase) ? -1.0f : 1.0f);
    p.feedbackLowCut = on (ParamIDs::loEq);
    p.feedbackHighCut = on (ParamIDs::hiEq);
    p.repeat = on (ParamIDs::repeat);
    p.mix = get (ParamIDs::mix) * (on (ParamIDs::delayPhase) ? -1.0f : 1.0f);
    p.stereo = 0.0f;
    p.character = get (ParamIDs::character);
    p.outputDb = get (ParamIDs::output);
    return p;
}

bool HoldfastProcessor::wantsExpandedMemory() const
{
    return apvts.getRawParameterValue (ParamIDs::memory)->load() > 0.5f;
}

void HoldfastProcessor::applyMemorySetting()
{
    const bool expanded = wantsExpandedMemory();
    unitA.setConfig (holdfast::makeConfig (expanded));
    unitB.setConfig (holdfast::makeConfig (expanded));
    unitA.prepare (currentSampleRate);
    unitB.prepare (currentSampleRate);
    engineExpanded.store (expanded);
}

void HoldfastProcessor::parameterChanged (const juce::String&, float)
{
    triggerAsyncUpdate(); // may be called from the audio thread: never allocate here
}

void HoldfastProcessor::handleAsyncUpdate()
{
    if (wantsExpandedMemory() == engineExpanded.load())
        return;
    suspendProcessing (true);
    applyMemorySetting();
    suspendProcessing (false);
}

void HoldfastProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    applyMemorySetting();
    scratch.setSize (4, samplesPerBlock);
}

bool HoldfastProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void HoldfastProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    if (scratch.getNumSamples() < numSamples)
        scratch.setSize (4, numSamples, false, false, true);

    auto params = readParams();

    // PROGRAM DELAY AUTO: step through the taps of the SCALE window, one step each time the
    // modulator completes a cycle, so the steps stay in time with the VCO sweep.
    const bool autoProgram = juce::roundToInt (apvts.getRawParameterValue (ParamIDs::program)->load()) == 1;
    const double lfoPhase = unitA.getLfoPhase();
    if (autoProgram)
    {
        if (lfoPhase < lastLfoPhase)
            ++autoSteps;
        const int scale = juce::jlimit (0, holdfast::kNumScales - 1,
                                        juce::roundToInt (apvts.getRawParameterValue (ParamIDs::scale)->load()));
        params.tap = holdfast::tapIndex (scale, holdfast::autoSelect (autoSteps));
    }
    else
    {
        autoSteps = juce::roundToInt (apvts.getRawParameterValue (ParamIDs::select)->load());
    }
    lastLfoPhase = lfoPhase;
    displayTap.store (params.tap);

    const bool linked = juce::roundToInt (apvts.getRawParameterValue (ParamIDs::stereoMode)->load()) == 1;

    if (numIn == 0)
        scratch.clear();
    const float* inL = numIn > 0 ? buffer.getReadPointer (0) : scratch.getReadPointer (3);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;

    // SLEW meter: steepest input slope after the input gain, relative to a full-scale 4 kHz sine
    // (an approximation of where the delta modulator starts to slew-limit).
    {
        const float gain = juce::Decibels::decibelsToGain (params.inputDb);
        const float ref = (float) (2.0 * juce::MathConstants<double>::pi * 4000.0 / currentSampleRate);
        float maxStep = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = 0.5f * (inL[i] + inR[i]) * gain;
            maxStep = std::max (maxStep, std::abs (x - lastInput));
            lastInput = x;
        }
        slewPeak.store (std::max (maxStep / ref, slewPeak.load() * 0.85f));
    }

    float* aL = scratch.getWritePointer (0);
    float* aR = scratch.getWritePointer (1);
    float* bL = scratch.getWritePointer (2);
    float* bR = scratch.getWritePointer (3);

    if (! linked)
    {
        // One unit on the mono sum, the same signal on both outputs (dry stays per channel).
        unitA.setParams (params);
        unitA.process (inL, numIn > 1 ? inR : nullptr, aL, aR, numSamples);
    }
    else
    {
        // Linked pair: two units, one per side. SPREAD detunes the right unit's delay factor and
        // VCO rate, as when two hardware units were set by hand next to each other.
        const float spread = apvts.getRawParameterValue (ParamIDs::spread)->load();
        auto right = params;
        right.delayFactor = juce::jlimit (0.25f, 1.0f, params.delayFactor * (1.0f - 0.18f * spread));
        right.speedHz = params.speedHz * (1.0f + 0.27f * spread);
        right.externalControl = juce::jlimit (0.0f, 1.0f, params.externalControl + 0.1f * spread);

        unitA.setParams (params);
        unitB.setParams (right);
        // With no input, inL/inR point at channel 3 (cleared), which unit B also writes: the
        // engine reads each input sample before writing the outputs, so this is safe.
        unitA.process (inL, nullptr, aL, aR, numSamples);
        unitB.process (inR, nullptr, bL, bR, numSamples);
        juce::FloatVectorOperations::copy (aR, bL, numSamples);
    }

    if (numOut >= 2)
    {
        buffer.copyFrom (0, 0, scratch, 0, 0, numSamples);
        buffer.copyFrom (1, 0, scratch, 1, 0, numSamples);
    }
    else if (numOut == 1)
    {
        buffer.copyFrom (0, 0, scratch, 0, 0, numSamples);
        buffer.addFrom (0, 0, scratch, 1, 0, numSamples);
        buffer.applyGain (0, 0, numSamples, 0.5f);
    }

    displayDelayMs.store ((float) unitA.getCurrentDelayMs());
    inputPeak.store (std::max ((float) unitA.getInputLevel(), inputPeak.load() * 0.85f));
}

void HoldfastProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HoldfastProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* HoldfastProcessor::createEditor()
{
    return new HoldfastEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HoldfastProcessor();
}
