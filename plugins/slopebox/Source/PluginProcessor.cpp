#include "PluginProcessor.h"
#include "PluginEditor.h"

SlopeboxProcessor::SlopeboxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SlopeboxState", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SlopeboxProcessor::createLayout()
{
    using namespace juce;
    using APF = AudioParameterFloat;
    using APC = AudioParameterChoice;
    using APB = AudioParameterBool;

    auto pct = [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; };
    auto db = [] (float v, int) { return String (v, 1) + " dB"; };
    auto bipolarPct = [] (float v, int) { return (v > 0 ? "+" : "") + String (juce::roundToInt (v * 100.0f)) + " %"; };

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::input, 1 }, "Input Level",
        NormalisableRange<float> (-24.0f, 26.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    layout.add (std::make_unique<APC> (ParameterID { ParamIDs::tap, 1 }, "Delay",
        StringArray { "Off", "Flange Hi (1 ms)", "Flange Mid (4 ms)", "Flange Lo (16 ms)", "Double (64 ms)", "Echo Short (256 ms)", "Echo Long (1024 ms)" }, 4));

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::delayFactor, 1 }, "Delay Factor",
        NormalisableRange<float> (0.25f, 1.0f, 0.001f), 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return "x" + String (v, 2); })));

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::width, 1 }, "Width",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    NormalisableRange<float> speedRange (0.1f, 10.0f);
    speedRange.setSkewForCentre (1.0f);
    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::speed, 1 }, "Speed", speedRange, 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " Hz"; })));

    layout.add (std::make_unique<APC> (ParameterID { ParamIDs::modSource, 1 }, "Modulator",
        StringArray { "Sine LFO", "Triangle LFO", "Envelope" }, 0));

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::feedback, 1 }, "Feedback",
        NormalisableRange<float> (-1.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (bipolarPct)));

    layout.add (std::make_unique<APB> (ParameterID { ParamIDs::repeat, 1 }, "Infinite Repeat", false));

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::mix, 1 }, "Delay Mix",
        NormalisableRange<float> (-1.0f, 1.0f), 0.5f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            if (std::abs (v) < 0.005f) return String ("Source");
            return (v > 0 ? "+" : "") + String (juce::roundToInt (v * 100.0f)) + " %";
        })));

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::stereo, 1 }, "Stereo",
        NormalisableRange<float> (0.0f, 1.0f), 1.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::character, 1 }, "Vintage",
        NormalisableRange<float> (0.0f, 1.0f), 1.0f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));

    layout.add (std::make_unique<APF> (ParameterID { ParamIDs::output, 1 }, "Output Trim",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (db)));

    return layout;
}

vdc::Params SlopeboxProcessor::readParams() const
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    vdc::Params p;
    p.inputDb = get (ParamIDs::input);
    p.tap = juce::jlimit (0, slopebox::kNumTaps - 1, juce::roundToInt (get (ParamIDs::tap)));
    p.delayFactor = get (ParamIDs::delayFactor);
    p.width = get (ParamIDs::width);
    p.speedHz = get (ParamIDs::speed);
    p.modSource = (vdc::ModSource) juce::jlimit (0, 2, juce::roundToInt (get (ParamIDs::modSource)));
    p.feedback = get (ParamIDs::feedback);
    p.repeat = get (ParamIDs::repeat) > 0.5f;
    p.mix = get (ParamIDs::mix);
    p.stereo = get (ParamIDs::stereo);
    p.character = get (ParamIDs::character);
    p.outputDb = get (ParamIDs::output);
    return p;
}

void SlopeboxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    scratch.setSize (2, samplesPerBlock);
}

bool SlopeboxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void SlopeboxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    if (scratch.getNumSamples() < numSamples)
        scratch.setSize (2, numSamples, false, false, true);

    const auto params = readParams();
    engine.setParams (params);

    const float* inL = numIn > 0 ? buffer.getReadPointer (0) : scratch.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : nullptr;
    if (numIn == 0)
        scratch.clear();

    float* outL = scratch.getWritePointer (0);
    float* outR = scratch.getWritePointer (1);
    engine.process (inL, inR, outL, outR, numSamples);

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

    displayDelayMs.store ((float) engine.getCurrentDelayMs());
    inputPeak.store (std::max ((float) engine.getInputLevel(), inputPeak.load() * 0.85f));
}

void SlopeboxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SlopeboxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* SlopeboxProcessor::createEditor()
{
    return new SlopeboxEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SlopeboxProcessor();
}
