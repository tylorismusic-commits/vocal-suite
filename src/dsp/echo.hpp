// Vocal Suite — ducked stereo echo
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Adapted from Amp Suite's tape delay.  Adds vocal-specific ducking, width
// cross-feed, LoFi feedback filtering and a 4 s buffer.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "common.hpp"
#include "controls.hpp"

namespace vocal {

inline float echoDivisionBeats(int index) noexcept {
  static const float kBeats[10] = {4.0f,  2.0f,  1.0f,  0.75f, 0.5f,
                                   0.333f, 0.375f, 0.25f, 0.166f, 0.125f};
  return kBeats[std::clamp(index, 0, 9)];
}

class Echo {
public:
  void prepare(float sampleRate, float maxMs = 4000.0f) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    maxSamples_ = std::max(64, static_cast<int>(sr_ * maxMs * 0.001f) + 4);
    bufferL_.assign(static_cast<size_t>(maxSamples_), 0.0f);
    bufferR_.assign(static_cast<size_t>(maxSamples_), 0.0f);
    write_ = 0;
    time_.setTime(100.0f, sr_);
    feedback_.setTime(20.0f, sr_);
    mix_.setTime(20.0f, sr_);
    width_.setTime(20.0f, sr_);
    duck_.setTime(20.0f, sr_);
    time_.reset(0.25f * sr_);
    feedback_.reset(0.3f);
    mix_.reset(0.0f);
    width_.reset(0.5f);
    duck_.reset(0.6f);
    toneL_.set(4000.0f, sr_);
    toneR_.set(4000.0f, sr_);
    loHiL_.set(300.0f, sr_);
    loHiR_.set(300.0f, sr_);
    modPhase_ = 0.0f;
    modInc_ = 2.0f * kPi * 0.3f / sr_;
    duckEnv_ = 0.0f;
    reset();
  }

  void setControls(const Controls& c) noexcept {
    on_ = c.echoOn;
    const float beats = echoDivisionBeats(c.echoDiv);
    float ms;
    if (c.echoSync) {
      const float tempo = (std::isfinite(c.bpm) && c.bpm > 1.0f) ? c.bpm : 120.0f;
      ms = 60000.0f / tempo * beats;
    } else {
      ms = c.echoTime;
    }
    ms = clampf(ms, 1.0f, static_cast<float>(maxSamples_ - 3) * 1000.0f / sr_);
    time_.setTarget(ms * 0.001f * sr_);
    feedback_.setTarget(clampf(c.echoFeedback / 100.0f, 0.0f, 0.95f));
    mix_.setTarget(clampf(c.echoMix / 100.0f, 0.0f, 1.0f));
    width_.setTarget(clampf(c.echoWidth / 100.0f, 0.0f, 1.0f));
    duck_.setTarget(clampf(c.echoDuck / 100.0f, 0.0f, 1.0f));
    const float toneHz = 500.0f * std::pow(32.0f, clampf(c.echoTone / 100.0f, 0.0f, 1.0f));
    toneL_.set(toneHz, sr_);
    toneR_.set(toneHz, sr_);
    lofi_ = c.echoLofi;
    modDepth_ = clampf(c.echoMod / 100.0f, 0.0f, 1.0f) * 0.003f * sr_;
  }

  void process(float& l, float& r) noexcept {
    if (!on_) {
      return;
    }
    const float dryL = l;
    const float dryR = r;
    const float in = 0.5f * (std::fabs(dryL) + std::fabs(dryR));
    const float atk = timeCoeff(5.0f, sr_);
    const float rel = timeCoeff(150.0f, sr_);
    duckEnv_ += (in - duckEnv_) * (in > duckEnv_ ? atk : rel);

    float d = time_.next();
    if (modDepth_ > 0.0f) {
      d += modDepth_ * std::sin(modPhase_);
      modPhase_ += modInc_;
      if (modPhase_ > 2.0f * kPi) {
        modPhase_ -= 2.0f * kPi;
      }
    }
    d = clampf(d, 1.0f, static_cast<float>(maxSamples_ - 2));

    float wetL = toneL_.process(read(bufferL_, d));
    float wetR = toneR_.process(read(bufferR_, d));
    if (lofi_) {
      wetL = loHiL_.process(wetL);
      wetR = loHiR_.process(wetR);
    }

    const float fb = feedback_.next();
    const float width = width_.next();
    float fbL = wetL * fb;
    float fbR = wetR * fb;
    // Width 0: mono repeats.  Width 1: full cross-feedback (ping-pong).
    const float xL = fbL * (1.0f - width) + fbR * width;
    const float xR = fbR * (1.0f - width) + fbL * width;
    bufferL_[write_] = dryL + xL;
    bufferR_[write_] = dryR + xR;
    write_ = (write_ + 1) % maxSamples_;

    // Width 0 collapses the repeats to mono; width 1 keeps the full
    // cross-fed stereo image.
    const float widthOut = width_.next();
    const float mid = 0.5f * (wetL + wetR);
    wetL = mid + (wetL - mid) * widthOut;
    wetR = mid + (wetR - mid) * widthOut;

    const float duck = duck_.next();
    const float duckGain = 1.0f - duck * clampf(duckEnv_ * 8.0f, 0.0f, 1.0f);
    const float mix = mix_.next() * duckGain;
    l = dryL * (1.0f - mix) + wetL * mix;
    r = dryR * (1.0f - mix) + wetR * mix;
  }

  void reset() noexcept {
    std::fill(bufferL_.begin(), bufferL_.end(), 0.0f);
    std::fill(bufferR_.begin(), bufferR_.end(), 0.0f);
    write_ = 0;
    modPhase_ = 0.0f;
    duckEnv_ = 0.0f;
    toneL_.reset();
    toneR_.reset();
    loHiL_.reset();
    loHiR_.reset();
  }

private:
  float read(const std::vector<float>& buf, float delaySamples) const noexcept {
    float pos = static_cast<float>(write_) - delaySamples;
    while (pos < 0.0f) {
      pos += static_cast<float>(maxSamples_);
    }
    const int i0 = static_cast<int>(pos) % maxSamples_;
    const int i1 = (i0 + 1) % maxSamples_;
    const float frac = pos - std::floor(pos);
    return buf[static_cast<size_t>(i0)] * (1.0f - frac) +
           buf[static_cast<size_t>(i1)] * frac;
  }

  float sr_ = 48000.0f;
  int maxSamples_ = 192000;
  int write_ = 0;
  std::vector<float> bufferL_;
  std::vector<float> bufferR_;
  Smoothed time_, feedback_, mix_, width_, duck_;
  OnePoleLP toneL_, toneR_, loHiL_, loHiR_;
  float modPhase_ = 0.0f;
  float modInc_ = 0.0f;
  float modDepth_ = 0.0f;
  float duckEnv_ = 0.0f;
  bool on_ = false;
  bool lofi_ = false;
};

} // namespace vocal
