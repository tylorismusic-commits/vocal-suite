// hearth-dsp — dynamics.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

namespace hdsp {

// One shared ratio ladder for every model with a selectable ratio, so the same
// control position means the same ratio when switching models.  Step 8 is the
// 76 "all buttons" position; other models treat it as 20:1.
inline int ratioStepFrom(float value) noexcept {
  return std::min(std::max(static_cast<int>(std::lround(value)), 0), 8);
}

inline float ratioFromStep(int step) noexcept {
  static const float kRatios[9] = {1.5f, 2.0f, 3.0f, 4.0f, 6.0f,
                                   8.0f, 12.0f, 20.0f, 20.0f};
  return kRatios[std::min(std::max(step, 0), 8)];
}

// Static gain computer with an explicit slope (dB of reduction per dB over
// threshold).  Feed-forward models pass 1 - 1/ratio; feedback models need
// ratio - 1, because the loop multiplies the static curve by 1/(1 + slope).
//
// maxDb is a soft ceiling: real control laws saturate, and without it a
// feedback model's 6 dB transient can ask for >100 dB of reduction and round
// the gain to zero.
inline float gainReductionDbSlope(float levelDb, float thresholdDb,
                                  float slope, float kneeDb,
                                  float maxDb = 40.0f) noexcept {
  const float over = levelDb - thresholdDb;
  float gr = 0.0f;
  if (kneeDb <= 0.0f || over > 0.5f * kneeDb) {
    gr = std::max(slope * over, 0.0f);
  } else if (over > -0.5f * kneeDb) {
    const float t = over + 0.5f * kneeDb;
    gr = std::max(slope * t * t / (2.0f * kneeDb), 0.0f);
  }

  const float ceilingStart = 0.7f * maxDb;
  if (gr > ceilingStart) {
    const float t = (gr - ceilingStart) / (maxDb - ceilingStart);
    gr = ceilingStart + (maxDb - ceilingStart) * (t / (1.0f + t));
  }
  return gr;
}

// Static gain computer: gain reduction in dB (>= 0) for a level in dB, with a
// quadratic soft knee centred on the threshold.
inline float gainReductionDb(float levelDb, float thresholdDb, float ratio,
                             float kneeDb) noexcept {
  const float slope = 1.0f - 1.0f / std::max(ratio, 1.0f);
  return gainReductionDbSlope(levelDb, thresholdDb, slope, kneeDb);
}

} // namespace hdsp
