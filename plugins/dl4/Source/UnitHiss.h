// Background hiss of the hardware unit, added in the VINTAGE character.
//
// Calibration (2026-11): the published spec gives 90-95 dB dynamic range with +6 dB headroom,
// which puts the idle noise near -90 dBFS when 0 dBFS is the clip point. Demo recordings of
// the original unit show a dark hiss rather than a bright one (noise spectrum falling roughly
// 13-30 dB from 500 Hz to 8 kHz, part of it from the amps and mics in those videos). The engine
// alone idles at about -114 dBFS, so this adds the missing floor: white noise through a one-pole
// low-pass at 900 Hz plus a little flat noise, about 17 dB down at 8 kHz relative to 500 Hz.
#pragma once

#include <cmath>
#include <cstdint>

namespace holdfast
{
constexpr float kHissDbfs = -90.0f;

class UnitHiss
{
public:
    explicit UnitHiss (uint32_t seed = 0x9e3779b9u) : state (seed ? seed : 1u) {}

    void prepare (double sampleRate)
    {
        lpCoef = (float) (1.0 - std::exp (-2.0 * 3.14159265358979 * 900.0 / sampleRate));
        // Scale so the filtered sum has the target RMS: the one-pole output of unit white noise
        // has variance a/(2-a); the flat part is 0.08 of the white level.
        const float lpVar = lpCoef / (2.0f - lpCoef);
        const float rms = std::sqrt (lpVar * 1.0f + 0.08f * 0.08f);
        gain = std::pow (10.0f, kHissDbfs / 20.0f) / rms;
        lp = 0.0f;
    }

    // Adds hiss scaled by `amount` (0..1) to n samples; the same hiss also goes to `x2` if given
    // (one mono unit feeding both outputs).
    void add (float* x, int n, float amount, float* x2 = nullptr)
    {
        if (amount <= 0.0f)
            return;
        const float g = gain * amount;
        for (int i = 0; i < n; ++i)
        {
            const float w = white();
            lp += lpCoef * (w - lp);
            const float h = g * (lp + 0.08f * white());
            x[i] += h;
            if (x2 != nullptr)
                x2[i] += h;
        }
    }

private:
    // Approximately Gaussian, unit variance: sum of 4 uniforms (variance 4/12), rescaled.
    float white()
    {
        float s = 0.0f;
        for (int k = 0; k < 4; ++k)
        {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            s += (float) (state >> 8) * (1.0f / 16777216.0f) - 0.5f;
        }
        return s * 1.7320508f;
    }

    uint32_t state;
    float lpCoef = 0.1f, lp = 0.0f, gain = 0.0f;
};
} // namespace holdfast
