// Vocal Suite — plate / room / hall reverb
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Eight-line feedback delay network with input diffusion, damping and slow
// modulation.  Plate, Room and Hall change the line lengths, damping and
// input diffusion; Size scales the network.  Wet output has a width control
// and a ducking envelope.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "common.hpp"
#include "controls.hpp"

namespace vocal {

class Space {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    maxSamples_ = std::max(64, static_cast<int>(sr_ * 0.25f) + 4);
    for (int i = 0; i < 8; ++i) {
      lineL_[i].buf.assign(static_cast<size_t>(maxSamples_), 0.0f);
      lineR_[i].buf.assign(static_cast<size_t>(maxSamples_), 0.0f);
      lineL_[i].write = 0;
      lineR_[i].write = 0;
    }
    preSize_ = std::max(16, static_cast<int>(sr_ * 0.16f) + 2);
    preL_.assign(static_cast<size_t>(preSize_), 0.0f);
    preR_.assign(static_cast<size_t>(preSize_), 0.0f);
    preWrite_ = 0;
    const float apMs[4] = {5.1f, 7.3f, 9.7f, 12.1f};
    for (int i = 0; i < 4; ++i) {
      apL_[i].prepare(static_cast<int>(sr_ * apMs[i] * 0.001f) + 1);
      apR_[i].prepare(static_cast<int>(sr_ * apMs[i] * 0.001f) + 1);
    }
    mix_.setTime(25.0f, sr_);
    decaySmooth_.setTime(40.0f, sr_);
    mix_.reset(0.0f);
    modPhase_ = 0.0f;
    modInc_ = 2.0f * kPi * 0.13f / sr_;
    duckEnv_ = 0.0f;
    reset();
    setControls(Controls{});
  }

  void setControls(const Controls& c) noexcept {
    on_ = c.spaceOn;
    mode_ = std::clamp(c.spaceMode, 0, 2);
    const float size = clampf(0.6f + 0.01f * c.spaceSize, 0.6f, 1.6f);
    const float rt60 = clampf(c.spaceDecay, 0.3f, 8.0f);

    static const float kPlate[8] = {29.7f, 37.1f, 41.1f, 43.7f,
                                    47.9f, 53.3f, 59.3f, 67.1f};
    static const float kRoom[8] = {17.3f, 19.7f, 23.1f, 27.7f,
                                   31.3f, 37.9f, 43.1f, 47.3f};
    static const float kHall[8] = {41.1f, 47.9f, 53.3f, 59.3f,
                                   67.1f, 73.7f, 83.9f, 97.1f};
    const float* base = mode_ == 1 ? kRoom : (mode_ == 2 ? kHall : kPlate);
    for (int i = 0; i < 8; ++i) {
      const float offset = (i & 1) ? 1.7f : 0.0f;
      const float ms = base[i] * size + offset;
      len_[i] = clampf(ms * 0.001f * sr_, 16.0f,
                       static_cast<float>(maxSamples_ - 3));
      const float lenSec = len_[i] / sr_;
      float g = std::pow(10.0f, -3.0f * lenSec / rt60);
      g = clampf(g, 0.15f, 0.985f);
      // Plate is the most energetic; room decays a little faster.
      if (mode_ == 1) {
        g *= 0.97f;
      }
      fb_[i] = g;
    }

    const float tone = clampf(c.spaceTone / 100.0f, 0.0f, 1.0f);
    const float dampHz = 2000.0f * std::pow(8.0f, tone);
    dampG_ = 1.0f - std::exp(-2.0f * kPi * clampf(dampHz, 300.0f, sr_ * 0.45f) /
                             sr_);
    preSamples_ = clampf(c.spacePreDelay * 0.001f * sr_, 0.0f,
                         static_cast<float>(preSize_ - 2));
    width_ = clampf(c.spaceWidth / 100.0f, 0.0f, 1.0f);
    duck_ = clampf(c.spaceDuck / 100.0f, 0.0f, 1.0f);
    mix_.setTarget(clampf(c.spaceMix / 100.0f, 0.0f, 1.0f));
    diffusion_ = mode_ == 1 ? 0.35f : (mode_ == 2 ? 0.6f : 0.5f);
  }

  void process(float& l, float& r) noexcept {
    if (!on_) {
      return;
    }
    const float dryL = l;
    const float dryR = r;
    const float in = 0.5f * (std::fabs(dryL) + std::fabs(dryR));
    const float atk = timeCoeff(5.0f, sr_);
    const float rel = timeCoeff(180.0f, sr_);
    duckEnv_ += (in - duckEnv_) * (in > duckEnv_ ? atk : rel);

    // Pre-delay.
    preL_[preWrite_] = dryL;
    preR_[preWrite_] = dryR;
    int read = preWrite_ - static_cast<int>(std::lround(preSamples_));
    if (read < 0) {
      read += preSize_;
    }
    float inL = preL_[read];
    float inR = preR_[read];
    preWrite_ = (preWrite_ + 1) % preSize_;

    for (int i = 0; i < 4; ++i) {
      inL = apL_[i].process(inL, diffusion_);
      inR = apR_[i].process(inR, diffusion_);
    }

    float dL[8], dR[8];
    for (int i = 0; i < 8; ++i) {
      const float mod =
          0.0007f * sr_ * std::sin(modPhase_ + static_cast<float>(i) * 0.7f);
      const float len = clampf(len_[i] + mod, 4.0f, static_cast<float>(maxSamples_ - 3));
      dL[i] = lineL_[i].read(len);
      dR[i] = lineR_[i].read(len);
      dampL_[i] += (dL[i] - dampL_[i]) * dampG_;
      dampR_[i] += (dR[i] - dampR_[i]) * dampG_;
      dL[i] = dampL_[i];
      dR[i] = dampR_[i];
    }
    modPhase_ += modInc_;
    if (modPhase_ > 2.0f * kPi) {
      modPhase_ -= 2.0f * kPi;
    }

    hadamard(dL);
    hadamard(dR);
    constexpr float kNorm = 0.35355339f;  // 1/sqrt(8)
    for (int i = 0; i < 8; ++i) {
      const float sign = (i & 1) ? -1.0f : 1.0f;
      const float injL = inL * 0.22f * sign;
      const float injR = inR * 0.22f * ((i & 2) ? -1.0f : 1.0f);
      lineL_[i].push(injL + dL[i] * kNorm * fb_[i]);
      lineR_[i].push(injR + dR[i] * kNorm * fb_[i]);
    }

    float wetL = 0.0f;
    float wetR = 0.0f;
    for (int i = 0; i < 8; ++i) {
      const float tap = (i & 1) ? -0.18f : 0.18f;
      wetL += dL[i] * tap;
      wetR += dR[i] * ((i & 2) ? -0.18f : 0.18f);
    }
    // Width: 0 = mono, 1 = normal stereo.
    const float mid = 0.5f * (wetL + wetR);
    const float side = 0.5f * (wetL - wetR) * width_;
    wetL = mid + side;
    wetR = mid - side;

    const float duckGain = 1.0f - duck_ * clampf(duckEnv_ * 8.0f, 0.0f, 1.0f);
    const float mix = mix_.next() * duckGain;
    l = dryL * (1.0f - mix) + wetL * mix;
    r = dryR * (1.0f - mix) + wetR * mix;
  }

  void reset() noexcept {
    for (int i = 0; i < 8; ++i) {
      std::fill(lineL_[i].buf.begin(), lineL_[i].buf.end(), 0.0f);
      std::fill(lineR_[i].buf.begin(), lineR_[i].buf.end(), 0.0f);
      lineL_[i].write = 0;
      lineR_[i].write = 0;
      dampL_[i] = 0.0f;
      dampR_[i] = 0.0f;
    }
    std::fill(preL_.begin(), preL_.end(), 0.0f);
    std::fill(preR_.begin(), preR_.end(), 0.0f);
    preWrite_ = 0;
    for (int i = 0; i < 4; ++i) {
      apL_[i].reset();
      apR_[i].reset();
    }
    duckEnv_ = 0.0f;
    modPhase_ = 0.0f;
  }

private:
  struct Line {
    std::vector<float> buf;
    int write = 0;

    float read(float delaySamples) const noexcept {
      const int size = static_cast<int>(buf.size());
      float pos = static_cast<float>(write) - delaySamples;
      while (pos < 0.0f) {
        pos += static_cast<float>(size);
      }
      const int i0 = static_cast<int>(pos) % size;
      const int i1 = (i0 + 1) % size;
      const float frac = pos - std::floor(pos);
      return buf[static_cast<size_t>(i0)] * (1.0f - frac) +
             buf[static_cast<size_t>(i1)] * frac;
    }

    void push(float x) noexcept {
      buf[static_cast<size_t>(write)] = x;
      write = (write + 1) % static_cast<int>(buf.size());
    }
  };

  struct Allpass {
    std::vector<float> buf;
    int write = 0;

    void prepare(int size) noexcept {
      buf.assign(static_cast<size_t>(std::max(2, size)), 0.0f);
      write = 0;
    }

    float process(float x, float g) noexcept {
      const int size = static_cast<int>(buf.size());
      const float delayed = buf[static_cast<size_t>(write)];
      const float y = -g * x + delayed;
      buf[static_cast<size_t>(write)] = x + g * y;
      write = (write + 1) % size;
      return y;
    }

    void reset() noexcept {
      std::fill(buf.begin(), buf.end(), 0.0f);
      write = 0;
    }
  };

  static void hadamard(float* d) noexcept {
    for (int len = 1; len < 8; len <<= 1) {
      for (int i = 0; i < 8; i += len << 1) {
        for (int j = 0; j < len; ++j) {
          const float a = d[i + j];
          const float b = d[i + j + len];
          d[i + j] = a + b;
          d[i + j + len] = a - b;
        }
      }
    }
  }

  float sr_ = 48000.0f;
  int maxSamples_ = 12000;
  int preSize_ = 0;
  int preWrite_ = 0;
  float preSamples_ = 0.0f;
  Line lineL_[8], lineR_[8];
  Allpass apL_[4], apR_[4];
  std::vector<float> preL_, preR_;
  float len_[8] = {};
  float fb_[8] = {};
  float dampL_[8] = {};
  float dampR_[8] = {};
  float dampG_ = 0.3f;
  float width_ = 0.7f;
  float duck_ = 0.4f;
  float diffusion_ = 0.5f;
  float modPhase_ = 0.0f;
  float modInc_ = 0.0f;
  float duckEnv_ = 0.0f;
  Smoothed mix_;
  Smoothed decaySmooth_;
  bool on_ = false;
  int mode_ = 0;
};

} // namespace vocal
