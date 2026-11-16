// Vocal Suite — input stage
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <hdsp/biquad.hpp>

#include "common.hpp"
#include "controls.hpp"

namespace vocal {

class InputStage {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    gain_.setTime(20.0f, sr_);
    gain_.reset(1.0f);
    hpf_[0].reset();
    hpf_[1].reset();
    setControls(Controls{});
  }

  void setControls(const Controls& c) noexcept {
    gain_.setTarget(dbToGain(c.inputDb));
    polarity_ = c.polarity;
    if (c.hpfOn != hpfOn_ || c.hpfHz != hpfHz_) {
      hpfOn_ = c.hpfOn;
      hpfHz_ = c.hpfHz;
      if (hpfOn_) {
        hpf_[0].highPass(hpfHz_, 0.7071f, sr_);
        hpf_[1].highPass(hpfHz_, 0.7071f, sr_);
      }
    }
  }

  void process(float& l, float& r) noexcept {
    const float g = gain_.next();
    l *= g;
    r *= g;
    if (polarity_) {
      l = -l;
      r = -r;
    }
    if (hpfOn_) {
      l = hpf_[0].process(l);
      r = hpf_[1].process(r);
    }
  }

  void reset() noexcept {
    gain_.snap();
    hpf_[0].reset();
    hpf_[1].reset();
  }

private:
  float sr_ = 48000.0f;
  Smoothed gain_;
  hdsp::Biquad hpf_[2];
  bool hpfOn_ = true;
  float hpfHz_ = 90.0f;
  bool polarity_ = false;
};

} // namespace vocal
