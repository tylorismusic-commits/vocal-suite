// Vocal Suite — fet.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cmath>

#include "common.hpp"
#include "comp_controls.hpp"

namespace vocal {

// FET peak limiter modelled on the 1176 family.  The input transformer and
// FET attenuator add asymmetric even and odd harmonics that grow with gain
// reduction.  The "all buttons" setting runs the ratio at 20:1 with the extra
// colour and faster release that mode adds on the hardware.
//
// The hardware detector is a feedback loop, but a digital loop closed that way
// collapses at high ratios: the static slope has to be ratio-1, and on a
// transient that demands more reduction than the detector can survive, the
// gain latches to its floor and the audio turns into blips.  The NC76 reference
// this model is measured against solves it the same way: a feed-forward peak
// detector, attack/release applied to the over-threshold level, and a
// (ratio-1)/ratio slope.  That is what is implemented here.  It keeps the
// input knob's behaviour (more input, more reduction) and stays musical at
// 20:1 and all-buttons.
//
// Both revisions have the "all buttons" mode.  On Blue it is extreme: the
// ratio pushes further, the attack tightens, the release shortens and the FET
// stage is driven harder.  On Black the same mode engages but stays more
// controlled, matching the Rev E's more linear response.
//
// Blue (Rev A "bluestripe") — more distortion and even-order colour from the
// input and FET stages.
// Black (Rev E "blackface") — more linear compression response (softer knee),
// slower release once reduction is deep, and a cleaner, slightly darker
// output.
//
// Distortion targets were measured against a stillwell-derived reference
// implementation (Tukan's NC76, BSD-3): at 100 Hz and -12 dBFS it produces
// roughly -24 dB H2 and -31 dB THD.  That is far dirtier than a textbook
// compressor, and matching it is a large part of the 1176 character.
class FetLimiter {
public:
  enum class Variant { Blue, Black };

  void prepare(float sampleRate, Variant variant) noexcept {
    sr_ = sampleRate;
    variant_ = variant;
    const bool blue = variant == Variant::Blue;
    inputBias_ = blue ? 0.26f : 0.16f;
    fetBias_ = 0.0f;
    baseDrive_ = blue ? 1.30f : 1.10f;
    inputDriveScale_ = blue ? 1.60f : 1.00f;
    // Keep the full range: the references are open at the top, and a low
    // output low-pass removes exactly the room and air a drum bus needs.
    const float outLpHz = blue ? 20000.0f : 18000.0f;
    for (int ch = 0; ch < 2; ++ch) {
      lpOut_[ch].set(outLpHz, sr_);
      inShaper_[ch].reset();
      fetShaper_[ch].reset();
      lpOut_[ch].reset();
    }
    inputSmooth_.setTime(25.0f, sr_);
    inputSmooth_.reset(dbToGain(-80.0f));
    driveSmooth_.setTime(25.0f, sr_);
    driveSmooth_.reset(1.3f);
    makeupSmooth_.setTime(15.0f, sr_);
    makeupSmooth_.reset(1.0f);
    outputSmooth_.setTime(15.0f, sr_);
    outputSmooth_.reset(1.0f);
    for (int ch = 0; ch < 2; ++ch) {
      // Only remove DC, not sub-bass.  The old 18 Hz corner thinned the low
      // end compared with the references.
      dcLp_[ch].set(5.0f, sr_);
      dcLp_[ch].reset();
    }
    overDb_ = 0.0f;
    setControls(controls_);
    makeupSmooth_.snap();
    outputSmooth_.snap();
  }

  void setControls(const CompControls& c) noexcept {
    controls_ = c;
    const float input = clampf(c.input, 0.0f, 1.0f);
    // Input 0 is silence, as on the hardware: the knob is an input
    // attenuator, so you bring the signal in with it.
    const float inputDb = -80.0f + 103.0f * std::sqrt(input);
    inputSmooth_.setTarget(dbToGain(inputDb));
    driveSmooth_.setTarget(baseDrive_ + inputDriveScale_ * input);

    const int ratioStep = ratioStepFrom(c.ratio);
    ratio_ = ratioFromStep(ratioStep);
    ratioStep_ = ratioStep;
    allButtons_ = ratioStep >= 8;

    // 1176 attack runs 1..7 (about 1.6 ms down to 40 us); release runs 1..7
    // (about 50 ms up to 1.1 s).  Both are log-spaced between their ends.  The
    // attack range matches the NC76 reference: a slower attack lets the drum
    // transient through and compresses the room behind it, instead of pinning
    // the whole hit.
    const float attackStep =
        1.0f + std::round(clampf(c.attack, 0.0f, 1.0f) * 6.0f);
    const float releaseStep =
        1.0f + std::round(clampf(c.release, 0.0f, 1.0f) * 6.0f);
    attackMs_ = 0.04f * std::pow(40.0f, (attackStep - 1.0f) / 6.0f);
    releaseMs_ = 50.0f * std::pow(22.0f, (releaseStep - 1.0f) / 6.0f);
    if (allButtons_) {
      // All In is a faster, angrier setting on the hardware: the release is
      // roughly a tenth of the panel value and the attack tightens.
      attackMs_ *= variant_ == Variant::Blue ? 0.5f : 0.7f;
      releaseMs_ *= 0.1f;
    }
    attackCoeff_ = timeCoeff(attackMs_, sr_);
    releaseCoeff_ = timeCoeff(releaseMs_, sr_);
    releaseDeepCoeff_ = timeCoeff(releaseMs_ * 1.35f, sr_);

    outputSmooth_.setTarget(
        dbToGain(-12.0f + 24.0f * clampf(c.output, 0.0f, 1.0f)));
    // Bipolar make-up, like the other models, independent of the output trim.
    makeupSmooth_.setTarget(
        dbToGain(20.0f * (clampf(c.makeup, 0.0f, 1.0f) - 0.5f)));
  }

  // Processes one stereo frame in place.  Returns gain reduction in dB (>= 0).
  float process(float& left, float& right,
                const SidechainFrame& sc = {}) noexcept {
    const float inputGain = inputSmooth_.next();
    const float drive = driveSmooth_.next();
    const float inL = left * inputGain;
    const float inR = right * inputGain;

    // Feed-forward peak detector, as in the NC76 reference.  The programme
    // input gain is part of the detector path, so turning the input up
    // increases reduction.  An external side-chain is taken raw: its level is
    // already set by the send, and the input knob should not attenuate it.
    const float detL = sc.active ? sc.left : left * inputGain;
    const float detR = sc.active ? sc.right : right * inputGain;
    const float peak = std::max(std::fabs(detL), std::fabs(detR));
    const float levelDb = gainToDb(peak);

    // The input knob has a different taper from the reference (-80..+23 dB
    // here, -20..+30 there).  This threshold is calibrated so the *effective*
    // drive at the same knob position matches the NC76's Low mode, which is
    // the comparison point: at input 0.5 the raw threshold lands at -20.8 dB.
    constexpr float kThresholdDb = -28.0f;
    const float kneeDb = variant_ == Variant::Black ? 2.5f : 1.0f;
    // Effective over-threshold level, with a small soft knee.  Slope 1 turns
    // the gain computer into the over-level itself; the ratio is applied when
    // the level is turned into reduction, so attack and release act on dB of
    // over, exactly as the reference does.
    const float over = gainReductionDbSlope(levelDb, kThresholdDb, 1.0f,
                                           kneeDb, 1.0e9f);

    const bool blackDeep = variant_ == Variant::Black && overDb_ > 6.0f;
    const float rc = blackDeep ? releaseDeepCoeff_ : releaseCoeff_;
    if (over > overDb_) {
      overDb_ += (over - overDb_) * (1.0f - attackCoeff_);
    } else {
      overDb_ += (over - overDb_) * (1.0f - rc);
    }

    // Feed-forward slope.  All In adds the reference's 1.4x over-compression
    // factor; at 20:1 this makes the mode bite noticeably harder without ever
    // latching the gain to zero.
    const float slope =
        (1.0f - 1.0f / std::max(ratio_, 1.0f)) *
        (allButtons_ ? 1.35f : 1.0f);
    const float grDb = std::max(overDb_ * slope, 0.0f);
    const float gain = dbToGain(-grDb);

    // The FET attenuator distorts more as it works harder.
    const float grNorm = clampf(grDb / 20.0f, 0.0f, 1.5f);
    // Higher ratios drive the FET stage harder, so the buttons change
    // character as well as amount, as on the hardware.
    const float allDrive = variant_ == Variant::Blue ? 1.5f : 0.7f;
    const float ratioDrive = 0.22f * static_cast<float>(ratioStep_);
    const float fetDrive =
        2.2f + 3.2f * grNorm + ratioDrive + (allButtons_ ? allDrive : 0.0f);

    const float makeup = makeupSmooth_.next();
    const float output = outputSmooth_.next();
    const float cellL = inL * gain;
    const float cellR = inR * gain;
    left = processChannel(cellL, 0, fetDrive, drive) * makeup * output;
    right = processChannel(cellR, 1, fetDrive, drive) * makeup * output;
    return grDb;
  }

  void reset() noexcept {
    for (int ch = 0; ch < 2; ++ch) {
      lpOut_[ch].reset();
      dcLp_[ch].reset();
      inShaper_[ch].reset();
      fetShaper_[ch].reset();
    }
    overDb_ = 0.0f;
    // Keep the current make-up/output level across a host reset; resetting to
    // unity would make the output jump.
    makeupSmooth_.snap();
    outputSmooth_.snap();
  }

private:
  // Input transformer, then FET attenuator, then output transformer.
  float processChannel(float x, int ch, float fetDrive, float inputDrive) noexcept {
    const float y1 = inShaper_[ch].process(x, inputDrive, inputBias_);
    const float y2 = fetShaper_[ch].process(y1, fetDrive, fetBias_);
    // Remove the DC the asymmetric stages create; a kick would otherwise
    // offset the whole stage and thump.
    return lpOut_[ch].process(y2 - dcLp_[ch].process(y2));
  }

  float sr_ = 48000.0f;
  Variant variant_ = Variant::Blue;
  CompControls controls_;

  float inputBias_ = 0.0f;
  float fetBias_ = 0.0f;
  float baseDrive_ = 2.0f;
  float inputDriveScale_ = 3.0f;
  float ratio_ = 4.0f;
  bool allButtons_ = false;
  int ratioStep_ = 0;
  float attackMs_ = 0.1f;
  float releaseMs_ = 100.0f;
  float attackCoeff_ = 0.0f;
  float releaseCoeff_ = 0.0f;
  float releaseDeepCoeff_ = 0.0f;
  float overDb_ = 0.0f;
  OnePoleLP lpOut_[2];
  OnePoleLP dcLp_[2];
  Smoothed inputSmooth_;
  Smoothed driveSmooth_;
  Smoothed makeupSmooth_;
  Smoothed outputSmooth_;
  AsymShaper inShaper_[2];
  AsymShaper fetShaper_[2];
};

} // namespace vocal
