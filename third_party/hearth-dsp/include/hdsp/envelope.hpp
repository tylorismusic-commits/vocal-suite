// hearth-dsp — envelope.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

#include "util.hpp"

namespace hdsp {

// Topology-preserving one-pole low-pass (Zavalishin).
struct OnePoleLP {
  float g = 0.0f;
  float ic = 0.0f;

  void set(float cutoff, float sampleRate) noexcept {
    cutoff = clampf(cutoff, 1.0f, sampleRate * 0.49f);
    const float t = std::tan(kPi * cutoff / sampleRate);
    g = t / (1.0f + t);
  }
  void reset() noexcept { ic = 0.0f; }
  float process(float x) noexcept {
    const float v = (x - ic) * g;
    const float y = v + ic;
    ic = y + v;
    if (std::fabs(ic) < 1.0e-20f) {
      ic = 0.0f;
    }
    return y;
  }
};

// Two cascaded one-poles give a 12 dB/oct high-pass.
struct HighPass2 {
  OnePoleLP lp1;
  OnePoleLP lp2;

  void set(float cutoff, float sampleRate) noexcept {
    lp1.set(cutoff, sampleRate);
    lp2.set(cutoff, sampleRate);
  }
  void reset() noexcept {
    lp1.reset();
    lp2.reset();
  }
  float process(float x) noexcept {
    const float low = lp2.process(lp1.process(x));
    return x - low;
  }
};

// One-pole coefficient with the "settles to 95% in t ms" convention.
// Returned directly usable as s += (target - s) * k.
inline float settleK(double ms, double sampleRate) noexcept {
  if (ms <= 0.0) {
    return 1.0f; // instantaneous
  }
  const double k = 1.0 - std::pow(0.05, 1.0 / (sampleRate * ms * 0.001));
  return static_cast<float>(std::min(std::max(k, 0.0), 1.0));
}

} // namespace hdsp
