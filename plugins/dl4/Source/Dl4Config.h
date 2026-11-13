// Holdfast 512: configuration of the shared ADM engine, after a 1980 single-channel
// ADM digital delay rack (1-512 ms, three ranges of stepped delays plus a 4:1 sweep).
//
// Public specifications this follows:
//   * mono, 1 to 512 ms, discrete steps in three ranges, continuous x0.25..x1 tuning between steps
//   * 20 Hz - 15 kHz (+1/-3 dB) at every delay, 90-95 dB dynamic range, +6 dB headroom
//   * THD+N 0.2 % at x0.25 and 0.4 % at x1: distortion rises as the clock slows, as in ADM
//   * VCO 0 to >10 Hz, depth 0-100 % of the 4:1 range, shape continuous triangle -> sine -> square
//   * feedback, infinite repeat/hold, optional memory expansion up to 2 s
//
// ESTIMATED (to check against the owner's manual): the exact steps of each range, the clock
// frequency, the pre-emphasis and the feedback filter. Steps are assumed to double, three per
// range: 2/4/8, 16/32/64 and 128/256/512 ms at x1 (0.5 ms to 512 ms with the sweep).

#pragma once

#include <vdc/AdmDelayEngine.h>

#include <array>

namespace holdfast
{

constexpr int kNumRanges = 3;
constexpr int kStepsPerRange = 3;
constexpr int kNumTaps = kNumRanges * kStepsPerRange;

constexpr double kClockAtX1 = 256000.0;
constexpr int kCellsStandard = 131072;     // 512 ms at x1
constexpr int kCellsExpanded = 4 * 131072; // 2048 ms at x1 with the memory expansion

inline const char* rangeName (int range)
{
    static const char* names[kNumRanges] = { "SHORT", "MEDIUM", "LONG" };
    return names[range];
}

// Nominal delay in ms at x1 for each tap, standard and expanded memory.
inline std::array<int, kNumTaps> tapMs (bool expanded)
{
    if (expanded)
        return { 2, 4, 8, 16, 32, 64, 512, 1024, 2048 };
    return { 2, 4, 8, 16, 32, 64, 128, 256, 512 };
}

inline int tapIndex (int range, int step) { return range * kStepsPerRange + step; }

inline vdc::EngineConfig makeConfig (bool expandedMemory)
{
    vdc::EngineConfig c;
    c.cells = expandedMemory ? kCellsExpanded : kCellsStandard;
    c.clockAtX1 = kClockAtX1;
    c.vcoRatio = 4.0;
    c.tapCells.clear();
    for (int ms : tapMs (expandedMemory))
        c.tapCells.push_back ((int) (ms * kClockAtX1 / 1000.0));
    c.bandwidthHz = 15000.0;
    c.feedbackLowpassHz = 12000.0;
    c.preEmphasisZero = 50.0e-6;
    c.preEmphasisPole = 15.0e-6;
    c.syntheticStereo = false;  // single-channel unit; stereo comes from two linked units
    c.modulatorSwing = 0.5;     // 100 % depth = exactly the 4:1 range
    c.feedbackMax = 0.97;
    return c;
}

} // namespace holdfast
