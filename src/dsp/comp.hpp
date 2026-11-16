// Vocal Suite — three-mode vocal compressor
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>

#include "comp_controls.hpp"
#include "common.hpp"
#include "controls.hpp"
#include "fet.hpp"
#include "optical.hpp"
#include "vca.hpp"

namespace vocal {

class VocalComp {
public:
  enum Mode { Opto = 0, Fet = 1, Vca = 2 };

  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    fet_.prepare(sr_, FetLimiter::Variant::Blue);
    opto_.prepare(sr_, OptoLeveler::Variant::LA2A);
    vca_.prepare(sr_);
    mode_ = Opto;
    update(Controls{});
    reset();
  }

  void setControls(const Controls& c) noexcept {
    update(c);
  }

  // Returns gain reduction in dB (>= 0).
  float process(float& l, float& r) noexcept {
    switch (mode_) {
      case Fet: return fet_.process(l, r);
      case Vca: return vca_.process(l, r);
      default: return opto_.process(l, r);
    }
  }

  void reset() noexcept {
    fet_.reset();
    opto_.reset();
    vca_.reset();
  }

  int mode() const noexcept { return mode_; }

private:
  void update(const Controls& c) noexcept {
    const int next = std::clamp(c.compMode, 0, 2);
    mode_ = next;

    const int ratioStep = c.compRatio == 0 ? 3 : (c.compRatio == 1 ? 5 : 7);
    const float speed = clampf(c.compSpeed / 10.0f, 0.0f, 1.0f);

    CompControls cc;
    cc.ratio = static_cast<float>(ratioStep);
    cc.makeup = 0.5f;
    cc.output = 0.5f;
    cc.mix = 1.0f;
    cc.sidechainHpf = 0.0f;
    cc.link = 1.0f;
    cc.knee = 0.5f;

    if (mode_ == Fet) {
      cc.input = clampf(0.42f + 0.013f * c.compAmount, 0.30f, 0.70f);
      cc.attack = clampf(0.15f + 0.06f * speed, 0.0f, 1.0f);
      cc.release = clampf(0.30f + 0.055f * speed, 0.0f, 1.0f);
      fet_.setControls(cc);
    } else if (mode_ == Vca) {
      cc.threshold = clampf(0.85f - 0.07f * c.compAmount, 0.10f, 0.90f);
      cc.attack = clampf(0.15f + 0.06f * speed, 0.0f, 1.0f);
      cc.release = clampf(0.30f + 0.055f * speed, 0.0f, 1.0f);
      vca_.setControls(cc);
    } else {
      cc.threshold = clampf(c.compAmount / 10.0f, 0.0f, 1.0f);
      cc.ratio = 0.3f;
      opto_.setControls(cc);
    }
  }

  float sr_ = 48000.0f;
  int mode_ = Opto;
  FetLimiter fet_;
  OptoLeveler opto_;
  BusVca vca_;
};

} // namespace vocal
