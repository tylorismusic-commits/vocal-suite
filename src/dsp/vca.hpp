// Vocal Suite — vca.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cmath>

#include "common.hpp"
#include "comp_controls.hpp"

namespace vocal {

// Stereo-linked feed-forward VCA bus compressor in the style of an SSL-type
// bus unit.  The detector is RMS over both channels, so the mix bus stays
// balanced.  Attack and release are continuous, the ratio runs 2:1 to 10:1,
// and the sidechain high-pass keeps low-frequency content from pumping the
// gain.  The output stage is clean, as on the hardware.
class BusVca {
public:
  void prepare(float sampleRate) noexcept {
    sr_ = sampleRate;
    for (auto& f : sidechain_) {
      f.reset();
    }
    for (int ch = 0; ch < 2; ++ch) {
      env_[ch] = 0.0f;
      hold_[ch] = 0;
    }
    for (auto& sh : shaper_) {
      sh.reset();
    }
    holdSamples_ = static_cast<int>(0.002f * sr_);
    setControls(controls_);
  }

  void setControls(const CompControls& c) noexcept {
    controls_ = c;
    const float thr = clampf(c.threshold, 0.0f, 1.0f);
    thresholdDb_ = -40.0f + 40.0f * thr;
    ratio_ = ratioFromStep(ratioStepFrom(c.ratio));
    attackMs_ = 0.1f * std::pow(300.0f, clampf(c.attack, 0.0f, 1.0f));
    releaseMs_ = 100.0f * std::pow(12.0f, clampf(c.release, 0.0f, 1.0f));
    attackCoeff_ = timeCoeff(attackMs_, sr_);
    releaseCoeff_ = timeCoeff(releaseMs_, sr_);
    // Bipolar: 0.5 is unity, matching the other models.
    makeupGain_ = dbToGain(20.0f * (clampf(c.makeup, 0.0f, 1.0f) - 0.5f));
    autoRelease_ = c.vcaAutoRelease;
    link_ = clampf(c.link, 0.0f, 1.0f);

    const float hpf = clampf(c.sidechainHpf, 0.0f, 1.0f);
    sidechainOn_ = hpf > 0.01f;
    if (sidechainOn_) {
      const float cutoff = 20.0f * std::pow(20.0f, hpf);
      for (auto& f : sidechain_) {
        f.set(cutoff, sr_);
      }
    }
  }

  // Processes one stereo frame in place.  Returns gain reduction in dB (>= 0).
  float process(float& left, float& right,
                const SidechainFrame& sc = {}) noexcept {
    float detL = sc.active ? sc.left : left;
    float detR = sc.active ? sc.right : right;
    if (sidechainOn_) {
      detL = sidechain_[0].process(detL);
      detR = sidechain_[1].process(detR);
    }
    // Peak detection, not RMS: with an RMS detector a -18 dBFS tone sits
    // below the default threshold and the ratio control appears dead.
    const float levelL = std::fabs(detL);
    const float levelR = std::fabs(detR);
    // Link: 1 = both channels follow the louder one (bus behaviour),
    // 0 = independent detectors (dual mono).
    const float loudest = std::max(levelL, levelR);
    const float targetL = link_ * loudest + (1.0f - link_) * levelL;
    const float targetR = link_ * loudest + (1.0f - link_) * levelR;

    float grDb[2];
    updateDetector(0, targetL);
    updateDetector(1, targetR);
    grDb[0] = gainReductionDb(gainToDb(env_[0]), thresholdDb_, ratio_, kKneeDb);
    grDb[1] = gainReductionDb(gainToDb(env_[1]), thresholdDb_, ratio_, kKneeDb);

    const float gainL = dbToGain(-grDb[0]) * makeupGain_;
    const float gainR = dbToGain(-grDb[1]) * makeupGain_;
    // The VCAs and output stage add a little colour; the measured reference
    // bus compressor is not perfectly clean either.
    left = shaper_[0].process(left * gainL, 1.7f, 0.13f);
    right = shaper_[1].process(right * gainR, 1.7f, 0.13f);
    return std::max(std::max(grDb[0], grDb[1]), 0.0f);
  }

  void reset() noexcept {
    for (auto& f : sidechain_) {
      f.reset();
    }
    for (int ch = 0; ch < 2; ++ch) {
      env_[ch] = 0.0f;
      hold_[ch] = 0;
    }
    for (auto& sh : shaper_) {
      sh.reset();
    }
  }

private:
  static constexpr float kKneeDb = 2.0f;

  // Attack, then a short hold, then release.  The hold is what stops the
  // detector releasing between the cycles of low-frequency material, which
  // otherwise modulates the gain at the signal's own rate.
  void updateDetector(int ch, float level) noexcept {
    if (level > env_[ch]) {
      env_[ch] = attackCoeff_ * env_[ch] + (1.0f - attackCoeff_) * level;
      hold_[ch] = holdSamples_;
    } else if (hold_[ch] > 0) {
      --hold_[ch];
    } else {
      float coeff = releaseCoeff_;
      if (autoRelease_) {
        // Two-stage automatic release: quick while the reduction is small,
        // progressively slower as it deepens, as on the bus compressor.
        // Depth is how far the detector sits over the threshold, not the
        // absolute level: a quiet signal must not get the slowest release.
        const float over =
            clampf((gainToDb(std::max(env_[ch], 1e-6f)) - thresholdDb_) / 20.0f,
                   0.0f, 1.0f);
        coeff = timeCoeff(releaseMs_ * (1.0f + 10.0f * over), sr_);
      }
      env_[ch] = coeff * env_[ch] + (1.0f - coeff) * level;
    }
  }

  float sr_ = 48000.0f;
  CompControls controls_;
  float thresholdDb_ = -18.0f;
  float ratio_ = 4.0f;
  float attackMs_ = 10.0f;
  float releaseMs_ = 300.0f;
  float attackCoeff_ = 0.0f;
  float releaseCoeff_ = 0.0f;
  float makeupGain_ = 1.0f;
  bool sidechainOn_ = false;
  bool autoRelease_ = false;
  float link_ = 1.0f;
  int holdSamples_ = 0;
  float env_[2] = {0.0f, 0.0f};
  int hold_[2] = {0, 0};
  HighPass2 sidechain_[2];
  AsymShaper shaper_[2];
};

} // namespace vocal
