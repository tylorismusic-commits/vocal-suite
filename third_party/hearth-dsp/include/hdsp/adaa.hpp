// hearth-dsp — adaa.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

namespace hdsp {

// Antiderivative-based anti-aliasing (first-order ADAA) for a memoryless
// waveshaper y = f(x), where F is an antiderivative of f.
//
// The finite difference is evaluated in double precision: near a signal peak
// the cancellation error in float32 is large enough to produce audible spikes.
// When dx is very small the midpoint value f((x+x1)/2) is used instead; the
// error is O(dx^2), far below the noise floor.
struct Adaa {
  float x1 = 0.0f;
  double f1 = 0.0; // antiderivative at x1, intentionally double

  void reset() noexcept {
    x1 = 0.0f;
    f1 = 0.0;
  }

  template <typename Shaper, typename Antider>
  float process(float x, Shaper f, Antider F) noexcept {
    const double dx = static_cast<double>(x) - static_cast<double>(x1);
    const double F0 = F(static_cast<double>(x));
    const double y =
        std::fabs(dx) < 1.0e-4 ? f(0.5 * (x + x1)) : (F0 - f1) / dx;
    f1 = F0;
    x1 = x;
    return static_cast<float>(y);
  }
};

// Numerically stable log(cosh(x)), an antiderivative of tanh(x).
inline double logCosh(double x) noexcept {
  const double ax = std::fabs(x);
  return ax + std::log1p(std::exp(-2.0 * ax)) - 0.6931471805599453;
}

// Asymmetric soft clipper, normalised to unity slope at the origin, with ADAA.
// k sets drive, bias sets the asymmetry (even harmonics).
class AsymShaper {
public:
  void reset() noexcept {
    adaa_.reset();
    primed_ = false;
  }

  float process(float x, float k, float bias) noexcept {
    const double kd = k;
    const double bd = bias;
    const double tb = std::tanh(bd);
    const float norm = static_cast<float>(kd * (1.0 - tb * tb));
    // k = 0 (or a bias so extreme the normalisation collapses) is a bypass,
    // not a divide by zero.
    if (std::fabs(norm) < 1.0e-6f) {
      return x;
    }
    auto f = [kd, bd, tb](double v) { return std::tanh(kd * v + bd) - tb; };
    auto F = [kd, bd, tb](double v) {
      return logCosh(kd * v + bd) / kd - tb * v;
    };

    // ADAA stores F(x1) from the previous sample, computed with the drive in
    // force then.  Recompute it whenever the drive moves, or a peak can turn
    // the mismatch into a burst.
    if (k != lastK_ || bias != lastBias_) {
      if (primed_) {
        adaa_.f1 = F(static_cast<double>(adaa_.x1));
      }
      lastK_ = k;
      lastBias_ = bias;
    }
    // Prime on the first sample: F(0) is not 0 for a biased shaper.
    if (!primed_) {
      primed_ = true;
      adaa_.x1 = x;
      adaa_.f1 = F(static_cast<double>(x));
      return static_cast<float>(f(static_cast<double>(x)) / norm);
    }
    return adaa_.process(x, f, F) / norm;
  }

private:
  Adaa adaa_;
  bool primed_ = false;
  float lastK_ = -1.0f;
  float lastBias_ = 0.0f;
};

} // namespace hdsp
