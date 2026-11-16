// Vocal Suite — drive / saturation
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Single-band ADAA saturation adapted from Patina.  Four voicings: Valve,
// Console, Tape and Clip.  Auto gain keeps the module level-neutral.
#pragma once

#include <algorithm>
#include <cmath>

#include <hdsp/adaa.hpp>
#include <hdsp/biquad.hpp>

#include "common.hpp"
#include "controls.hpp"

namespace vocal {

inline float driveShape(int model, float drive, float character, float x) {
  const float k = 1.0f + 5.0f * clampf(drive, 0.0f, 1.0f);
  float bias = 0.10f + 0.20f * character;
  switch (model) {
    case 1: bias = 0.04f + 0.10f * character; break;  // Console
    case 2: bias = 0.06f + 0.12f * character; break;  // Tape
    case 3: bias = 0.20f + 0.30f * character; break;  // Clip
    default: break;                                    // Valve
  }
  const float tb = std::tanh(bias);
  const float norm = std::max(k * (1.0f - tb * tb), 1.0e-6f);
  if (model == 3) {
    // Harder knee for the Clip voice.
    return (std::tanh(1.6f * k * x + bias) - tb) / (1.6f * norm);
  }
  return (std::tanh(k * x + bias) - tb) / norm;
}

class Drive {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    drive_.setTime(15.0f, sr_);
    mix_.setTime(10.0f, sr_);
    autoGain_.setTime(80.0f, sr_);
    drive_.reset(1.0f);
    mix_.reset(0.0f);
    autoGain_.reset(1.0f);
    inEnv_ = 0.0f;
    outEnv_ = 0.0f;
    reset();
    setControls(Controls{});
  }

  void setControls(const Controls& c) noexcept {
    model_ = std::clamp(c.driveModel, 0, 3);
    amount_ = clampf(c.driveAmount / 10.0f, 0.0f, 1.0f);
    character_ = clampf(c.driveChar / 10.0f, 0.0f, 1.0f);
    drive_.setTarget(dbToGain(-12.0f + 24.0f * amount_));
    mix_.setTarget(clampf(c.driveMix / 100.0f, 0.0f, 1.0f));
    const float tone = character_;
    for (int ch = 0; ch < 2; ++ch) {
      float preDb = 0.0f;
      float preHz = 160.0f;
      switch (model_) {
        case 1: preHz = 150.0f; preDb = 1.5f * tone; break;
        case 2: preHz = 90.0f; preDb = 4.0f * tone; break;
        case 3: preHz = 120.0f; preDb = 2.0f * tone; break;
        default: preDb = 1.2f * tone; break;
      }
      pre_[ch].lowShelf(preHz, preDb, 0.7f, sr_);
      high_[ch].highShelf(3500.0f + 1500.0f * tone, (tone - 0.5f) * 8.0f,
                          0.7f, sr_);
      if (model_ == 2) {
        lp_[ch].lowPass(12000.0f + 4000.0f * tone, 0.7071f, sr_);
      } else {
        lp_[ch].lowPass(21000.0f, 0.7071f, sr_);
      }
      hp_[ch].highPass(25.0f, 0.7071f, sr_);
    }
  }

  void process(float& l, float& r) noexcept {
    if (amount_ <= 0.0001f) {
      return;
    }
    const float dryL = l;
    const float dryR = r;
    const float drive = drive_.next();
    const float k = 1.0f + 5.0f * amount_;

    float xL = pre_[0].process(dryL * drive);
    float xR = pre_[1].process(dryR * drive);
    xL = shaper_[0].process(xL, k, biasFor(0));
    xR = shaper_[1].process(xR, k, biasFor(1));
    xL = hp_[0].process(lp_[0].process(high_[0].process(xL)));
    xR = hp_[1].process(lp_[1].process(high_[1].process(xR)));

    // Level-neutral: compare slow RMS before and after the shaper.
    inEnv_ += (0.5f * (dryL * dryL + dryR * dryR) - inEnv_) * 0.0004f;
    outEnv_ += (0.5f * (xL * xL + xR * xR) - outEnv_) * 0.0004f;
    const float rmsRatio =
        (outEnv_ > 1.0e-8f) ? std::sqrt(inEnv_ / outEnv_) : 1.0f;
    autoGain_.setTarget(clampf(rmsRatio, 0.25f, 4.0f));
    const float ag = autoGain_.next();

    const float mix = mix_.next();
    l = dryL + mix * (xL * ag - dryL);
    r = dryR + mix * (xR * ag - dryR);
  }

  void reset() noexcept {
    for (int ch = 0; ch < 2; ++ch) {
      shaper_[ch].reset();
      pre_[ch].reset();
      high_[ch].reset();
      lp_[ch].reset();
      hp_[ch].reset();
    }
    inEnv_ = 0.0f;
    outEnv_ = 0.0f;
  }

private:
  float biasFor(int ch) const noexcept {
    switch (model_) {
      case 1:
        return 0.04f + 0.10f * character_ +
               (ch == 0 ? 0.02f : -0.02f) * character_;
      case 2: return 0.06f + 0.12f * character_;
      case 3: return 0.20f + 0.30f * character_;
      default: return 0.10f + 0.20f * character_;
    }
  }

  float sr_ = 48000.0f;
  int model_ = 0;
  float amount_ = 0.0f;
  float character_ = 0.5f;
  Smoothed drive_, mix_, autoGain_;
  hdsp::AsymShaper shaper_[2];
  hdsp::Biquad pre_[2], high_[2], lp_[2], hp_[2];
  float inEnv_ = 0.0f;
  float outEnv_ = 0.0f;
};

} // namespace vocal
