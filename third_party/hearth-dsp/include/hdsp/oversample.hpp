// hearth-dsp — oversample.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hdsp {

// Two-times oversampling stage built from a linear-phase half-band FIR.
//
// The filter is a 61-tap Kaiser-windowed half-band (beta 8), designed at the
// oversampled rate: -0.1 dB at 21 kHz, -12 dB at 25 kHz, below -80 dB from
// 29 kHz up, for a 48 kHz base rate.  Because it is a half-band, every even
// tap except the centre is zero, so it can be implemented in polyphase form:
// 30 multiply-accumulates per output sample instead of 61.
//
// Latency is (taps - 1) / 2 = 30 samples at the oversampled rate per direction,
// which is 30 base-rate samples for a 2x up/down pair.  Cascading two stages
// for 4x adds another 15 base-rate samples.
class Oversampler {
public:
  static constexpr int kMaxBaseFrames = 1024;
  static constexpr int kOddTaps = 30;

  // factor must be 1, 2 or 4.
  void prepare(int factor) noexcept {
    stages_ = factor >= 4 ? 2 : (factor == 2 ? 1 : 0);
    for (auto& st : stage_) {
      for (auto& ch : st) {
        ch.reset();
      }
    }
  }

  void reset() noexcept {
    for (auto& st : stage_) {
      for (auto& ch : st) {
        ch.reset();
      }
    }
  }

  int factor() const noexcept { return 1 << stages_; }

  // Latency in base-rate samples.
  int latencySamples() const noexcept {
    return stages_ == 0 ? 0 : (stages_ == 1 ? 30 : 45);
  }

  // Upsamples n base-rate frames into n * factor() frames.  n must be at most
  // kMaxBaseFrames; larger blocks are the caller's job to chunk.  Input is
  // clamped and finite, so the filter state cannot be poisoned.
  void up(const float* inL, const float* inR, int n, float* outL,
          float* outR) noexcept {
    if (n <= 0) {
      return;
    }
    if (stages_ == 0) {
      for (int i = 0; i < n; ++i) {
        outL[i] = sanitize(inL[i]);
        outR[i] = sanitize(inR[i]);
      }
      return;
    }
    if (stages_ == 1) {
      stage_[0][0].up(inL, n, outL);
      stage_[0][1].up(inR, n, outR);
      return;
    }
    stage_[0][0].up(inL, n, midL_);
    stage_[0][1].up(inR, n, midR_);
    stage_[1][0].up(midL_, 2 * n, outL);
    stage_[1][1].up(midR_, 2 * n, outR);
  }

  // Downsamples n * factor() frames into n base-rate frames.
  void down(const float* inL, const float* inR, int n, float* outL,
            float* outR) noexcept {
    if (n <= 0) {
      return;
    }
    if (stages_ == 0) {
      for (int i = 0; i < n; ++i) {
        outL[i] = inL[i];
        outR[i] = inR[i];
      }
      return;
    }
    if (stages_ == 1) {
      stage_[0][0].down(inL, n, outL);
      stage_[0][1].down(inR, n, outR);
      return;
    }
    // The first decimation halves 4n -> 2n; the second halves 2n -> n.
    stage_[1][0].down(inL, 2 * n, midL_);
    stage_[1][1].down(inR, 2 * n, midR_);
    stage_[0][0].down(midL_, n, outL);
    stage_[0][1].down(midR_, n, outR);
  }

private:
  static float sanitize(float x) noexcept {
    if (!std::isfinite(x)) {
      return 0.0f;
    }
    return std::min(std::max(x, -32.0f), 32.0f);
  }

  struct Stage {
    // Centre tap (h[30]) and the 30 non-zero odd taps h[1], h[3], ... h[59].
    // Generated with a Kaiser window (beta 8) and normalised to unity DC.
    static constexpr float kCentre = 0.499991244871f;
    static constexpr float kOdd[kOddTaps] = {
        6.05368366667e-05f,   -0.000201244870769f, 0.000486131090037f,
        -0.000996134189552f,  0.00183585769594f,   -0.00313684549993f,
        0.005063949714f,      -0.00782970725346f,   0.0117274160352f,
        -0.0172083239302f,    0.0250720381912f,     -0.0369950257376f,
        0.0573387338051f,     -0.102196658555f,     0.316983654233f,
        0.316983654233f,      -0.102196658555f,     0.0573387338051f,
        -0.0369950257376f,    0.0250720381912f,     -0.0172083239302f,
        0.0117274160352f,     -0.00782970725346f,   0.005063949714f,
        -0.00313684549993f,   0.00183585769594f,    -0.000996134189552f,
        0.000486131090037f,   -0.000201244870769f,  6.05368366667e-05f};

    float upHistory[kOddTaps] = {};
    float evenHistory[16] = {};
    float oddHistory[kOddTaps] = {};
    int upPos = 0;
    int evenPos = 0;
    int oddPos = 0;

    void reset() noexcept {
      for (float& v : upHistory) v = 0.0f;
      for (float& v : evenHistory) v = 0.0f;
      for (float& v : oddHistory) v = 0.0f;
      upPos = evenPos = oddPos = 0;
    }

    // in: n base-rate samples.  out: 2n samples.
    void up(const float* in, int n, float* out) noexcept {
      for (int i = 0; i < n; ++i) {
        const float x = sanitize(in[i]);
        upHistory[upPos] = x;
        upPos = (upPos + 1) % kOddTaps;
        // Even phase: only the centre tap survives, at delay 15.
        // Zero-stuffing halves the DC level, so the interpolator carries the
        // customary gain of 2.
        out[2 * i] =
            2.0f * kCentre *
            upHistory[(upPos - 1 - 15 + kOddTaps) % kOddTaps];
        float odd = 0.0f;
        for (int j = 0; j < kOddTaps; ++j) {
          odd += kOdd[j] * upHistory[(upPos - 1 - j + kOddTaps) % kOddTaps];
        }
        out[2 * i + 1] = 2.0f * odd;
      }
    }

    // in: 2n oversampled samples.  out: n base-rate samples.
    void down(const float* in, int n, float* out) noexcept {
      for (int i = 0; i < n; ++i) {
        const float e = in[2 * i];
        const float o = in[2 * i + 1];
        evenHistory[evenPos] = e;
        evenPos = (evenPos + 1) % 16;
        oddHistory[oddPos] = o;
        oddPos = (oddPos + 1) % kOddTaps;
        float y = kCentre * evenHistory[(evenPos - 1 - 15 + 16) % 16];
        // v[2n - (2j+1)] is the odd sample one step before the current even
        // one: o[n-1-j], not o[n-j].
        for (int j = 0; j < kOddTaps; ++j) {
          y += kOdd[j] *
               oddHistory[(oddPos - 2 - j + 2 * kOddTaps) % kOddTaps];
        }
        out[i] = y;
      }
    }
  };

  // [cascade stage][channel].  Sharing one filter state across channels made
  // the right channel start from the left channel's history, which is a buzz.
  Stage stage_[2][2];
  int stages_ = 0;
  float midL_[2 * kMaxBaseFrames] = {};
  float midR_[2 * kMaxBaseFrames] = {};
};

} // namespace hdsp
