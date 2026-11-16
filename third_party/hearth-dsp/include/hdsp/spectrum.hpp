// hearth-dsp — spectrum.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "db.hpp"

namespace hdsp {

// Real-time FFT spectrum analyzer for editor meters.  Audio-thread safe: no
// allocation and no locks.  It keeps a 2048-sample window, analyses the most
// recent block every 1024 samples, and maps the FFT to 128 log-spaced bins
// between 20 Hz and 20 kHz.
class SpectrumAnalyzer {
public:
  static constexpr int kFftSize = 2048;
  static constexpr int kBins = 128;
  static constexpr int kHop = 1024;
  static constexpr float kMinHz = 20.0f;
  static constexpr float kMaxHz = 20000.0f;

  void prepare(float sampleRate) noexcept {
    sampleRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    for (int i = 0; i < kFftSize; ++i) {
      window_[i] = 0.5f - 0.5f * std::cos(2.0f * kPi * i / (kFftSize - 1));
      // Bit-reversal permutation table.
      uint16_t x = static_cast<uint16_t>(i);
      uint16_t r = 0;
      for (int bit = 0; bit < 11; ++bit) {
        r = static_cast<uint16_t>((r << 1) | (x & 1u));
        x = static_cast<uint16_t>(x >> 1);
      }
      reversed_[i] = r;
    }
    for (int i = 0; i < kFftSize / 2; ++i) {
      const float a = -2.0f * kPi * i / kFftSize;
      twiddleCos_[i] = std::cos(a);
      twiddleSin_[i] = std::sin(a);
    }
    for (int i = 0; i < kBins; ++i) {
      const float f0 = binFrequency(i);
      const float f1 = binFrequency(i + 1);
      int lo = static_cast<int>(f0 * kFftSize / sampleRate_);
      int hi = static_cast<int>(f1 * kFftSize / sampleRate_);
      lo = std::clamp(lo, 1, kFftSize / 2 - 1);
      hi = std::clamp(hi, lo + 1, kFftSize / 2);
      binLo_[i] = lo;
      binHi_[i] = hi;
    }
    reset();
  }

  void reset() noexcept {
    std::fill(ring_, ring_ + kFftSize, 0.0f);
    std::fill(bins_, bins_ + kBins, -120.0f);
    write_ = 0;
    hop_ = 0;
  }

  void push(const float* left, const float* right, int frames) noexcept {
    for (int i = 0; i < frames; ++i) {
      const float l = left ? left[i] : 0.0f;
      const float r = right ? right[i] : l;
      ring_[write_] = 0.5f * (l + r);
      write_ = (write_ + 1) % kFftSize;
      if (++hop_ >= kHop) {
        hop_ = 0;
        analyse();
      }
    }
  }

  const float* bins() const noexcept { return bins_; }

  static float binFrequency(int index) noexcept {
    const float t = static_cast<float>(std::clamp(index, 0, kBins)) /
                    static_cast<float>(kBins);
    return kMinHz * std::pow(kMaxHz / kMinHz, t);
  }

private:
  void analyse() noexcept {
    // Oldest sample is at write_; copy into FFT order through the window.
    for (int i = 0; i < kFftSize; ++i) {
      const int src = (write_ + i) % kFftSize;
      const int dst = reversed_[i];
      re_[dst] = ring_[src] * window_[i];
      im_[dst] = 0.0f;
    }

    // Iterative radix-2 Cooley-Tukey.
    for (int len = 2; len <= kFftSize; len <<= 1) {
      const int half = len >> 1;
      const int step = kFftSize / len;
      for (int base = 0; base < kFftSize; base += len) {
        for (int j = 0; j < half; ++j) {
          const float wr = twiddleCos_[j * step];
          const float wi = twiddleSin_[j * step];
          const int a = base + j;
          const int b = a + half;
          const float tr = wr * re_[b] - wi * im_[b];
          const float ti = wr * im_[b] + wi * re_[b];
          re_[b] = re_[a] - tr;
          im_[b] = im_[a] - ti;
          re_[a] += tr;
          im_[a] += ti;
        }
      }
    }

    for (int i = 0; i < kBins; ++i) {
      float peak = 0.0f;
      for (int k = binLo_[i]; k < binHi_[i]; ++k) {
        const float mag = std::sqrt(re_[k] * re_[k] + im_[k] * im_[k]);
        peak = std::max(peak, mag);
      }
      // Hann coherent gain is 0.5, so a sine of amplitude A has
      // magnitude A * N / 4 after the window and FFT.
      const float amp = 4.0f * peak / static_cast<float>(kFftSize);
      const float db = std::max(gainToDb(amp), -120.0f);
      // Instant attack, gentle release so the display is readable.
      bins_[i] = std::max(db, bins_[i] - 2.0f);
    }
  }

  float sampleRate_ = 48000.0f;
  float ring_[kFftSize] = {};
  float re_[kFftSize] = {};
  float im_[kFftSize] = {};
  float window_[kFftSize] = {};
  float twiddleCos_[kFftSize / 2] = {};
  float twiddleSin_[kFftSize / 2] = {};
  uint16_t reversed_[kFftSize] = {};
  int binLo_[kBins] = {};
  int binHi_[kBins] = {};
  float bins_[kBins] = {};
  int write_ = 0;
  int hop_ = 0;
};

} // namespace hdsp
