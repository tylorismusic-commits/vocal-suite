// hearth-dsp — smoother.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "db.hpp"

namespace hdsp {

// Per-sample parameter smoother.  setTime() uses the retention convention from
// timeCoeff(); setTarget() and next() implement a one-pole approach.
class Smoothed {
public:
  void setTime(float ms, float sampleRate) noexcept {
    coeff_ = timeCoeff(ms, sampleRate);
  }
  void reset(float value) noexcept { current_ = target_ = value; }
  void snap() noexcept { current_ = target_; }
  void setTarget(float value) noexcept { target_ = value; }
  float current() const noexcept { return current_; }
  float next() noexcept {
    current_ = target_ + coeff_ * (current_ - target_);
    return current_;
  }

  // Advance toward an explicit target without changing the stored one.
  float next(float target) noexcept {
    current_ = target + coeff_ * (current_ - target);
    return current_;
  }

private:
  float current_ = 0.0f;
  float target_ = 0.0f;
  float coeff_ = 0.0f;
};

} // namespace hdsp
