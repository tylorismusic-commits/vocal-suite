// Vocal Suite — two-band de-esser
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Each band is a dynamic peaking cut.  A band-pass detector follows the
// sibilance or harshness region and the cut gain is smoothed before it is
// applied, so the band never chatters on a sustained note.
#pragma once

#include <algorithm>
#include <cmath>

#include <hdsp/biquad.hpp>

#include "common.hpp"
#include "controls.hpp"

namespace vocal {

class DeEss {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    reset();
    setControls(Controls{});
  }

  void setControls(const Controls& c) noexcept {
    sibAmount_ = clampf(c.sibAmount / 10.0f, 0.0f, 1.0f);
    harshAmount_ = clampf(c.harshAmount / 10.0f, 0.0f, 1.0f);
    sibFreq_ = c.sibFreq;
    harshFreq_ = c.harshFreq;
    sibDet_.set(sibFreq_, 0.55f, sr_);
    harshDet_.set(harshFreq_, 0.55f, sr_);
  }

  void process(float& l, float& r, float& sibGr, float& harshGr) noexcept {
    sibGr = 0.0f;
    harshGr = 0.0f;
    if (sibAmount_ <= 0.0001f && harshAmount_ <= 0.0001f) {
      return;
    }
    const float det = 0.5f * (l + r);
    sibGr = processBand(bandSib_, sibDet_, det, l, r, sibAmount_, sibFreq_);
    harshGr = processBand(bandHarsh_, harshDet_, det, l, r, harshAmount_,
                          harshFreq_);
  }

  void reset() noexcept {
    bandSib_.reset();
    bandHarsh_.reset();
    sibDet_.reset();
    harshDet_.reset();
  }

private:
  struct SvfBp {
    float ic1 = 0.0f;
    float ic2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;
    float k = 1.0f;

    void set(float freq, float q, float sr) noexcept {
      freq = clampf(freq, 20.0f, sr * 0.45f);
      const float g = std::tan(kPi * freq / sr);
      k = 1.0f / std::max(q, 0.1f);
      a1 = 1.0f / (1.0f + g * (g + k));
      a2 = g * a1;
      a3 = g * a2;
    }

    float process(float x) noexcept {
      const float v3 = x - ic2;
      const float v1 = a1 * ic1 + a2 * v3;
      const float v2 = ic2 + a2 * ic1 + a3 * v3;
      ic1 = 2.0f * v1 - ic1;
      ic2 = 2.0f * v2 - ic2;
      return k * v1;
    }

    void reset() noexcept {
      ic1 = 0.0f;
      ic2 = 0.0f;
    }
  };

  struct Band {
    hdsp::Biquad cut[2];
    float env = 0.0f;
    float grDb = 0.0f;
    int counter = 0;

    void reset() noexcept {
      cut[0].reset();
      cut[1].reset();
      env = 0.0f;
      grDb = 0.0f;
      counter = 0;
    }
  };

  float processBand(Band& band, SvfBp& det, float detect, float& l, float& r,
                    float amount, float freq) noexcept {
    if (amount <= 0.0001f) {
      return 0.0f;
    }
    const float bp = std::fabs(det.process(detect));
    const float attack = timeCoeff(1.0f, sr_);
    const float release = timeCoeff(60.0f, sr_);
    band.env += (bp - band.env) * (bp > band.env ? attack : release);

    const float thrDb = -40.0f + 1.4f * (amount * 10.0f);
    const float overDb = gainToDb(band.env) - thrDb;
    const float maxGr = 12.0f * amount;
    const float target = clampf(overDb * 0.8f, 0.0f, maxGr);

    const float smooth = timeCoeff(5.0f, sr_);
    band.grDb += (target - band.grDb) * smooth;
    if (band.grDb < 0.0005f) {
      band.grDb = 0.0f;
    }

    if (++band.counter >= 16) {
      band.counter = 0;
      band.cut[0].peak(freq, -band.grDb, 1.0f, sr_);
      band.cut[1].peak(freq, -band.grDb, 1.0f, sr_);
    }
    l = band.cut[0].process(l);
    r = band.cut[1].process(r);
    return band.grDb;
  }

  float sr_ = 48000.0f;
  float sibAmount_ = 0.0f;
  float harshAmount_ = 0.0f;
  float sibFreq_ = 6500.0f;
  float harshFreq_ = 3000.0f;
  Band bandSib_;
  Band bandHarsh_;
  SvfBp sibDet_;
  SvfBp harshDet_;
};

} // namespace vocal
