// Offline checks and demo renders for Holdfast 512 (no JUCE needed).
//
//   holdfast_test            run the checks (exit code 0 = all passed); used by ctest
//   holdfast_test --render   also write demo WAVs (holdfast_<name>.wav) in the current directory
//
// Checks: every delay tap lands at its nominal time at x1 and x0.25 (standard and external
// memory), the AUTO program walks the SCALE window, REPEAT holds the memory without decay, the VCO shapes all sweep the clock over the
// full 4:1 range, and nothing goes non-finite at extreme settings.

#include "Dl4Config.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace
{
constexpr double fs = 48000.0;

using Buffer = std::vector<float>;

Buffer run (vdc::AdmDelayEngine& e, const Buffer& in, Buffer* right = nullptr)
{
    Buffer l (in.size()), r (in.size());
    for (size_t i = 0; i < in.size(); i += 256)
    {
        const int n = (int) std::min<size_t> (256, in.size() - i);
        e.process (&in[i], nullptr, &l[i], &r[i], n);
    }
    if (right) *right = r;
    return l;
}

// 2 kHz Hann-windowed burst, 2 ms long, centred at t0.
Buffer burst (size_t length, double t0)
{
    Buffer b (length, 0.0f);
    const int n = (int) (0.002 * fs);
    const size_t start = (size_t) (t0 * fs) - (size_t) n / 2;
    for (int i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * vdc::kPi * i / n);
        b[start + (size_t) i] = (float) (0.5 * w * std::sin (2.0 * vdc::kPi * 2000.0 * i / fs));
    }
    return b;
}

// Time (s) of the energy centroid of |x|^2 after 'from' seconds.
double centroid (const Buffer& x, double from, double to)
{
    double num = 0, den = 0;
    for (size_t i = (size_t) (from * fs); i < std::min (x.size(), (size_t) (to * fs)); ++i)
    {
        const double e = (double) x[i] * x[i];
        num += e * i;
        den += e;
    }
    return den > 0 ? num / den / fs : -1.0;
}

double rms (const Buffer& x, double from, double to)
{
    double s = 0;
    size_t n = 0;
    for (size_t i = (size_t) (from * fs); i < std::min (x.size(), (size_t) (to * fs)); ++i, ++n)
        s += (double) x[i] * x[i];
    return n ? std::sqrt (s / n) : 0.0;
}

int failures = 0;

void check (bool ok, const std::string& what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (! ok) ++failures;
}

void checkTiming (bool expanded)
{
    const auto ms = holdfast::tapMs (expanded);
    for (int tap = 0; tap < holdfast::kNumTaps; ++tap)
    {
        for (double factor : { 1.0, 0.25 })
        {
            const double expected = ms[(size_t) tap] * factor / 1000.0;
            const double t0 = 0.05;
            const Buffer in = burst ((size_t) ((t0 + expected + 0.3) * fs), t0);

            vdc::AdmDelayEngine e (holdfast::makeConfig (expanded));
            e.prepare (fs);
            vdc::Params p;
            p.tap = tap;
            p.delayFactor = (float) factor;
            p.mix = 1.0f;
            p.character = 1.0f;
            e.setParams (p);
            const Buffer out = run (e, in);

            // Look for the echo within +-25 % (and +-1 ms) of the expected time.
            const double tol = std::max (0.001, expected * 0.25);
            const double got = centroid (out, t0 + expected - tol, t0 + expected + tol) - t0;
            const double err = std::abs (got - expected);
            char msg[160];
            std::snprintf (msg, sizeof msg, "%s tap %d x%.2f: expected %.2f ms, got %.2f ms",
                           expanded ? "ext mem " : "standard", tap, factor, expected * 1000.0, got * 1000.0);
            check (got > 0 && err < 0.0006 + expected * 0.02, msg);
        }
    }
}

void checkRepeat()
{
    // Fill 128 ms of memory with a tone, then hold: the loop must keep its level for seconds.
    Buffer in ((size_t) (6.0 * fs), 0.0f);
    for (size_t i = 0; i < (size_t) (1.0 * fs); ++i)
        in[i] = (float) (0.3 * std::sin (2.0 * vdc::kPi * 440.0 * i / fs));

    vdc::AdmDelayEngine e (holdfast::makeConfig (false));
    e.prepare (fs);
    vdc::Params p;
    p.tap = 7; // 128 ms
    p.mix = 1.0f;
    e.setParams (p);

    Buffer out (in.size()), r (in.size());
    for (size_t i = 0; i < in.size(); i += 256)
    {
        if (i >= (size_t) (1.0 * fs)) { p.repeat = true; e.setParams (p); }
        const int n = (int) std::min<size_t> (256, in.size() - i);
        e.process (&in[i], nullptr, &out[i], &r[i], n);
    }
    const double early = rms (out, 1.5, 2.0), late = rms (out, 5.5, 6.0);
    char msg[120];
    std::snprintf (msg, sizeof msg, "repeat holds: rms 1.5-2 s %.3f, 5.5-6 s %.3f", early, late);
    check (early > 0.05 && std::abs (20.0 * std::log10 (late / early)) < 1.0, msg);
}

void checkVcoShapes()
{
    for (float shape : { 0.0f, 0.5f, 1.0f })
    {
        vdc::AdmDelayEngine e (holdfast::makeConfig (false));
        e.prepare (fs);
        vdc::Params p;
        p.tap = 4; // 16 ms
        p.width = 1.0f;
        p.speedHz = 2.0f;
        p.modSource = vdc::ModSource::Shape;
        p.shapeMorph = shape;
        e.setParams (p);

        Buffer in (64, 0.0f), l (64), r (64);
        double lo = 1e9, hi = 0;
        for (int b = 0; b < (int) (2.0 * fs / 64); ++b)
        {
            e.process (in.data(), nullptr, l.data(), r.data(), 64);
            if (b > (int) (0.5 * fs / 64))
            {
                lo = std::min (lo, e.getCurrentDelayMs());
                hi = std::max (hi, e.getCurrentDelayMs());
            }
        }
        char msg[120];
        std::snprintf (msg, sizeof msg, "VCO shape %.1f sweeps %.2f..%.2f ms (ratio %.2f)", shape, lo, hi, hi / lo);
        check (hi / lo > 2.8 && hi / lo < 4.5, msg); // triangle peaks are rounded by the 30 ms control smoothing
    }
}

void checkAutoProgram()
{
    // Up and down through the eight taps of the window: 0..7 then 6..1, period 14.
    const int expected[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1, 0, 1 };
    bool ok = true;
    for (int i = 0; i < 16; ++i)
        ok = ok && holdfast::autoSelect (i) == expected[i];
    check (ok, "AUTO program walks the SCALE window up and down");
    check (holdfast::tapIndex (2, 7) == 9 && holdfast::tapIndex (0, 0) == 0, "SCALE x4 reaches 512 ms, x1 starts at 1 ms");
}

void checkExtremes()
{
    Buffer in ((size_t) (2.0 * fs));
    uint32_t seed = 1;
    for (auto& v : in) { seed = seed * 1664525u + 1013904223u; v = (float) ((seed >> 8) / 8388608.0 - 1.0); }

    vdc::AdmDelayEngine e (holdfast::makeConfig (true));
    e.prepare (fs);
    vdc::Params p;
    p.inputDb = 26.0f; p.tap = 0; p.width = 1.0f; p.speedHz = 12.0f; p.modSource = vdc::ModSource::Shape;
    p.shapeMorph = 1.0f; p.feedback = 1.0f; p.mix = -1.0f;
    e.setParams (p);
    const Buffer out = run (e, in);
    bool finite = true;
    float peak = 0;
    for (float v : out) { finite = finite && std::isfinite (v); peak = std::max (peak, std::abs (v)); }
    char msg[120];
    std::snprintf (msg, sizeof msg, "extreme settings stay finite and bounded (peak %.2f)", peak);
    check (finite && peak < 40.0f, msg);
}

void writeWav (const std::string& path, const Buffer& l, const Buffer& r)
{
    std::ofstream f (path, std::ios::binary);
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    const uint32_t frames = (uint32_t) l.size();
    f.write ("RIFF", 4); w32 (36 + frames * 8); f.write ("WAVE", 4);
    f.write ("fmt ", 4); w32 (16); w16 (3); w16 (2); w32 ((uint32_t) fs); w32 ((uint32_t) fs * 8); w16 (8); w16 (32);
    f.write ("data", 4); w32 (frames * 8);
    for (uint32_t i = 0; i < frames; ++i) { f.write ((const char*) &l[i], 4); f.write ((const char*) &r[i], 4); }
}

void renderDemos()
{
    // Plucked test phrase.
    Buffer dry ((size_t) (9.0 * fs), 0.0f);
    const double notes[] = { 196.0, 246.94, 293.66, 392.0, 293.66, 246.94 };
    uint32_t seed = 3;
    for (int k = 0; k < 6; ++k)
    {
        const size_t start = (size_t) (fs * (0.2 + k * 0.5));
        Buffer line ((size_t) (fs / notes[k]));
        for (auto& v : line) { seed = seed * 1664525u + 1013904223u; v = (float) ((seed >> 8) / 8388608.0 - 1.0); }
        size_t idx = 0;
        float prev = 0;
        for (size_t i = start; i < std::min (dry.size(), start + (size_t) (fs * 1.5)); ++i)
        {
            const float cur = line[idx];
            line[idx] = 0.996f * 0.5f * (cur + prev);
            prev = cur;
            idx = (idx + 1) % line.size();
            dry[i] += 0.35f * cur;
        }
    }
    writeWav ("holdfast_dry.wav", dry, dry);

    struct Demo { const char* name; vdc::Params p; };
    std::vector<Demo> demos;
    vdc::Params p;
    using holdfast::tapIndex;
    // Settings from the "typical set ups" of the manual.
    p = {}; p.tap = tapIndex (0, 1); p.width = 1.0f; p.speedHz = 0.2f; p.modSource = vdc::ModSource::Shape; p.shapeMorph = 0.0f; p.feedback = 0.8f; p.mix = 0.5f;
    demos.push_back ({ "flange_triangle", p });
    p = {}; p.tap = tapIndex (1, 4); p.delayFactor = 0.6f; p.width = 0.15f; p.speedHz = 0.7f; p.modSource = vdc::ModSource::Shape; p.shapeMorph = 0.5f; p.feedback = 0.25f; p.mix = 0.5f;
    demos.push_back ({ "chorus_sine", p });
    p = {}; p.tap = tapIndex (0, 5); p.delayFactor = 1.0f; p.width = 0.25f; p.speedHz = 5.0f; p.modSource = vdc::ModSource::Shape; p.shapeMorph = 0.5f; p.mix = 1.0f;
    demos.push_back ({ "vibrato_32ms", p });
    p = {}; p.tap = tapIndex (1, 4); p.delayFactor = 0.5f; p.width = 0.4f; p.speedHz = 3.0f; p.modSource = vdc::ModSource::Shape; p.shapeMorph = 1.0f; p.mix = 1.0f;
    demos.push_back ({ "square_trill", p });
    p = {}; p.tap = tapIndex (2, 5); p.delayFactor = 0.5f; p.mix = 0.35f;
    demos.push_back ({ "slapback", p });
    p = {}; p.tap = tapIndex (2, 7); p.delayFactor = 0.8f; p.feedback = 0.6f; p.mix = 0.4f;
    demos.push_back ({ "echo_512", p });

    for (const auto& d : demos)
    {
        vdc::AdmDelayEngine e (holdfast::makeConfig (false));
        e.prepare (fs);
        e.setParams (d.p);
        Buffer r;
        const Buffer l = run (e, dry, &r);
        writeWav (std::string ("holdfast_") + d.name + ".wav", l, r);
        std::printf ("wrote holdfast_%s.wav\n", d.name);
    }
}
} // namespace

int main (int argc, char** argv)
{
    checkTiming (false);
    checkTiming (true);
    checkAutoProgram();
    checkRepeat();
    checkVcoShapes();
    checkExtremes();

    if (argc > 1 && std::string (argv[1]) == "--render")
        renderDemos();

    std::printf ("%s (%d failed)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
