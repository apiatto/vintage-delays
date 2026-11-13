// Slopebox 1024: configuration of the shared ADM engine, after the 1024 ms model of an
// early-80s ADM delay/flanger rack (owner's manual, ADM 256/1024).
//
//   * 256K x 1-bit memory, one VCO clock from 256 kHz (x1) to 1.024 MHz (x0.25)
//   * taps at 1, 4, 16, 64, 256 and 1024 ms at x1 (all buttons out = memory bypassed)
//   * pre-/de-emphasis, 16 kHz bandwidth, regeneration through a 12 kHz low-pass
//   * synthesised stereo on the ring of the output jack

#pragma once

#include <vdc/AdmDelayEngine.h>

namespace slopebox
{

// Delay buttons, in the order of vdc::EngineConfig::tapCells below.
enum class Tap : int { Off = 0, FlangeHi, FlangeMid, FlangeLo, Double, EchoShort, EchoLong };
constexpr int kNumTaps = 7;

inline vdc::EngineConfig makeConfig()
{
    vdc::EngineConfig c;
    c.cells = 262144;
    c.clockAtX1 = 256000.0;
    c.vcoRatio = 4.0;
    c.tapCells = { 0, 256, 1024, 4096, 16384, 65536, 262144 };
    c.bandwidthHz = 16000.0;
    c.feedbackLowpassHz = 12000.0;
    c.preEmphasisZero = 50.0e-6;
    c.preEmphasisPole = 15.0e-6;
    c.syntheticStereo = true;
    c.modulatorSwing = 0.55;
    c.feedbackMax = 0.97;
    return c;
}

} // namespace slopebox
