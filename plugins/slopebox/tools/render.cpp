// Offline render tool for the Slopebox engine.
//
//   slopebox_render                 renders the demo presets on a built-in test signal
//   slopebox_render input.wav       renders the demo presets on your own file (PCM 16/24/32 or float)
//
//   slopebox_render --write-ref f   renders the built-in signal and writes per-preset levels to f
//   slopebox_render --check f       renders the built-in signal and compares with f (regression test)
//
// Output files are written to the current directory as slopebox_<preset>.wav (stereo, 32-bit float).

#include "SlopeboxConfig.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct Audio
{
    double sampleRate = 48000;
    std::vector<float> l, r;
};

uint32_t rd32 (const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t) p[3] << 24); }
uint16_t rd16 (const unsigned char* p) { return (uint16_t) (p[0] | (p[1] << 8)); }

bool readWav (const std::string& path, Audio& out)
{
    std::ifstream f (path, std::ios::binary);
    if (! f) return false;
    std::vector<unsigned char> d ((std::istreambuf_iterator<char> (f)), {});
    if (d.size() < 12 || std::memcmp (d.data(), "RIFF", 4) != 0 || std::memcmp (d.data() + 8, "WAVE", 4) != 0) return false;

    int fmt = 0, ch = 0, bits = 0;
    size_t pos = 12;
    while (pos + 8 <= d.size())
    {
        const uint32_t sz = rd32 (&d[pos + 4]);
        const unsigned char* body = &d[pos + 8];
        if (std::memcmp (&d[pos], "fmt ", 4) == 0)
        {
            fmt = rd16 (body);
            ch = rd16 (body + 2);
            out.sampleRate = rd32 (body + 4);
            bits = rd16 (body + 14);
            if (fmt == 0xFFFE && sz >= 26) fmt = rd16 (body + 24);
        }
        else if (std::memcmp (&d[pos], "data", 4) == 0)
        {
            const int bps = bits / 8;
            const size_t frames = std::min<size_t> (sz, d.size() - pos - 8) / (size_t) (bps * ch);
            out.l.resize (frames);
            out.r.resize (frames);
            for (size_t i = 0; i < frames; ++i)
                for (int c = 0; c < std::min (ch, 2); ++c)
                {
                    const unsigned char* s = body + (i * ch + c) * bps;
                    float v = 0;
                    if (fmt == 3 && bits == 32) std::memcpy (&v, s, 4);
                    else if (bits == 16) v = (int16_t) rd16 (s) / 32768.0f;
                    else if (bits == 24) v = (float) ((int32_t) ((s[0] << 8) | (s[1] << 16) | ((uint32_t) s[2] << 24)) / 2147483648.0);
                    else if (bits == 32) v = (float) ((int32_t) rd32 (s) / 2147483648.0);
                    (c == 0 ? out.l : out.r)[i] = v;
                }
            if (ch == 1) out.r = out.l;
            return fmt == 1 || fmt == 3;
        }
        pos += 8 + sz + (sz & 1);
    }
    return false;
}

void writeWav (const std::string& path, const Audio& a)
{
    std::ofstream f (path, std::ios::binary);
    auto w32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto w16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    const uint32_t frames = (uint32_t) a.l.size();
    f.write ("RIFF", 4); w32 (36 + frames * 8); f.write ("WAVE", 4);
    f.write ("fmt ", 4); w32 (16); w16 (3); w16 (2); w32 ((uint32_t) a.sampleRate); w32 ((uint32_t) a.sampleRate * 8); w16 (8); w16 (32);
    f.write ("data", 4); w32 (frames * 8);
    for (uint32_t i = 0; i < frames; ++i) { f.write ((const char*) &a.l[i], 4); f.write ((const char*) &a.r[i], 4); }
}

// Built-in test signal: plucked notes (Karplus-Strong), a few drum hits and a short sweep.
Audio makeTestSignal (double fs)
{
    Audio a;
    a.sampleRate = fs;
    const size_t n = (size_t) (fs * 8.0);
    a.l.assign (n, 0.0f);
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> uni (-1.0f, 1.0f);

    const double notes[] = { 110.0, 164.81, 220.0, 261.63, 329.63, 392.0, 329.63, 220.0 };
    for (int k = 0; k < 8; ++k)
    {
        const size_t start = (size_t) (fs * (0.25 + k * 0.42));
        const int period = (int) (fs / notes[k]);
        std::vector<float> line ((size_t) period);
        for (auto& v : line) v = uni (rng);
        size_t idx = 0;
        float prev = 0;
        for (size_t i = start; i < std::min (n, start + (size_t) (fs * 1.6)); ++i)
        {
            const float cur = line[idx];
            const float nv = 0.996f * 0.5f * (cur + prev);
            prev = cur;
            line[idx] = nv;
            idx = (idx + 1) % line.size();
            a.l[i] += 0.35f * cur;
        }
    }

    // Kick and hat pattern in the second half.
    for (int k = 0; k < 8; ++k)
    {
        const size_t start = (size_t) (fs * (4.2 + k * 0.45));
        double ph = 0;
        for (size_t i = 0; i < (size_t) (fs * 0.3) && start + i < n; ++i)
        {
            const double t = i / fs;
            const double f0 = 50.0 + 120.0 * std::exp (-t * 30.0);
            ph += 2.0 * vdc::kPi * f0 / fs;
            a.l[start + i] += (float) (0.6 * std::sin (ph) * std::exp (-t * 9.0));
        }
        const size_t hat = start + (size_t) (fs * 0.225);
        for (size_t i = 0; i < (size_t) (fs * 0.05) && hat + i < n; ++i)
            a.l[hat + i] += 0.15f * uni (rng) * (float) std::exp (-(double) i / (fs * 0.01));
    }
    a.r = a.l;
    return a;
}

struct Preset
{
    const char* name;
    vdc::Params params;
};

std::vector<Preset> presets()
{
    // Settings taken from the "typical set ups" in the original owner's manual.
    // Knob clock positions: 7:00 = fully counter-clockwise, 5:00 = fully clockwise.
    using namespace vdc;
    using slopebox::Tap;
    std::vector<Preset> v;
    Params p;

    // Flanging: FLANGE button, WIDTH max, SPEED between 7 and 9 o'clock, mix +50 %.
    p = {}; p.tap = (int) Tap::FlangeMid; p.width = 1.0f; p.speedHz = 0.2f; p.mix = 0.5f;
    v.push_back ({ "flange", p });

    // Flanging with feedback: 1 or 4 ms, feedback near max, WIDTH max, slow speed.
    p = {}; p.tap = (int) Tap::FlangeMid; p.width = 1.0f; p.speedHz = 0.15f; p.feedback = 0.9f; p.mix = 0.5f;
    v.push_back ({ "flange_feedback_pos", p });

    p = {}; p.tap = (int) Tap::FlangeHi; p.width = 1.0f; p.speedHz = 0.15f; p.feedback = -0.9f; p.mix = -0.5f;
    v.push_back ({ "flange_feedback_neg", p });

    // Doubling: DOUBLE, mix +50 %, delay factor for about 30 ms, a little width and speed.
    p = {}; p.tap = (int) Tap::Double; p.delayFactor = 0.5f; p.width = 0.08f; p.speedHz = 0.3f; p.mix = 0.5f;
    v.push_back ({ "doubling", p });

    // Chorus: doubling plus a little feedback.
    p = {}; p.tap = (int) Tap::Double; p.delayFactor = 0.3f; p.width = 0.15f; p.speedHz = 0.6f; p.feedback = 0.3f; p.mix = 0.5f;
    v.push_back ({ "chorus", p });

    // Vibrato: 32 ms, small width, speed between 1 and 3 o'clock, delayed signal only.
    p = {}; p.tap = (int) Tap::Double; p.delayFactor = 0.5f; p.width = 0.12f; p.speedHz = 5.0f; p.mix = 1.0f;
    v.push_back ({ "vibrato", p });

    // Tuned resonance: 16 ms, feedback max, delay factor 0.5..0.75, mix at 100 %.
    p = {}; p.tap = (int) Tap::FlangeLo; p.delayFactor = 0.6f; p.feedback = 1.0f; p.mix = 1.0f;
    v.push_back ({ "tuned_resonance", p });

    // Slapback and echoes.
    p = {}; p.tap = (int) Tap::EchoShort; p.delayFactor = 0.5f; p.mix = 0.35f;
    v.push_back ({ "slapback", p });

    p = {}; p.tap = (int) Tap::EchoLong; p.delayFactor = 0.4f; p.feedback = 0.6f; p.mix = 0.4f;
    v.push_back ({ "echo_feedback", p });

    p = {}; p.tap = (int) Tap::EchoLong; p.delayFactor = 0.4f; p.feedback = 0.6f; p.mix = 0.4f; p.character = 0.0f;
    v.push_back ({ "echo_feedback_clean", p });

    // Hot input into long echoes: slope overload and limiter.
    p = {}; p.tap = (int) Tap::EchoLong; p.delayFactor = 1.0f; p.feedback = 0.8f; p.mix = 0.45f; p.inputDb = 9.0f; p.outputDb = -6.0f;
    v.push_back ({ "echo_1024ms_hot", p });

    // Envelope controlled sweep (stands in for an envelope follower on the CONTROL jack).
    p = {}; p.tap = (int) Tap::FlangeMid; p.width = 1.0f; p.modSource = ModSource::Envelope; p.feedback = 0.7f; p.mix = 0.5f;
    v.push_back ({ "envelope_sweep", p });

    return v;
}
} // namespace

int main (int argc, char** argv)
{
    std::string mode, refPath;
    Audio input;
    if (argc > 2 && (std::string (argv[1]) == "--write-ref" || std::string (argv[1]) == "--check"))
    {
        mode = argv[1];
        refPath = argv[2];
        input = makeTestSignal (48000.0);
    }
    else if (argc > 1)
    {
        if (! readWav (argv[1], input))
        {
            std::fprintf (stderr, "Cannot read %s (PCM 16/24/32-bit or 32-bit float WAV expected)\n", argv[1]);
            return 1;
        }
    }
    else
    {
        input = makeTestSignal (48000.0);
        writeWav ("slopebox_dry.wav", input);
    }

    // Add a tail so echoes can ring out.
    const size_t tail = (size_t) (input.sampleRate * 3.0);
    input.l.resize (input.l.size() + tail, 0.0f);
    input.r.resize (input.r.size() + tail, 0.0f);

    std::string refText;
    for (const auto& preset : presets())
    {
        vdc::AdmDelayEngine engine (slopebox::makeConfig());
        engine.prepare (input.sampleRate);
        engine.setParams (preset.params);

        Audio out;
        out.sampleRate = input.sampleRate;
        out.l.resize (input.l.size());
        out.r.resize (input.r.size());

        const auto t0 = std::chrono::steady_clock::now();
        const int block = 256;
        for (size_t i = 0; i < input.l.size(); i += block)
        {
            const int n = (int) std::min<size_t> (block, input.l.size() - i);
            engine.process (&input.l[i], &input.r[i], &out.l[i], &out.r[i], n);
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double audioSecs = input.l.size() / input.sampleRate;

        float peak = 0;
        double sumL = 0, sumR = 0;
        for (size_t i = 0; i < out.l.size(); ++i)
        {
            peak = std::max ({ peak, std::abs (out.l[i]), std::abs (out.r[i]) });
            sumL += (double) out.l[i] * out.l[i];
            sumR += (double) out.r[i] * out.r[i];
        }
        const double rmsL = 10.0 * std::log10 (sumL / out.l.size() + 1e-30);
        const double rmsR = 10.0 * std::log10 (sumR / out.r.size() + 1e-30);

        if (mode.empty())
        {
            const std::string file = std::string ("slopebox_") + preset.name + ".wav";
            writeWav (file, out);
            std::printf ("%-22s peak %6.2f dBFS  realtime x%6.1f\n", preset.name, 20.0 * std::log10 (std::max (peak, 1e-9f)), audioSecs / secs);
        }
        char line[160];
        std::snprintf (line, sizeof line, "%s %.4f %.4f\n", preset.name, rmsL, rmsR);
        refText += line;
    }

    if (mode == "--write-ref")
    {
        std::ofstream (refPath) << refText;
        return 0;
    }
    if (mode == "--check")
    {
        // Levels must match within 0.05 dB: catches behaviour changes, tolerates
        // floating-point differences between compilers and CPUs.
        std::ifstream ref (refPath);
        std::istringstream got (refText);
        std::string n1, n2;
        double a1, b1, a2, b2;
        int failures = 0, count = 0;
        while (ref >> n1 >> a1 >> b1)
        {
            if (! (got >> n2 >> a2 >> b2) || n1 != n2)
            {
                std::printf ("preset list differs at %s\n", n1.c_str());
                return 1;
            }
            ++count;
            const bool ok = std::abs (a1 - a2) < 0.05 && std::abs (b1 - b2) < 0.05;
            std::printf ("%-4s %-22s L %8.3f (ref %8.3f)  R %8.3f (ref %8.3f)\n", ok ? "ok" : "FAIL", n1.c_str(), a2, a1, b2, b1);
            failures += ok ? 0 : 1;
        }
        if (count == 0) { std::printf ("no reference data in %s\n", refPath.c_str()); return 1; }
        return failures == 0 ? 0 : 1;
    }
    return 0;
}
