// Vocal Suite — DSP smoke tests
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "dsp/engine.hpp"
#include "params.hpp"
#include "presets.hpp"

int main() {
  using namespace vocal;
  Engine engine;
  engine.prepare(48000.0f);
  Meters meters;

  // Silence in, silence out.
  {
    float l[64] = {};
    float r[64] = {};
    engine.process(l, r, 64, meters);
    for (int i = 0; i < 64; ++i) {
      assert(std::isfinite(l[i]) && std::isfinite(r[i]));
      assert(std::fabs(l[i]) < 1.0e-6f);
    }
  }

  // A sine through the full chain stays finite and bounded.
  {
    Controls c;
    c.compOn = true;
    c.compMode = 1;
    c.compAmount = 6;
    c.driveOn = true;
    c.driveAmount = 4;
    c.deessOn = true;
    c.sibAmount = 4;
    c.echoOn = true;
    c.echoMix = 20;
    c.spaceOn = true;
    c.spaceMix = 15;
    engine.setControls(c);
    float l[512];
    float r[512];
    for (int i = 0; i < 512; ++i) {
      l[i] = r[i] = 0.15f * std::sin(2.0 * 3.141592653589793 * 220.0 * i / 48000.0);
    }
    engine.process(l, r, 512, meters);
    for (int i = 0; i < 512; ++i) {
      assert(std::isfinite(l[i]) && std::isfinite(r[i]));
      assert(std::fabs(l[i]) < 2.0f && std::fabs(r[i]) < 2.0f);
    }
  }

  // Every factory preset loads and processes without NaN.
  for (const Preset& p : factoryPresets()) {
    Shared s;
    for (uint32_t i = 0; i < P_COUNT; ++i) {
      s.setFromHost(i, p.values[i]);
    }
    engine.setControls(controlsFrom(s));
    float l[128];
    float r[128];
    for (int i = 0; i < 128; ++i) {
      l[i] = r[i] = 0.2f * std::sin(2.0 * 3.141592653589793 * 440.0 * i / 48000.0);
    }
    engine.process(l, r, 128, meters);
    for (int i = 0; i < 128; ++i) {
      assert(std::isfinite(l[i]) && std::isfinite(r[i]));
    }
  }

  // Input learn reports the post-trim peak.
  {
    Controls c;
    c.inputLearnActive = true;
    c.compOn = false;
    c.toneOn = false;
    c.driveOn = false;
    c.deessOn = false;
    c.echoOn = false;
    c.spaceOn = false;
    c.limitOn = false;
    engine.setControls(c);
    float l[2048];
    float r[2048];
    for (int i = 0; i < 2048; ++i) {
      l[i] = r[i] = 0.1f * std::sin(2.0 * 3.141592653589793 * 1000.0 * i / 48000.0);
    }
    engine.process(l, r, 2048, meters);
    assert(meters.inputLearnPeakDb > -21.0f && meters.inputLearnPeakDb < -19.0f);
  }

  // Sibilance learn finds a 7 kHz tone in the 4-10 kHz search window.
  {
    Controls c;
    c.sibLearnActive = true;
    c.compOn = false;
    c.toneOn = false;
    c.driveOn = false;
    c.deessOn = false;
    c.echoOn = false;
    c.spaceOn = false;
    c.limitOn = false;
    engine.setControls(c);
    float l[8192];
    float r[8192];
    for (int i = 0; i < 8192; ++i) {
      l[i] = r[i] = 0.05f * std::sin(2.0 * 3.141592653589793 * 7000.0 * i / 48000.0);
    }
    engine.process(l, r, 8192, meters);
    assert(meters.sibLearnValid);
    assert(meters.sibLearnHz > 6500.0f && meters.sibLearnHz < 7500.0f);
  }

  // Host tempo drives the echo division and survives setControls(), which
  // carries the manual tempo fallback every block.
  {
    Controls c;
    c.compOn = false;
    c.toneOn = false;
    c.driveOn = false;
    c.deessOn = false;
    c.echoOn = true;
    c.echoSync = true;
    c.echoDiv = 3;       // 1/8D = 0.75 beats
    c.echoMix = 100.0f;
    c.echoFeedback = 0.0f;
    c.echoDuck = 0.0f;
    c.echoTone = 100.0f;
    c.echoMod = 0.0f;
    c.bpm = 120.0f;      // manual fallback
    c.spaceOn = false;
    c.limitOn = false;
    engine.setTempo(90.0f, true);
    engine.setControls(c);  // must not overwrite the host tempo
    std::vector<float> l(48000 * 2, 0.0f);
    std::vector<float> r(48000 * 2, 0.0f);
    l[0] = r[0] = 0.5f;
    engine.process(l.data(), r.data(), static_cast<int>(l.size()), meters);
    int peak = 0;
    float peakV = 0.0f;
    for (int i = 100; i < static_cast<int>(l.size()); ++i) {
      if (std::fabs(l[i]) > peakV) {
        peakV = std::fabs(l[i]);
        peak = i;
      }
    }
    const double expected = 48000.0 * 60.0 / 90.0 * 0.75;
    assert(std::fabs(peak - expected) < 200.0);
  }

  // Limiter ceiling moves the output ceiling.
  {
    Controls c;
    c.compOn = false;
    c.toneOn = false;
    c.driveOn = false;
    c.deessOn = false;
    c.echoOn = false;
    c.spaceOn = false;
    c.limitOn = true;
    c.limitCeilingDb = -6.0f;
    engine.setControls(c);
    std::vector<float> l(48000, 0.0f), r(48000, 0.0f);
    for (int i = 0; i < 48000; ++i) {
      l[i] = r[i] = 2.0f * std::sin(2.0 * 3.141592653589793 * 1000.0 * i / 48000.0);
    }
    engine.process(l.data(), r.data(), 48000, meters);
    float peak = 0.0f;
    for (float x : l) {
      peak = std::max(peak, std::fabs(x));
    }
    assert(peak <= 0.505f);
  }

  // Compare passes the dry signal through.
  {
    Controls c;
    c.compare = true;
    engine.setControls(c);
    float l[32];
    float r[32];
    for (int i = 0; i < 32; ++i) {
      l[i] = r[i] = 0.25f;
    }
    engine.process(l, r, 32, meters);
    for (int i = 0; i < 32; ++i) {
      assert(std::fabs(l[i] - 0.25f) < 1.0e-6f);
      assert(std::fabs(r[i] - 0.25f) < 1.0e-6f);
    }
  }

  std::printf("vocal engine: %d presets, all finite\n", presetCount());
  return 0;
}
