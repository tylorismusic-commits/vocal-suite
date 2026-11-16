// Vocal Suite — geometry.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "ui/panel.hpp"

namespace vocal {

// The editor is laid out once in logical units (kPanelWidth x kPanelHeight)
// and scaled uniformly.  Scales move in steps of 1/80, because 1040 = 13 * 80
// and 640 = 8 * 80: every step gives an exact 1040:640 window, with no rounding
// drift between width and height.
constexpr int kScaleSteps = 80;
constexpr int kMinScaleStep = 40;   // 0.5x
constexpr int kMaxScaleStep = 200;  // 2.5x
constexpr double kMinUiScale = static_cast<double>(kMinScaleStep) / kScaleSteps;
constexpr double kMaxUiScale = static_cast<double>(kMaxScaleStep) / kScaleSteps;

struct UiSize {
  uint32_t width;
  uint32_t height;
};

inline int scaleStep(double s) {
  const long step = std::lround(s * kScaleSteps);
  return static_cast<int>(std::min<long>(std::max<long>(step, kMinScaleStep), kMaxScaleStep));
}

// Quantises a scale to the nearest allowed step.
inline double clampScale(double s) {
  return static_cast<double>(scaleStep(s)) / kScaleSteps;
}

// Returns the closest valid size to a requested one.  The dimension that
// moved furthest from nominal decides the scale, so dragging either edge
// feels natural, and the result always keeps the exact aspect ratio.
inline double scaleForRequest(uint32_t width, uint32_t height) {
  const double sw = static_cast<double>(width) / kPanelWidth;
  const double sh = static_cast<double>(height) / kPanelHeight;
  const double s = std::fabs(sw - 1.0) >= std::fabs(sh - 1.0) ? sw : sh;
  return clampScale(s);
}

inline UiSize sizeForScale(double scale) {
  const int step = scaleStep(scale);
  return {static_cast<uint32_t>(kPanelWidth / kScaleSteps * step),
          static_cast<uint32_t>(kPanelHeight / kScaleSteps * step)};
}

inline UiSize snapSize(uint32_t width, uint32_t height) {
  return sizeForScale(scaleForRequest(width, height));
}

} // namespace vocal
