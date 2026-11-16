// hearth-dsp — biquad.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

#include "envelope.hpp"
#include "util.hpp"

namespace hdsp {

// RBJ cookbook biquad for side-chain shaping and tone stages.  Coefficients
// are only recomputed when a control changes, never per sample.
struct Biquad {
  float b0 = 1.0f;
  float b1 = 0.0f;
  float b2 = 0.0f;
  float a1 = 0.0f;
  float a2 = 0.0f;

  float x1 = 0.0f;
  float x2 = 0.0f;
  float y1 = 0.0f;
  float y2 = 0.0f;

  void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }

  inline float process(float x) noexcept {
    const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1;
    x1 = x;
    y2 = y1;
    y1 = y;
    return y;
  }

  void lowShelf(float freq, float gainDb, float q, float sampleRate) noexcept {
    const float a = std::pow(10.0f, gainDb / 40.0f);
    const float w = omega(freq, sampleRate);
    const float cw = std::cos(w);
    const float alpha = std::sin(w) / (2.0f * std::max(q, 0.05f));
    const float twoSqrtAAlpha = 2.0f * std::sqrt(a) * alpha;

    const float b0n = a * ((a + 1.0f) - (a - 1.0f) * cw + twoSqrtAAlpha);
    const float b1n = 2.0f * a * ((a - 1.0f) - (a + 1.0f) * cw);
    const float b2n = a * ((a + 1.0f) - (a - 1.0f) * cw - twoSqrtAAlpha);
    const float a0n = (a + 1.0f) + (a - 1.0f) * cw + twoSqrtAAlpha;
    const float a1n = -2.0f * ((a - 1.0f) + (a + 1.0f) * cw);
    const float a2n = (a + 1.0f) + (a - 1.0f) * cw - twoSqrtAAlpha;
    set(b0n, b1n, b2n, a0n, a1n, a2n);
  }

  void highShelf(float freq, float gainDb, float q, float sampleRate) noexcept {
    const float a = std::pow(10.0f, gainDb / 40.0f);
    const float w = omega(freq, sampleRate);
    const float cw = std::cos(w);
    const float alpha = std::sin(w) / (2.0f * std::max(q, 0.05f));
    const float twoSqrtAAlpha = 2.0f * std::sqrt(a) * alpha;

    const float b0n = a * ((a + 1.0f) + (a - 1.0f) * cw + twoSqrtAAlpha);
    const float b1n = -2.0f * a * ((a - 1.0f) + (a + 1.0f) * cw);
    const float b2n = a * ((a + 1.0f) + (a - 1.0f) * cw - twoSqrtAAlpha);
    const float a0n = (a + 1.0f) - (a - 1.0f) * cw + twoSqrtAAlpha;
    const float a1n = 2.0f * ((a - 1.0f) - (a + 1.0f) * cw);
    const float a2n = (a + 1.0f) - (a - 1.0f) * cw - twoSqrtAAlpha;
    set(b0n, b1n, b2n, a0n, a1n, a2n);
  }

  void peak(float freq, float gainDb, float q, float sampleRate) noexcept {
    const float a = std::pow(10.0f, gainDb / 40.0f);
    const float w = omega(freq, sampleRate);
    const float cw = std::cos(w);
    const float alpha = std::sin(w) / (2.0f * std::max(q, 0.05f));

    const float b0n = 1.0f + alpha * a;
    const float b1n = -2.0f * cw;
    const float b2n = 1.0f - alpha * a;
    const float a0n = 1.0f + alpha / a;
    const float a1n = -2.0f * cw;
    const float a2n = 1.0f - alpha / a;
    set(b0n, b1n, b2n, a0n, a1n, a2n);
  }

  void lowPass(float freq, float q, float sampleRate) noexcept {
    const float w = omega(freq, sampleRate);
    const float cw = std::cos(w);
    const float alpha = std::sin(w) / (2.0f * std::max(q, 0.05f));
    const float b0n = (1.0f - cw) * 0.5f;
    const float b1n = 1.0f - cw;
    const float b2n = (1.0f - cw) * 0.5f;
    const float a0n = 1.0f + alpha;
    const float a1n = -2.0f * cw;
    const float a2n = 1.0f - alpha;
    set(b0n, b1n, b2n, a0n, a1n, a2n);
  }

  void highPass(float freq, float q, float sampleRate) noexcept {
    const float w = omega(freq, sampleRate);
    const float cw = std::cos(w);
    const float alpha = std::sin(w) / (2.0f * std::max(q, 0.05f));
    const float b0n = (1.0f + cw) * 0.5f;
    const float b1n = -(1.0f + cw);
    const float b2n = (1.0f + cw) * 0.5f;
    const float a0n = 1.0f + alpha;
    const float a1n = -2.0f * cw;
    const float a2n = 1.0f - alpha;
    set(b0n, b1n, b2n, a0n, a1n, a2n);
  }

private:
  static float omega(float freq, float sampleRate) noexcept {
    const float nyquist = 0.5f * sampleRate;
    freq = std::min(std::max(freq, 1.0f), nyquist * 0.98f);
    return 2.0f * kPi * freq / sampleRate;
  }

  void set(float b0n, float b1n, float b2n, float a0n, float a1n,
           float a2n) noexcept {
    const float inv = 1.0f / a0n;
    b0 = b0n * inv;
    b1 = b1n * inv;
    b2 = b2n * inv;
    a1 = a1n * inv;
    a2 = a2n * inv;
  }
};

} // namespace hdsp
