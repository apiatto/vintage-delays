// vintage_delay_core: small filters shared by the engines. No dependencies beyond the
// C++ standard library.

#pragma once

#include <algorithm>
#include <cmath>

namespace vdc
{

constexpr double kPi = 3.14159265358979323846;

// Direct form II transposed biquad.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void setLowpass (double sampleRate, double cutoff, double q)
    {
        cutoff = std::min (cutoff, sampleRate * 0.45);
        const double w0 = 2.0 * kPi * cutoff / sampleRate;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 - cw) * 0.5 / a0;
        b1 = (1.0 - cw) / a0;
        b2 = b0;
        a1 = -2.0 * cw / a0;
        a2 = (1.0 - alpha) / a0;
    }

    double process (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() { z1 = z2 = 0; }
};

// 4th-order Butterworth low-pass made of two biquads.
struct Lowpass4
{
    Biquad s1, s2;
    void set (double sampleRate, double cutoff)
    {
        s1.setLowpass (sampleRate, cutoff, 0.54119610);
        s2.setLowpass (sampleRate, cutoff, 1.30656296);
    }
    double process (double x) { return s2.process (s1.process (x)); }
    void reset() { s1.reset(); s2.reset(); }
};

// First-order section: shelf from an analogue pole/zero pair, H(s) = (1 + s*tz) / (1 + s*tp),
// via the bilinear transform, or a first-order allpass.
struct FirstOrder
{
    double b0 = 1, b1 = 0, a1 = 0, z = 0;

    void setShelf (double sampleRate, double tz, double tp)
    {
        const double k = 2.0 * sampleRate;
        const double a0 = 1.0 + k * tp;
        b0 = (1.0 + k * tz) / a0;
        b1 = (1.0 - k * tz) / a0;
        a1 = (1.0 - k * tp) / a0;
    }

    void setAllpass (double sampleRate, double freq)
    {
        const double t = std::tan (kPi * std::min (freq, sampleRate * 0.45) / sampleRate);
        const double c = (t - 1.0) / (t + 1.0);
        b0 = c;
        b1 = 1.0;
        a1 = c;
    }

    double process (double x)
    {
        const double y = b0 * x + z;
        z = b1 * x - a1 * y;
        return y;
    }

    void reset() { z = 0; }
};

} // namespace vdc
