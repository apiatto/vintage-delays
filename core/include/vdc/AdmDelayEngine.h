// vintage_delay_core: a variable-clock, 1-bit adaptive delta modulation (ADM) delay engine,
// modelled on the signal flow of late-70s / early-80s ADM delay rack units.
//
// Signal flow (one mono channel, as on the hardware):
//
//   input -> level amp -> soft limit -> pre-emphasis -> (+ feedback) -> anti-alias filter
//         -> ADM encoder -> 1-bit memory (one ring, fixed taps) -> ADM decoder at the selected tap
//         -> reconstruction filter -> de-emphasis -> delay mix
//   The feedback is taken before de-emphasis, through a low-pass.
//   Optional "synthesised stereo" on the right output: lows in phase, mids/highs phase-shifted.
//
// Encoder, memory and decoder share one clock from a linear VCO. At delay factor x1 the clock
// is config.clockAtX1 (longest delay); the VCO control 0..1 (0..5 V on the hardware) raises it
// by config.vcoRatio. The delay is (tap cells / clock), so modulating the clock also modulates
// pitch, like a tape machine with variable speed. WIDTH crossfades the VCO control between the
// DELAY FACTOR setting and the modulator. REPEAT disconnects the input and lets the whole memory
// loop onto itself, bit for bit.
//
// Everything that differs between units lives in EngineConfig; see the plugins for examples.
// Depends only on the C++ standard library.

#pragma once

#include "Filters.h"

#include <cstdint>
#include <vector>

namespace vdc
{

struct EngineConfig
{
    int cells = 262144;                 // 1-bit memory cells; the full memory is the REPEAT loop
    double clockAtX1 = 256000.0;        // clock at delay factor x1 (longest delays)
    double vcoRatio = 4.0;              // clock range of the VCO control (x1 .. x0.25 = 4:1)
    std::vector<int> tapCells { 0, 256, 1024, 4096, 16384, 65536, 262144 }; // 0 = memory bypassed
    double bandwidthHz = 16000.0;       // anti-alias and reconstruction filters (4th order)
    double feedbackLowpassHz = 12000.0; // filter in the regeneration path
    double preEmphasisZero = 50.0e-6;   // pre-emphasis H(s) = (1 + s*zero) / (1 + s*pole)
    double preEmphasisPole = 15.0e-6;   // set both to 0 to disable
    bool syntheticStereo = true;        // phase-shift stereo on the right output
    double modulatorSwing = 0.55;       // LFO swing around 0.5 in VCO control units (0.55 -> ~5:1)
    double feedbackMax = 0.97;          // regeneration gain at full FEEDBACK
    double admSlewMax = 1.2e5;          // full-scale slew limits of the delta modulator, units/s
    double admSlewMin = 40.0;           // sets the idle (granular) noise floor
    double admLeakTau = 0.0015;         // integrator leak, seconds
    double admSyllabicTau = 0.004;      // step-size adaptation, seconds
};

enum class ModSource : int
{
    Sine = 0,
    Triangle = 1,
    Envelope = 2,  // the input envelope drives the VCO (louder = shorter delay)
    Shape = 3,     // continuous morph triangle -> sine -> square, see Params::shapeMorph
};

struct Params
{
    float inputDb = 0.0f;        // input level, also the overall gain
    int tap = 1;                 // index into EngineConfig::tapCells
    float delayFactor = 1.0f;    // 0.25 .. 1.0 (for vcoRatio 4)
    float width = 0.0f;          // 0..1: crossfade of the VCO control from DELAY FACTOR to the modulator
    float speedHz = 0.1f;        // LFO rate
    ModSource modSource = ModSource::Sine;
    float shapeMorph = 0.5f;     // ModSource::Shape: 0 = triangle, 0.5 = sine, 1 = square
    bool useExternalControl = false; // CONTROL jack plugged in: externalControl replaces DELAY FACTOR,
    float externalControl = 0.0f;    // 0..1 = 0..5 V (0 = x1, 1 = x1/vcoRatio); WIDTH still mixes in the LFO
    float feedback = 0.0f;       // -1..1, centre = no regeneration, negative = out of phase
    bool repeat = false;         // infinite repeat
    float mix = 0.0f;            // -1..1: -1 = delayed only, inverted; 0 = source; +1 = delayed only
    float stereo = 1.0f;         // 0 = mono, 1 = full synthesised stereo (if the config has it)
    float character = 1.0f;      // 0 = clean storage, 1 = full ADM conversion
    float outputDb = 0.0f;       // plugin trim
};

class AdmDelayEngine
{
public:
    explicit AdmDelayEngine (EngineConfig c = {}) : cfg (std::move (c)) {}

    const EngineConfig& config() const { return cfg; }

    // Not real-time safe: allocates the memory.
    void setConfig (EngineConfig c)
    {
        cfg = std::move (c);
        prepare (fs);
    }

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        bitMem.assign ((size_t) cfg.cells, 0);
        cleanMem.assign ((size_t) cfg.cells, 0.0f);

        const double bw = std::min (cfg.bandwidthHz, fs * 0.45);
        antiAlias.set (fs, bw);
        reconstruct.set (fs, bw);
        feedbackLp.setLowpass (fs, cfg.feedbackLowpassHz, 0.7071);

        usePreEmphasis = cfg.preEmphasisZero > 0.0 && cfg.preEmphasisPole > 0.0;
        if (usePreEmphasis)
        {
            preEmphasis.setShelf (fs, cfg.preEmphasisZero, cfg.preEmphasisPole);
            deEmphasis.setShelf (fs, cfg.preEmphasisPole, cfg.preEmphasisZero);
        }

        const double apFreqs[4] = { 450.0, 1300.0, 3500.0, 9000.0 };
        for (int i = 0; i < 4; ++i)
            phaseShift[i].setAllpass (fs, apFreqs[i]);
        stereoLowpass.setLowpass (fs, 250.0, 0.7071);

        controlSmoothCoef = 1.0 - std::exp (-1.0 / (fs * 0.03));
        paramSmoothCoef = 1.0 - std::exp (-1.0 / (fs * 0.02));
        envAttack = 1.0 - std::exp (-1.0 / (fs * 0.003));
        envRelease = 1.0 - std::exp (-1.0 / (fs * 0.12));
        dcCoef = std::exp (-2.0 * kPi * 8.0 / fs);
        reset();
    }

    void reset()
    {
        clearMemory();
        writeIdx = 0;
        tickPhase = 0;
        enc = {};
        dec = {};
        lastDecoded = lastClean = 0;
        prevU = 0;
        antiAlias.reset();
        reconstruct.reset();
        feedbackLp.reset();
        preEmphasis.reset();
        deEmphasis.reset();
        for (auto& ap : phaseShift) ap.reset();
        stereoLowpass.reset();
        fbSample = 0;
        dcX = dcY = 0;
        env = 0;
        lfoPhase = 0.75;
        firstBlock = true;
        inputLevel = 0;
    }

    void setParams (const Params& newParams) { p = newParams; }

    double getCurrentDelayMs() const { return currentDelayMs; }
    double getCurrentClockHz() const { return currentClockHz; }
    // Peak level seen by the encoder, 1.0 = LIMIT (0 dB reference = 0 dBFS).
    double getInputLevel() const { return inputLevel; }

    // inR may be null for mono input. Output is always stereo.
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples)
    {
        const double inGain = dbToGain (p.inputDb);
        const double outGain = dbToGain (p.outputDb);
        const double minFactor = 1.0 / cfg.vcoRatio;
        const double dfControl = p.useExternalControl
                                     ? std::clamp ((double) p.externalControl, 0.0, 1.0)
                                     : factorToControl (std::clamp ((double) p.delayFactor, minFactor, 1.0));
        const double width = std::clamp ((double) p.width, 0.0, 1.0);
        const double lfoInc = std::clamp ((double) p.speedHz, 0.05, 20.0) / fs;
        const double stereoAmt = cfg.syntheticStereo ? std::clamp ((double) p.stereo, 0.0, 1.0) : 0.0;
        const int tapIndex = std::clamp (p.tap, 0, (int) cfg.tapCells.size() - 1);
        const int readOffset = std::clamp (cfg.tapCells[(size_t) tapIndex], 0, cfg.cells);
        const bool bypassMemory = readOffset == 0;
        const double vcoSpan = cfg.vcoRatio - 1.0;
        const int cells = cfg.cells;

        if (firstBlock)
        {
            control = (1.0 - width) * dfControl + width * modulator();
            fbSm = p.feedback;
            mixSm = p.mix;
            charSm = p.character;
            firstBlock = false;
        }

        double peak = 0;

        for (int i = 0; i < numSamples; ++i)
        {
            const double srcL = inL[i] * inGain;
            const double srcR = (inR != nullptr ? inR[i] : inL[i]) * inGain;
            const double x = 0.5 * (srcL + srcR);
            peak = std::max (peak, std::abs (x));

            fbSm += (p.feedback - fbSm) * paramSmoothCoef;
            mixSm += (p.mix - mixSm) * paramSmoothCoef;
            charSm += (p.character - charSm) * paramSmoothCoef;

            const double ax = std::abs (x);
            env += (ax - env) * (ax > env ? envAttack : envRelease);

            // VCO control (0..1 = 0..5 V): WIDTH crossfades DELAY FACTOR and the modulator.
            const double targetControl = (1.0 - width) * dfControl + width * modulator();
            control += (targetControl - control) * controlSmoothCoef;
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0) lfoPhase -= 1.0;

            // Linear VCO: control 0 -> clockAtX1, control 1 -> clockAtX1 * vcoRatio.
            const double clock = cfg.clockAtX1 * std::max (1.0 + vcoSpan * control, 0.35);
            currentClockHz = clock;
            currentDelayMs = 1000.0 * readOffset / clock;

            // Input stage: soft limit, pre-emphasis, feedback sum, encoder range, anti-alias.
            double u = softLimit (x);
            if (usePreEmphasis) u = preEmphasis.process (u);
            u += cfg.feedbackMax * fbSm * fbSample;
            u = 2.0 * std::tanh (0.5 * u); // the encoder cannot track beyond full scale
            u = antiAlias.process (u);

            // Clock-rate dependent ADM constants (time constants are in real time).
            const double leak = std::exp (-1.0 / (clock * cfg.admLeakTau));
            const double syllabic = std::exp (-1.0 / (clock * cfg.admSyllabicTau));
            const double stepMax = cfg.admSlewMax / clock;
            const double stepMin = cfg.admSlewMin / clock;

            tickPhase += clock / fs;
            const int ticks = (int) tickPhase;
            tickPhase -= ticks;

            if (ticks > 0)
            {
                double accAdm = 0, accClean = 0;
                const double invTicks = 1.0 / ticks;
                for (int t = 0; t < ticks; ++t)
                {
                    // With REPEAT the input is disconnected and the cell keeps the bit written
                    // one full memory ago, so the whole memory recirculates unchanged.
                    if (! p.repeat)
                    {
                        const double s = prevU + (u - prevU) * ((t + 1) * invTicks);
                        const uint8_t bit = s >= enc.integ ? 1 : 0;
                        stepAdm (enc, bit, leak, syllabic, stepMax, stepMin);
                        bitMem[(size_t) writeIdx] = bit;
                        cleanMem[(size_t) writeIdx] = (float) s;
                    }

                    int readIdx = bypassMemory ? writeIdx : writeIdx - readOffset + 1;
                    if (readIdx < 0) readIdx += cells;

                    const uint8_t outBit = bitMem[(size_t) readIdx];
                    stepAdm (dec, outBit, leak, syllabic, stepMax, stepMin);
                    accAdm += dec.integ;
                    accClean += cleanMem[(size_t) readIdx];

                    if (++writeIdx >= cells)
                        writeIdx = 0;
                }
                lastDecoded = accAdm * invTicks;
                lastClean = accClean * invTicks;
            }
            prevU = u;

            // D/A stage: blend clean/ADM, reconstruction, DC blocker.
            double wet = lastClean + charSm * (lastDecoded - lastClean);
            wet = reconstruct.process (wet);
            const double dcOut = wet - dcX + dcCoef * dcY;
            dcX = wet;
            dcY = dcOut;
            wet = dcOut;

            // Regenerated signal, still pre-emphasised, through the feedback low-pass.
            fbSample = feedbackLp.process (wet);
            if (usePreEmphasis) wet = deEmphasis.process (wet);

            // DELAY MIX: centre-tapped pot, source at the centre, either phase of the delayed
            // signal at the ends.
            const double m = std::clamp (mixSm, -1.0, 1.0);
            const double dryGain = 1.0 - std::abs (m);
            const double l = srcL * dryGain + m * wet;
            const double r = srcR * dryGain + m * wet;

            outL[i] = (float) (l * outGain);

            if (stereoAmt > 0.0)
            {
                // Synthesised stereo: lows stay in phase, mids and highs go through a phase shifter.
                const double lowR = stereoLowpass.process (r);
                double shifted = r - lowR;
                for (auto& ap : phaseShift) shifted = ap.process (shifted);
                const double rStereo = lowR + shifted;
                outR[i] = (float) ((r + stereoAmt * (rStereo - r)) * outGain);
            }
            else
            {
                outR[i] = (float) (r * outGain);
            }
        }

        inputLevel = peak;

        if (! std::isfinite (enc.integ) || ! std::isfinite (dec.integ) || ! std::isfinite (dcY))
            reset();
    }

private:
    struct AdmState
    {
        double integ = 0;
        double step = 0;
        uint8_t history = 0;
    };

    // DELAY FACTOR (x1 .. x1/vcoRatio) to VCO control 0..1 for a linear VCO.
    double factorToControl (double factor) const { return (1.0 / factor - 1.0) / (cfg.vcoRatio - 1.0); }

    // Modulator voltage, 0..1 = 0..5 V. The LFO swings a little beyond the DELAY FACTOR range.
    double modulator() const
    {
        const double swing = cfg.modulatorSwing;
        switch (p.modSource)
        {
            case ModSource::Triangle: return 0.5 + swing * triangle();
            case ModSource::Envelope: return std::min (env * 6.0, 1.1);
            case ModSource::Shape:    return 0.5 + swing * morphShape (std::clamp ((double) p.shapeMorph, 0.0, 1.0));
            case ModSource::Sine:     return 0.5 + swing * std::sin (2.0 * kPi * lfoPhase);
        }
        return 0.5;
    }

    double triangle() const { return 1.0 - 4.0 * std::abs (lfoPhase - 0.5); }

    // 0 = triangle, 0.5 = sine, 1 = square with slightly rounded edges.
    double morphShape (double morph) const
    {
        const double sine = std::sin (2.0 * kPi * lfoPhase);
        if (morph <= 0.5)
            return triangle() + (sine - triangle()) * (morph * 2.0);
        const double square = std::tanh (8.0 * sine) / std::tanh (8.0);
        return sine + (square - sine) * ((morph - 0.5) * 2.0);
    }

    static void stepAdm (AdmState& st, uint8_t bit, double leak, double syllabic, double stepMax, double stepMin)
    {
        st.history = (uint8_t) (((st.history << 1) | bit) & 7);
        const bool overload = st.history == 0 || st.history == 7;
        st.step = st.step * syllabic + (overload ? (1.0 - syllabic) * stepMax : 0.0);
        if (st.step < stepMin) st.step = stepMin;
        st.integ = st.integ * leak + (bit ? st.step : -st.step);
        st.integ = std::clamp (st.integ, -2.5, 2.5);
    }

    static double softLimit (double x)
    {
        // Clean up to the LIMIT point (0 dBFS), then a smooth ceiling with about 6 dB of headroom.
        const double a = std::abs (x);
        if (a < 1.0) return x;
        const double y = 1.0 + std::tanh (a - 1.0);
        return x < 0 ? -y : y;
    }

    static double dbToGain (double db) { return std::pow (10.0, db / 20.0); }

    // Silence in delta modulation is an alternating 1010... pattern, not all zeros.
    void clearMemory()
    {
        for (size_t i = 0; i < bitMem.size(); ++i)
            bitMem[i] = (uint8_t) (i & 1);
        std::fill (cleanMem.begin(), cleanMem.end(), 0.0f);
    }

    EngineConfig cfg;
    double fs = 48000;
    Params p;

    std::vector<uint8_t> bitMem;
    std::vector<float> cleanMem;
    int writeIdx = 0;
    double tickPhase = 0;

    AdmState enc, dec;
    double lastDecoded = 0, lastClean = 0, prevU = 0;

    Lowpass4 antiAlias, reconstruct;
    Biquad feedbackLp, stereoLowpass;
    FirstOrder preEmphasis, deEmphasis;
    FirstOrder phaseShift[4];
    bool usePreEmphasis = true;

    double control = 0, fbSm = 0, mixSm = 0, charSm = 1;
    double controlSmoothCoef = 0, paramSmoothCoef = 0;
    double env = 0, envAttack = 0, envRelease = 0;
    double lfoPhase = 0.75;
    double fbSample = 0;
    double dcX = 0, dcY = 0, dcCoef = 0.999;
    bool firstBlock = true;

    double currentDelayMs = 0, currentClockHz = 0, inputLevel = 0;
};

} // namespace vdc
