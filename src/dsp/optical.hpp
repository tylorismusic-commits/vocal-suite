// Vocal Suite — optical.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

#include <hdsp/biquad.hpp>
#include "common.hpp"
#include "comp_controls.hpp"

namespace vocal {

// Optical leveling amplifier modelled on the 2A and 3A families.
//
// The behaviour lives in the side-chain, not in a ratio/knee gain computer:
//
//   * a three-band side-chain shaping network (10 Hz low shelf, 17 kHz high
//     shelf, 220 Hz dip) models the response of the internal photoelectric
//     side-chain, so low and high content do not trip the cell the same way;
//   * the detector blends RMS with instantaneous peak (25/75), as the cell
//     responds to both;
//   * after the first gain stage the side-chain is fed through a 43 Hz
//     +30 dB shelf and the larger of the direct and integrated levels drives a
//     second gain stage.  That is the low-frequency "hold" that stops an
//     optical compressor letting go on bass notes;
//   * the resulting gain passes through a four-stage envelope whose stages
//     reference each other, so a fast stage cannot release ahead of a slow
//     one.  That mutual reference is the cell's memory.
//
// LA2A uses the classic memory path; LA3A uses the faster one.
class OptoLeveler {
public:
  enum class Variant { LA2A, LA3A };

  void prepare(float sampleRate, Variant variant) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    variant_ = variant;

    for (int ch = 0; ch < 2; ++ch) {
      scLowShelf_[ch].lowShelf(10.0f, -9.0f, 0.5f, sr_);
      scHighShelf_[ch].highShelf(17000.0f, -6.0f, 0.3f, sr_);
      scDip_[ch].peak(220.0f, -3.25f, 0.42f, sr_);
      integrated_[ch].lowShelf(43.0f, 30.0f, 0.6f, sr_);
    }

    rmsK_ = settleK(36.0, sr_);
    attackK_ = settleK(0.0, sr_);   // instantaneous
    releaseK_ = settleK(20.0, sr_);
    levelAttackK_ = settleK(0.0, sr_);
    levelReleaseK_ = settleK(20.0, sr_);
    makeupSmooth_.setTime(20.0f, sr_);
    makeupSmooth_.reset(1.0f);
    outputSmooth_.setTime(20.0f, sr_);
    outputSmooth_.reset(1.0f);

    // The 3A cell is the faster of the two: same memory topology, shorter
    // release on every stage.
    const double releaseScale = variant_ == Variant::LA3A ? 0.65 : 1.0;

    lpf1AttackK_ = settleK(35000.0, sr_);
    lpf1ReleaseK_ = settleK(9000.0 * releaseScale, sr_);
    lpf2AttackK_ = settleK(1400.0, sr_);
    lpf2ReleaseK_ = settleK(900.0 * releaseScale, sr_);
    lpf3AttackK_ = settleK(1100.0, sr_);
    lpf3ReleaseK_ = settleK(12000.0 * releaseScale, sr_);
    lpf4ReleaseK_ = settleK(70.0 * releaseScale, sr_);
    dynAttackAttackK_ = settleK(15.0, sr_);
    dynAttackReleaseK_ = settleK(2.0, sr_);

    reset();
    setControls(controls_);
    makeupSmooth_.snap();
    outputSmooth_.snap();
  }

  void setControls(const CompControls& c) noexcept {
    controls_ = c;
    const double pr = clampf(c.threshold, 0.0f, 1.0f);

    // Mapping of peak reduction to the threshold curve, as in the reference.
    constA_ = (pr * 1.03 - 0.03) * -49.0;
    thresholdDb_ = constA_ + 47.0;
    thresholdLin_ = std::pow(10.0, thresholdDb_ / 20.0);
    constB_ = std::pow(2.0, 1.0 - pr) + 1.2;
    constC_ = std::log10(constB_);
    constD_ = std::pow(10.0, constA_ / 20.0);

    isLimit_ = ratioStepFrom(c.ratio) >= 4;
    ratioBase_ = isLimit_ ? 1.3 : 1.2;
    logRatioBase_ = std::log10(ratioBase_);

    gainScale_ = pr * pr * 14.0 + 1.0;
    makeupSmooth_.setTarget(
        dbToGain(20.0f * (clampf(c.makeup, 0.0f, 1.0f) - 0.5f)));
    outputSmooth_.setTarget(c.output > 0.5f ? dbToGain(6.0f) : 1.0f);
  }

  // Returns gain reduction in dB (>= 0).
  float process(float& left, float& right,
                const SidechainFrame& sc = {}) noexcept {
    const double x[2] = {left, right};
    double levelMax = 0.0;

    for (int ch = 0; ch < 2; ++ch) {
      // The cell sits inside the side-chain loop: the detector taps the output
      // of the attenuator, not the input (one-sample implicit feedback).
      // An external side-chain bypasses the loop entirely.
      const double detector =
          sc.active ? (ch == 0 ? sc.left : sc.right)
                    : (topology_ == Topology::Feedback
                           ? kFeedbackLoopGain * prevOut_[ch]
                           : x[ch]);
      const double shaped = scDip_[ch].process(
          scHighShelf_[ch].process(scLowShelf_[ch].process(detector)));
      sidechain_[ch] = shaped;
      rms_[ch] += (shaped * shaped - rms_[ch]) * rmsK_;
      if (rms_[ch] < 1.0e-20) {
        rms_[ch] = 0.0;  // keep denormals out of the detector
      }
      const double level = std::sqrt(std::max(rms_[ch], 0.0)) * 0.25 +
                           std::fabs(shaped) * 0.75;
      levelMax = std::max(levelMax, level);
    }
    levelMax = std::max(levelMax, kTiny);

    // First gain stage: a soft threshold curve rather than a ratio/knee.
    const double levelDb = 20.0 * std::log10(levelMax) - constA_;
    const double valueDb =
        std::log10(std::pow(constB_, levelDb) + 1.0) / constC_ + constA_;
    const double value = std::pow(10.0, valueDb / 20.0);
    const double target = (value - constD_) / levelMax;

    const float k = static_cast<float>(
        target > grFilter_ ? attackK_ : releaseK_);
    grFilter_ += (target - grFilter_) * k;

    // Second stage, driven by the compressed side-chain.  The 43 Hz shelf
    // lifts the low end so bass keeps the cell engaged.
    double levelDirect = 0.0;
    double levelIntegrated = 0.0;
    for (int ch = 0; ch < 2; ++ch) {
      const double compressed = sidechain_[ch] * grFilter_;
      const double low = integrated_[ch].process(static_cast<float>(compressed));
      levelDirect = std::max(levelDirect, std::fabs(compressed));
      levelIntegrated = std::max(levelIntegrated, std::fabs(low));
    }
    levelDirect = std::max(levelDirect, kTiny);
    levelIntegrated = std::max(levelIntegrated, kTiny);

    const float kd = static_cast<float>(
        levelDirect > levelDirectLevel_ ? levelAttackK_ : levelReleaseK_);
    levelDirectLevel_ += (levelDirect - levelDirectLevel_) * kd;
    const float ki = static_cast<float>(
        levelIntegrated > levelIntegratedLevel_ ? levelAttackK_ : levelReleaseK_);
    levelIntegratedLevel_ += (levelIntegrated - levelIntegratedLevel_) * ki;

    const double v1 = 20.0 * std::log10(levelIntegratedLevel_);
    const double v2 =
        std::log10(std::pow(ratioBase_, v1 - thresholdDb_) + 1.0);
    const double v3 =
        std::pow(10.0, (v2 / logRatioBase_ + thresholdDb_) / 20.0);
    const double v4 = thresholdLin_ / v3 * levelDirectLevel_;
    const double v5 =
        std::pow(constD_ / (constD_ + v4), isLimit_ ? 0.94 : 0.79);

    const double gain = envelope(v5);
    const double deltaGain =
        std::max((1.0 - std::pow(gain * 0.98, 0.1)) * 18.0, 0.0);
    const float makeup = makeupSmooth_.next();
    const float outputGain = outputSmooth_.next();

    left = saturateStage(x[0], gain, deltaGain, makeup, outputGain);
    right = saturateStage(x[1], gain, deltaGain, makeup, outputGain);
    // Tap for the next sample's detector: the cell output, before make-up.
    prevOut_[0] = x[0] * gain;
    prevOut_[1] = x[1] * gain;

    return static_cast<float>(-20.0 * std::log10(std::max(gain, 1e-6)));
  }

  void reset() noexcept {
    for (int ch = 0; ch < 2; ++ch) {
      scLowShelf_[ch].reset();
      scHighShelf_[ch].reset();
      scDip_[ch].reset();
      integrated_[ch].reset();
      rms_[ch] = 0.0;
      sidechain_[ch] = 0.0;
      prevOut_[ch] = 0.0;
    }
    grFilter_ = 1.0;
    levelDirectLevel_ = 0.0;
    levelIntegratedLevel_ = 0.0;
    stage1_ = 1.0;
    stage2_ = 1.0;
    stage3_ = 1.0;
    stage4_ = 1.0;
    dynAttack_ = 0.0;
  }

private:
  static constexpr double kTiny = 1.0e-10;

  // Asymmetric valve-ish transfer used on both sides of the gain cell.
  static double saturate(double x) noexcept {
    return x >= 0.0 ? x / (std::pow(x * 0.05, 1.3) + 1.0)
                    : x / (1.0 - x * 0.005);
  }

  float saturateStage(double x, double gain, double deltaGain, float makeup,
                      float outputGain) const noexcept {
    const double satIn = saturate(x);
    const double deltaIn = satIn - x;
    const double y = x * gain * gainScale_;
    const double satOut = saturate(y);
    const double deltaOut = satOut - y;
    return static_cast<float>(((deltaOut * 0.5 + deltaIn * 0.5) * deltaGain +
                               x) *
                              gain * outputGain * makeup);
  }

  // Four-stage envelope.  Each stage clamps against the one before it, so a
  // fast stage cannot release ahead of a slower one.
  double envelope(double target) noexcept {
    // Stage 1: slow memory, clamped so the cell never fully closes.
    stage1_ += (target - stage1_) *
               (target < stage1_ ? lpf1AttackK_ : lpf1ReleaseK_);
    stage1_ = std::max(stage1_, 0.7);

    // Stage 2: releases against the slower stage 1.
    if (target < stage2_) {
      stage2_ += (target - stage2_) * lpf2AttackK_;
    } else {
      stage2_ += (std::min(target, stage1_) - stage2_) * lpf2ReleaseK_;
    }
    stage2_ = std::max(stage2_, 0.4);

    // Stage 3 compares against the current output, not its own state.
    stage3_ += (target - stage3_) *
               (target < stage4_ ? lpf3AttackK_ : lpf3ReleaseK_);

    double shape = (stage3_ - 0.5) * 2.0;
    double maxGain = target * 12.0;
    if (variant_ == Variant::LA3A) {
      shape /= 5.5;
      maxGain = target * 6.0;
      dynAttack_ = 0.9;  // the faster cell restarts from a higher point
    }

    // Stage 4: attack speed follows the amount of reduction already applied.
    const double attackMs = 70.0 * std::max(0.3, shape);
    const float attackK = settleK(attackMs, sr_);

    if (target < stage4_) {
      if (target + 1.0e-4 < stage4_) {
        dynAttack_ += (1.0 - dynAttack_) * dynAttackAttackK_;
      }
      stage4_ += (target - stage4_) * attackK * dynAttack_;
      if (stage4_ >= maxGain) {
        stage4_ = maxGain;
      }
    } else {
      dynAttack_ += (0.0 - dynAttack_) * dynAttackReleaseK_;
      stage4_ += (std::min(target, stage2_) - stage4_) * lpf4ReleaseK_;
    }

    return std::min(std::max(stage4_, 0.0), 1.0);
  }

  double sr_ = 48000.0;
  Variant variant_ = Variant::LA2A;
  CompControls controls_;

  Biquad scLowShelf_[2];
  Biquad scHighShelf_[2];
  Biquad scDip_[2];
  Biquad integrated_[2];

  double rms_[2] = {0.0, 0.0};
  double sidechain_[2] = {0.0, 0.0};
  double prevOut_[2] = {0.0, 0.0};
  Topology topology_ = Topology::Feedback;
  // Loop-gain compensation: the detector taps the attenuated output, so the
  // same control positions would otherwise reduce far less.
  static constexpr double kFeedbackLoopGain = 3.0;

  double grFilter_ = 1.0;
  double levelDirectLevel_ = 0.0;
  double levelIntegratedLevel_ = 0.0;

  double stage1_ = 1.0;
  double stage2_ = 1.0;
  double stage3_ = 1.0;
  double stage4_ = 1.0;
  double dynAttack_ = 0.0;

  double constA_ = 0.0;
  double constB_ = 1.0;
  double constC_ = 1.0;
  double constD_ = 1.0;
  double thresholdDb_ = 0.0;
  double thresholdLin_ = 1.0;
  double ratioBase_ = 1.2;
  double logRatioBase_ = 0.079;
  bool isLimit_ = false;

  double gainScale_ = 1.0;
  Smoothed makeupSmooth_;
  Smoothed outputSmooth_;

  float rmsK_ = 0.0f;
  float attackK_ = 1.0f;
  float releaseK_ = 0.0f;
  float levelAttackK_ = 1.0f;
  float levelReleaseK_ = 0.0f;
  float lpf1AttackK_ = 0.0f;
  float lpf1ReleaseK_ = 0.0f;
  float lpf2AttackK_ = 0.0f;
  float lpf2ReleaseK_ = 0.0f;
  float lpf3AttackK_ = 0.0f;
  float lpf3ReleaseK_ = 0.0f;
  float lpf4ReleaseK_ = 0.0f;
  float dynAttackAttackK_ = 0.0f;
  float dynAttackReleaseK_ = 0.0f;
};

} // namespace vocal
