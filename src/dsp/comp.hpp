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
    previous_ = Opto;
    fade_ = 1.0f;
    fadeInc_ = 1.0f / (0.02f * sr_);
    update(Controls{});
    reset();
  }

  void setControls(const Controls& c) noexcept {
    update(c);
  }

  // Returns gain reduction in dB (>= 0).
  float process(float& l, float& r) noexcept {
    if (fade_ < 1.0f) {
      float oldL = l;
      float oldR = r;
      const float oldGr = processMode(previous_, oldL, oldR);
      const float newGr = processMode(mode_, l, r);
      const float t = fade_;
      l = oldL + (l - oldL) * t;
      r = oldR + (r - oldR) * t;
      fade_ = std::min(1.0f, fade_ + fadeInc_);
      return std::max(oldGr, newGr);
    }
    return processMode(mode_, l, r);
  }

  void reset() noexcept {
    fet_.reset();
    opto_.reset();
    vca_.reset();
    fade_ = 1.0f;
  }

  int mode() const noexcept { return mode_; }

private:
  float processMode(int mode, float& l, float& r) noexcept {
    switch (mode) {
      case Fet: return fet_.process(l, r);
      case Vca: return vca_.process(l, r);
      default: return opto_.process(l, r);
    }
  }

  void update(const Controls& c) noexcept {
    const int next = std::clamp(c.compMode, 0, 2);
    if (next != mode_) {
      previous_ = mode_;
      mode_ = next;
      fade_ = 0.0f;
    }

    const int ratioStep = c.compRatio == 0 ? 3 : (c.compRatio == 1 ? 5 : 7);
    const float speed = clampf(c.compSpeed / 10.0f, 0.0f, 1.0f);

    CompControls base;
    base.makeup = 0.5f;
    base.output = 0.5f;
    base.mix = 1.0f;
    base.sidechainHpf = 0.0f;
    base.link = 1.0f;
    base.knee = 0.5f;

    CompControls fet = base;
    fet.ratio = static_cast<float>(ratioStep);
    fet.input = clampf(0.42f + 0.013f * c.compAmount, 0.30f, 0.70f);
    fet.attack = clampf(0.15f + 0.06f * speed, 0.0f, 1.0f);
    fet.release = clampf(0.30f + 0.055f * speed, 0.0f, 1.0f);
    fet_.setControls(fet);

    CompControls vca = base;
    vca.ratio = static_cast<float>(ratioStep);
    vca.threshold = clampf(0.85f - 0.07f * c.compAmount, 0.10f, 0.90f);
    vca.attack = clampf(0.15f + 0.06f * speed, 0.0f, 1.0f);
    vca.release = clampf(0.30f + 0.055f * speed, 0.0f, 1.0f);
    vca_.setControls(vca);

    CompControls opto = base;
    opto.threshold = clampf(c.compAmount / 10.0f, 0.0f, 1.0f);
    opto.ratio = 0.3f;
    opto_.setControls(opto);
  }

  float sr_ = 48000.0f;
  int mode_ = Opto;
  int previous_ = Opto;
  float fade_ = 1.0f;
  float fadeInc_ = 1.0f;
  FetLimiter fet_;
  OptoLeveler opto_;
  BusVca vca_;
};

} // namespace vocal
