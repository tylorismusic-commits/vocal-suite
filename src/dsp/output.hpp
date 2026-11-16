// Vocal Suite — output trim and safety limiter
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cmath>

#include "common.hpp"
#include "controls.hpp"

namespace vocal {

class OutputStage {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    gain_.setTime(20.0f, sr_);
    gain_.reset(1.0f);
    setControls(Controls{});
  }

  void setControls(const Controls& c) noexcept {
    gain_.setTarget(dbToGain(c.outputDb));
    limit_ = c.limitOn;
  }

  // Returns the limiter gain reduction in dB (>= 0).
  float process(float& l, float& r) noexcept {
    const float g = gain_.next();
    l *= g;
    r *= g;
    if (!limit_) {
      return 0.0f;
    }
    constexpr float kCeiling = 0.9660509f;  // -0.3 dBFS
    const float al = std::fabs(l);
    const float ar = std::fabs(r);
    const float peak = std::max(al, ar);
    float gr = 0.0f;
    constexpr float kKneeStart = kCeiling - 0.05f;
    if (peak > kKneeStart) {
      // Soft knee that is continuous at kKneeStart, has unit slope there,
      // and asymptotes to the ceiling.  Zero latency by design.
      const float span = kCeiling - kKneeStart;
      const float limited = kCeiling - span * std::exp(-(peak - kKneeStart) / span);
      const float scale = limited / peak;
      l *= scale;
      r *= scale;
      gr = -gainToDb(scale);
    }
    return gr;
  }

  void reset() noexcept { gain_.snap(); }

private:
  float sr_ = 48000.0f;
  Smoothed gain_;
  bool limit_ = true;
};

} // namespace vocal
