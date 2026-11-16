// hearth-dsp — util.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>

namespace hdsp {

constexpr float kPi = 3.14159265358979323846f;

inline float clampf(float x, float lo, float hi) noexcept {
  return std::min(std::max(x, lo), hi);
}

} // namespace hdsp
