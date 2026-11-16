// hearth-dsp — truepeak.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

namespace hdsp {

// Four-times oversampled peak detector using Catmull-Rom interpolation.
// Detector only: the audio path stays at the host sample rate and the result
// drives a limiter's gain.
class TruePeakDetector {
public:
  void reset() noexcept {
    history_[0] = history_[1] = history_[2] = history_[3] = 0.0f;
    index_ = 0;
  }

  inline float process(float x) noexcept {
    history_[index_] = x;
    const int i0 = (index_ + 3) & 3;
    const int i1 = (index_ + 2) & 3;
    const int i2 = (index_ + 1) & 3;
    const int i3 = index_;
    index_ = (index_ + 1) & 3;

    const float p0 = history_[i0];
    const float p1 = history_[i1];
    const float p2 = history_[i2];
    const float p3 = history_[i3];

    float peak = std::max(std::fabs(p2), std::fabs(p3));
    for (int step = 1; step <= 4; ++step) {
      const float t = 0.25f * static_cast<float>(step);
      const float t2 = t * t;
      const float t3 = t2 * t;
      const float v = 0.5f * ((2.0f * p1) + (-p0 + p2) * t +
                              (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                              (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
      peak = std::max(peak, std::fabs(v));
    }
    return peak;
  }

private:
  float history_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  int index_ = 0;
};

} // namespace hdsp
