// Vocal Suite — engine
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The fixed serial chain: INPUT -> COMP -> TONE -> DRIVE -> DE-ESS -> ECHO
// -> SPACE -> OUT.  Every stage is always instantiated; the IN/OUT switches
// skip the stage without changing the routing.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <hdsp/spectrum.hpp>

#include "common.hpp"
#include "comp.hpp"
#include "controls.hpp"
#include "deess.hpp"
#include "drive.hpp"
#include "echo.hpp"
#include "input.hpp"
#include "meters.hpp"
#include "output.hpp"
#include "space.hpp"
#include "tone.hpp"

namespace vocal {

class Engine {
public:
  void prepare(float sampleRate, float /*oversample*/ = -1.0f) noexcept {
    sr_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    input_.prepare(sr_);
    comp_.prepare(sr_);
    tone_.prepare(sr_);
    drive_.prepare(sr_);
    deess_.prepare(sr_);
    echo_.prepare(sr_);
    space_.prepare(sr_);
    output_.prepare(sr_);
    analyzer_.prepare(sr_);
    compInEnv_ = compOutEnv_ = 0.0f;
    grAvg_ = 0.0f;
    inPeakL_ = inPeakR_ = outPeakL_ = outPeakR_ = 0.0f;
    setControls(Controls{});
    reset();
  }

  // CLAP compatibility: the vocal strip has no oversampling, so a deferred
  // rate change is a no-op.
  void setDeferOversample(bool /*defer*/) noexcept {}

  void setControls(const Controls& c) noexcept {
    controls_ = c;
    bpm_ = c.bpm > 1.0f ? c.bpm : 120.0f;
    input_.setControls(c);
    comp_.setControls(c);
    tone_.setControls(c);
    drive_.setControls(c);
    deess_.setControls(c);
    echo_.setControls(c);
    space_.setControls(c);
    output_.setControls(c);
  }

  void setTempo(float bpm, bool /*host*/) noexcept {
    if (std::isfinite(bpm) && bpm > 1.0f) {
      bpm_ = bpm;
      controls_.bpm = bpm;
      echo_.setControls(controls_);
    }
  }

  void reset() noexcept {
    input_.reset();
    comp_.reset();
    tone_.reset();
    drive_.reset();
    deess_.reset();
    echo_.reset();
    space_.reset();
    output_.reset();
    analyzer_.reset();
    inputLearnPeak_ = -120.0f;
    sibLearnHz_ = 6500.0f;
    sibLearnEnergy_ = -120.0f;
    sibLearnValid_ = false;
    compInEnv_ = compOutEnv_ = 0.0f;
    grAvg_ = 0.0f;
    inPeakL_ = inPeakR_ = outPeakL_ = outPeakR_ = 0.0f;
  }

  uint32_t latencySamples() const noexcept { return 0; }

  void process(float* left, float* right, int frames, Meters& meters,
               const float* /*scLeft*/ = nullptr,
               const float* /*scRight*/ = nullptr) noexcept {
    if (!left || !right || frames <= 0) {
      return;
    }
    meters = Meters{};
    meters.bpm = bpm_;
    if (!controls_.inputLearnActive) {
      inputLearnPeak_ = -120.0f;
    }
    if (!controls_.sibLearnActive) {
      sibLearnValid_ = false;
      sibLearnEnergy_ = -120.0f;
    }

    for (int i = 0; i < frames; ++i) {
      float l = left[i];
      float r = right[i];

      if (!controls_.power || controls_.compare) {
        left[i] = l;
        right[i] = r;
        updateMeters(l, r, l, r, 0.0f, 0.0f, 0.0f, 0.0f, meters);
        continue;
      }

      input_.process(l, r);
      const float inL = l;
      const float inR = r;

      float compGr = 0.0f;
      if (controls_.compOn) {
        const float preL = l;
        const float preR = r;
        compGr = comp_.process(l, r);
        const float mix = clampf(controls_.compMix / 100.0f, 0.0f, 1.0f);
        const float envCoeff = timeCoeff(250.0f, sr_);
        const float inPow = 0.5f * (preL * preL + preR * preR);
        const float outPow = 0.5f * (l * l + r * r);
        compInEnv_ += (inPow - compInEnv_) * envCoeff;
        compOutEnv_ += (outPow - compOutEnv_) * envCoeff;
        const float grCoeff = timeCoeff(300.0f, sr_);
        grAvg_ += (compGr - grAvg_) * grCoeff;
        float makeup = 1.0f;
        if (controls_.compAuto && compOutEnv_ > 1.0e-10f) {
          makeup = clampf(std::sqrt(compInEnv_ / compOutEnv_), 0.125f, 8.0f);
        }
        l = preL + mix * (l * makeup - preL);
        r = preR + mix * (r * makeup - preR);
      }

      if (controls_.toneOn) {
        tone_.process(l, r);
      }

      if (controls_.driveOn) {
        drive_.process(l, r);
      }

      // Analyse the de-ess input, not the output: the learn action must see
      // the sibilance before the dynamic cut removes it.
      analyzer_.push(&l, &r, 1);

      float sibGr = 0.0f;
      float harshGr = 0.0f;
      if (controls_.deessOn) {
        deess_.process(l, r, sibGr, harshGr);
      }

      if (controls_.echoOn) {
        echo_.process(l, r);
      }

      if (controls_.spaceOn) {
        space_.process(l, r);
      }

      const float limGr = output_.process(l, r);

      left[i] = l;
      right[i] = r;
      updateMeters(inL, inR, l, r, compGr, sibGr, harshGr, limGr, meters);
      if (controls_.inputLearnActive) {
        const float peak = std::max(std::fabs(inL), std::fabs(inR));
        inputLearnPeak_ = std::max(inputLearnPeak_, gainToDb(peak + 1.0e-12f));
      }
    }

    if (controls_.sibLearnActive) {
      const float* bins = analyzer_.bins();
      float bestDb = -120.0f;
      int bestBin = -1;
      for (int i = 0; i < hdsp::SpectrumAnalyzer::kBins; ++i) {
        const float hz = hdsp::SpectrumAnalyzer::binFrequency(i);
        if (hz >= 4000.0f && hz <= 10000.0f && bins[i] > bestDb) {
          bestDb = bins[i];
          bestBin = i;
        }
      }
      if (bestBin >= 0) {
        sibLearnEnergy_ = bestDb;
        if (bestDb > -55.0f) {
          sibLearnHz_ = hdsp::SpectrumAnalyzer::binFrequency(bestBin);
          sibLearnValid_ = true;
        }
      }
    }
    meters.inputLearnPeakDb = inputLearnPeak_;
    meters.sibLearnHz = sibLearnHz_;
    meters.sibLearnEnergyDb = sibLearnEnergy_;
    meters.sibLearnValid = sibLearnValid_;
  }

private:
  static float peakUpdate(float x, float& state) noexcept {
    const float a = std::fabs(x);
    if (a > state) {
      state = a;
    } else {
      state *= 0.9997f;
    }
    return state;
  }

  void updateMeters(float inL, float inR, float outL, float outR, float compGr,
                    float sibGr, float harshGr, float limGr,
                    Meters& m) noexcept {
    const float pl = peakUpdate(inL, inPeakL_);
    const float pr = peakUpdate(inR, inPeakR_);
    const float ol = peakUpdate(outL, outPeakL_);
    const float orr = peakUpdate(outR, outPeakR_);
    m.inPeakLDb = gainToDb(pl + 1.0e-12f);
    m.inPeakRDb = gainToDb(pr + 1.0e-12f);
    m.outPeakLDb = gainToDb(ol + 1.0e-12f);
    m.outPeakRDb = gainToDb(orr + 1.0e-12f);
    m.inPeakDb = std::max(m.inPeakLDb, m.inPeakRDb);
    m.outPeakDb = std::max(m.outPeakLDb, m.outPeakRDb);
    m.outVuDb = m.outPeakDb;
    compGrHold_ = std::max(compGr, compGrHold_ * 0.9995f);
    sibGrHold_ = std::max(sibGr, sibGrHold_ * 0.9995f);
    harshGrHold_ = std::max(harshGr, harshGrHold_ * 0.9995f);
    limGrHold_ = std::max(limGr, limGrHold_ * 0.9995f);
    m.compGrDb = compGrHold_;
    m.grDb = compGrHold_;
    m.sibGrDb = sibGrHold_;
    m.harshGrDb = harshGrHold_;
    m.limGrDb = limGrHold_;
  }

  float sr_ = 48000.0f;
  float bpm_ = 120.0f;
  Controls controls_;
  InputStage input_;
  VocalComp comp_;
  Tone tone_;
  Drive drive_;
  DeEss deess_;
  Echo echo_;
  Space space_;
  OutputStage output_;
  hdsp::SpectrumAnalyzer analyzer_;
  float inputLearnPeak_ = -120.0f;
  float sibLearnHz_ = 6500.0f;
  float sibLearnEnergy_ = -120.0f;
  bool sibLearnValid_ = false;
  float compGrHold_ = 0.0f;
  float sibGrHold_ = 0.0f;
  float harshGrHold_ = 0.0f;
  float limGrHold_ = 0.0f;
  float compInEnv_ = 0.0f;
  float compOutEnv_ = 0.0f;
  float grAvg_ = 0.0f;
  float inPeakL_ = 0.0f;
  float inPeakR_ = 0.0f;
  float outPeakL_ = 0.0f;
  float outPeakR_ = 0.0f;
};

} // namespace vocal
