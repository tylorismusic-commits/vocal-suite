// hearth-dsp — db.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

#include "util.hpp"

namespace hdsp {

inline float dbToGain(float db) noexcept {
  return std::pow(10.0f, db * 0.05f);
}

// Clamped at -240 dB so silence has a finite, printable value.
inline float gainToDb(float gain) noexcept {
  return 20.0f * std::log10(std::max(gain, 1.0e-12f));
}

// One-pole smoothing coefficient for a time constant in milliseconds.
// The returned value is the retention factor; use (1 - coeff) as the update
// weight when writing s += (target - s) * (1 - coeff).
inline float timeCoeff(float ms, float sampleRate) noexcept {
  const float samples = std::max(1.0f, ms * 0.001f * sampleRate);
  return std::exp(-1.0f / samples);
}

} // namespace hdsp
