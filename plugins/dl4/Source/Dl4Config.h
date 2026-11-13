// Holdfast 512: configuration of the shared ADM engine, after a 1980 single-channel ADM digital
// delay rack (owner's manual of the 1-512 ms "time line" unit, derived from an earlier ADM delay).
//
// From the manual:
//   * ten memory taps: 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 ms at delay factor x1, grouped as
//     FLANGE (1-8), DOUBLE (16-64) and ECHO (128-512)
//   * SCALE x1/x2/x4 picks a window of eight taps (1-128, 2-256, 4-512 ms); SELECT steps up/down
//   * PROGRAM DELAY AUTO steps through the taps at the modulator SPEED ("auto flange", "arpeggio")
//   * DELAY FACTOR 0.25-1.0 (4:1), WIDTH up to the full two octaves, SPEED to about 10 Hz,
//     SHAPE continuous triangle -> sine -> square
//   * regeneration through switchable LO-EQ (175 Hz cut) and HI-EQ (3.3 kHz cut), PHASE in/out
//   * REPEAT recirculates the memory without loss; optional external memory module, 2.5 s
//   * +6 dB of headroom above the 0 dB (clip / slew) point
//
// Not in the manual (no specification page), taken from published data of the unit:
// 20 Hz - 15 kHz bandwidth at every delay, 90-95 dB dynamic range. The clock frequency and the
// pre-emphasis are estimates.

#pragma once

#include <vdc/AdmDelayEngine.h>

#include <array>

namespace holdfast
{

constexpr int kNumTaps = 10;    // 1 .. 512 ms
constexpr int kNumScales = 3;   // x1, x2, x4
constexpr int kWindow = 8;      // taps reachable with one SCALE setting

constexpr double kClockAtX1 = 256000.0;
constexpr int kCellsStandard = 131072;   // 512 ms at x1
constexpr int kCellsExtended = 640000;   // 2.5 s at x1 with the external memory module
constexpr double kExtendedMs = 2500.0;

// Nominal delay in ms at x1 of each tap. With the external module the longest tap reaches the end
// of the 2.5 s memory (the manual gives the module's length, not how it maps onto the taps).
inline std::array<double, kNumTaps> tapMs (bool extended)
{
    return { 1, 2, 4, 8, 16, 32, 64, 128, 256, extended ? kExtendedMs : 512.0 };
}

inline const char* scaleName (int scale)
{
    static const char* names[kNumScales] = { "x1", "x2", "x4" };
    return names[scale];
}

// SCALE (0..2) and SELECT (0..7) to the tap index (0..9).
inline int tapIndex (int scale, int select) { return scale + select; }

inline vdc::EngineConfig makeConfig (bool extendedMemory)
{
    vdc::EngineConfig c;
    c.cells = extendedMemory ? kCellsExtended : kCellsStandard;
    c.clockAtX1 = kClockAtX1;
    c.vcoRatio = 4.0;
    c.tapCells.clear();
    for (double ms : tapMs (extendedMemory))
        c.tapCells.push_back ((int) (ms * kClockAtX1 / 1000.0 + 0.5));
    c.bandwidthHz = 15000.0;
    c.feedbackLowpassHz = 12000.0;
    c.feedbackLowCutHz = 175.0;    // LO-EQ CUT
    c.feedbackHighCutHz = 3300.0;  // HI-EQ CUT
    c.preEmphasisZero = 50.0e-6;
    c.preEmphasisPole = 15.0e-6;
    c.syntheticStereo = false;  // single-channel unit; stereo comes from two linked units
    c.modulatorSwing = 0.5;     // WIDTH max = exactly the 4:1 (two octave) range
    c.feedbackMax = 0.97;       // "just below self-oscillation"
    return c;
}

// AUTO program: steps up and down through the eight taps of the SCALE window, one step per
// modulator cycle. Returns the SELECT position (0..7) for a running step count.
inline int autoSelect (long long stepCount)
{
    const int period = 2 * (kWindow - 1);
    const int pos = (int) (stepCount % period);
    return pos < kWindow ? pos : period - pos;
}

} // namespace holdfast
