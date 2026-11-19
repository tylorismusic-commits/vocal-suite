// Vocal Suite — doubler / stereo width
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "common.hpp"
#include "controls.hpp"

namespace vocal {

class Doubler {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    size_ = std::max(64, static_cast<int>(sr_ * 0.06f));
    bufL_.assign(static_cast<size_t>(size_), 0.0f);
    bufR_.assign(static_cast<size_t>(size_), 0.0f);
    reset();
    setControls(Controls{});
  }

  void setControls(const Controls& c) noexcept {
    on_ = c.doubleOn;
    amount_ = clampf(c.doubleAmount / 100.0f, 0.0f, 1.0f);
    delayMs_ = clampf(c.doubleDelayMs, 8.0f, 40.0f);
    width_ = clampf(c.doubleWidth / 100.0f, 0.0f, 2.0f);
    mix_ = clampf(c.doubleMix / 100.0f, 0.0f, 1.0f);
  }

  void process(float& l, float& r) noexcept {
    if (!on_) {
      return;
    }
    const float dryL = l;
    const float dryR = r;
    bufL_[write_] = l;
    bufR_[write_] = r;
    const float base = delayMs_ * 0.001f * sr_;
    const float mod = 0.0015f * sr_ * std::sin(phase_);
    const float d1 = base + mod;
    const float d2 = base - mod;
    const float a = read(bufL_, d1);
    const float b = read(bufL_, d2);
    const float c = read(bufR_, d1);
    const float d = read(bufR_, d2);
    float wetL = 0.5f * (a + d);
    float wetR = 0.5f * (c + b);
    const float mid = 0.5f * (wetL + wetR);
    const float side = 0.5f * (wetL - wetR) * width_;
    wetL = (mid + side) * amount_;
    wetR = (mid - side) * amount_;
    l = dryL * (1.0f - mix_) + wetL * mix_;
    r = dryR * (1.0f - mix_) + wetR * mix_;
    write_ = (write_ + 1) % size_;
    phase_ += 2.0f * kPi * 0.17f / sr_;
    if (phase_ > 2.0f * kPi) phase_ -= 2.0f * kPi;
  }

  void reset() noexcept {
    std::fill(bufL_.begin(), bufL_.end(), 0.0f);
    std::fill(bufR_.begin(), bufR_.end(), 0.0f);
    write_ = 0;
    phase_ = 0.0f;
  }

private:
  float read(const std::vector<float>& buf, float samples) const noexcept {
    int delay = static_cast<int>(std::lround(samples));
    delay = std::clamp(delay, 1, size_ - 2);
    int index = write_ - delay;
    while (index < 0) index += size_;
    return buf[static_cast<size_t>(index)];
  }

  float sr_ = 48000.0f;
  bool on_ = false;
  float amount_ = 0.45f;
  float delayMs_ = 18.0f;
  float width_ = 1.3f;
  float mix_ = 0.35f;
  int size_ = 2880;
  int write_ = 0;
  float phase_ = 0.0f;
  std::vector<float> bufL_;
  std::vector<float> bufR_;
};

} // namespace vocal
