// Vocal Suite — gate / downward expander
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

#include "common.hpp"
#include "controls.hpp"

namespace vocal {

class Gate {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    reset();
    setControls(Controls{});
  }

  void setControls(const Controls& c) noexcept {
    on_ = c.gateOn;
    thresholdDb_ = clampf(c.gateThresholdDb, -70.0f, -20.0f);
    rangeDb_ = clampf(c.gateRangeDb, 0.0f, 60.0f);
    attackMs_ = clampf(c.gateAttackMs, 0.1f, 100.0f);
    releaseMs_ = clampf(c.gateReleaseMs, 10.0f, 1000.0f);
  }

  float process(float& l, float& r) noexcept {
    if (!on_) {
      return 0.0f;
    }
    const float peak = std::max(std::fabs(l), std::fabs(r));
    const float detCoeff = timeCoeff(peak > det_ ? 2.0f : 80.0f, sr_);
    det_ += (peak - det_) * detCoeff;
    const float detDb = gainToDb(det_ + 1.0e-12f);

    if (!open_ && detDb > thresholdDb_ + 3.0f) {
      open_ = true;
    } else if (open_ && detDb < thresholdDb_ - 3.0f) {
      open_ = false;
    }
    const float target = open_ ? 1.0f : dbToGain(-rangeDb_);
    const float coeff = timeCoeff(target > gain_ ? attackMs_ : releaseMs_, sr_);
    gain_ += (target - gain_) * coeff;
    l *= gain_;
    r *= gain_;
    return std::max(0.0f, -gainToDb(gain_ + 1.0e-12f));
  }

  void reset() noexcept {
    det_ = 0.0f;
    gain_ = 1.0f;
    open_ = true;
  }

private:
  float sr_ = 48000.0f;
  bool on_ = false;
  float thresholdDb_ = -45.0f;
  float rangeDb_ = 30.0f;
  float attackMs_ = 2.0f;
  float releaseMs_ = 120.0f;
  float det_ = 0.0f;
  float gain_ = 1.0f;
  bool open_ = true;
};

} // namespace vocal
