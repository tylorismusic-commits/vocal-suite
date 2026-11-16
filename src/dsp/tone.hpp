// Vocal Suite — three-band tone section with air shelf
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

#include <hdsp/biquad.hpp>

#include "common.hpp"
#include "controls.hpp"

namespace vocal {

class Tone {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    reset();
    setControls(Controls{});
  }

  void setControls(const Controls& c) noexcept {
    if (hasLast_ && c.lowGain == last_.lowGain && c.lowFreq == last_.lowFreq &&
        c.lowBell == last_.lowBell && c.midGain == last_.midGain &&
        c.midFreq == last_.midFreq && c.midQ == last_.midQ &&
        c.highGain == last_.highGain && c.highFreq == last_.highFreq &&
        c.highBell == last_.highBell && c.air == last_.air) {
      return;
    }
    last_ = c;
    hasLast_ = true;

    if (c.lowBell) {
      low_[0].peak(c.lowFreq, c.lowGain, 1.0f, sr_);
      low_[1].peak(c.lowFreq, c.lowGain, 1.0f, sr_);
    } else {
      low_[0].lowShelf(c.lowFreq, c.lowGain, 0.7f, sr_);
      low_[1].lowShelf(c.lowFreq, c.lowGain, 0.7f, sr_);
    }
    mid_[0].peak(c.midFreq, c.midGain, c.midQ, sr_);
    mid_[1].peak(c.midFreq, c.midGain, c.midQ, sr_);
    if (c.highBell) {
      high_[0].peak(c.highFreq, c.highGain, 1.0f, sr_);
      high_[1].peak(c.highFreq, c.highGain, 1.0f, sr_);
    } else {
      high_[0].highShelf(c.highFreq, c.highGain, 0.7f, sr_);
      high_[1].highShelf(c.highFreq, c.highGain, 0.7f, sr_);
    }
    air_[0].highShelf(12000.0f, c.air, 0.7f, sr_);
    air_[1].highShelf(12000.0f, c.air, 0.7f, sr_);

    active_ = std::fabs(c.lowGain) > 0.01f || std::fabs(c.midGain) > 0.01f ||
              std::fabs(c.highGain) > 0.01f || std::fabs(c.air) > 0.01f;
  }

  void process(float& l, float& r) noexcept {
    if (!active_) {
      return;
    }
    l = air_[0].process(high_[0].process(mid_[0].process(low_[0].process(l))));
    r = air_[1].process(high_[1].process(mid_[1].process(low_[1].process(r))));
  }

  void reset() noexcept {
    for (int ch = 0; ch < 2; ++ch) {
      low_[ch].reset();
      mid_[ch].reset();
      high_[ch].reset();
      air_[ch].reset();
    }
  }

private:
  float sr_ = 48000.0f;
  hdsp::Biquad low_[2], mid_[2], high_[2], air_[2];
  Controls last_{};
  bool hasLast_ = false;
  bool active_ = false;
};

} // namespace vocal
