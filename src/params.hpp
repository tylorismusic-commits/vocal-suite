// Vocal Suite — params.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One table drives the DSP, the CLAP parameters, the editor, the LV2 metadata
// (via tools/gen_ttl.cpp) and the factory presets.  Adding a parameter means
// adding one row here and one field in dsp/controls.hpp.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "dsp/controls.hpp"

namespace vocal {

// Parameter ids are the CLAP parameter ids and the index into Shared::values.
// They are also the ordering of the LV2 control ports (port = 4 + id), so this
// enum must only ever be appended to.
enum ParamId : uint32_t {
  P_POWER = 0,
  P_INPUT,
  P_POLARITY,
  P_HPF_ON,
  P_HPF_FREQ,
  P_COMP_ON,
  P_COMP_MODE,
  P_COMP_AMOUNT,
  P_COMP_SPEED,
  P_COMP_RATIO,
  P_COMP_MIX,
  P_COMP_AUTO,
  P_TONE_ON,
  P_LOW_GAIN,
  P_LOW_FREQ,
  P_LOW_BELL,
  P_MID_GAIN,
  P_MID_FREQ,
  P_MID_Q,
  P_HIGH_GAIN,
  P_HIGH_FREQ,
  P_HIGH_BELL,
  P_AIR,
  P_DRIVE_ON,
  P_DRIVE_MODEL,
  P_DRIVE_AMOUNT,
  P_DRIVE_CHAR,
  P_DRIVE_MIX,
  P_DEESS_ON,
  P_SIB_FREQ,
  P_SIB_AMOUNT,
  P_HARSH_FREQ,
  P_HARSH_AMOUNT,
  P_ECHO_ON,
  P_ECHO_SYNC,
  P_ECHO_DIV,
  P_ECHO_TIME,
  P_ECHO_FEEDBACK,
  P_ECHO_TONE,
  P_ECHO_WIDTH,
  P_ECHO_MOD,
  P_ECHO_DUCK,
  P_ECHO_LOFI,
  P_ECHO_MIX,
  P_TEMPO,
  P_SPACE_ON,
  P_SPACE_MODE,
  P_SPACE_SIZE,
  P_SPACE_DECAY,
  P_SPACE_PREDELAY,
  P_SPACE_TONE,
  P_SPACE_WIDTH,
  P_SPACE_DUCK,
  P_SPACE_MIX,
  P_OUTPUT,
  P_LIMIT_ON,
  P_COUNT
};

// Scale-point labels for a stepped parameter.  Values are always 0..count-1.
struct Labels {
  const char* const* names = nullptr;
  int count = 0;
};

template <size_t N>
constexpr Labels labels(const char* const (&arr)[N]) noexcept {
  return Labels{arr, static_cast<int>(N)};
}

// `symbol` is the LV2 port symbol and is part of the saved-session format.
// Never rename one after release.
struct ParamSpec {
  const char* symbol;
  const char* name;
  const char* module;
  double min;
  double max;
  double def;
  bool stepped;
  bool hidden;
  bool toggled;
  const char* unit;
  Labels labels;
  // Recommended range drawn as an arc on the knob.  Same units as min/max.
  double recMin;
  double recMax;
};

inline constexpr const char* kCompModeLabels[] = {"Opto", "FET", "VCA"};
inline constexpr const char* kCompRatioLabels[] = {"4:1", "8:1", "20:1"};
inline constexpr const char* kDriveModelLabels[] = {"Valve", "Console", "Tape",
                                                    "Clip"};
inline constexpr const char* kEchoDivLabels[] = {
    "1/1", "1/2", "1/4", "1/8D", "1/8", "1/8T", "1/16D", "1/16", "1/16T", "1/32"};
inline constexpr const char* kSpaceModeLabels[] = {"Plate", "Room", "Hall"};

inline constexpr ParamSpec kParamSpecs[P_COUNT] = {
    // Global
    {"power", "Power", "Global", 0.0, 1.0, 1.0, true, false, true, nullptr, {},
     0.0, 1.0},

    // Input
    {"input", "Input", "Input", -18.0, 18.0, 0.0, false, false, false, "dB",
     {}, -6.0, 6.0},
    {"polarity", "Polarity", "Input", 0.0, 1.0, 0.0, true, false, true, nullptr,
     {}, 0.0, 1.0},
    {"hpf_on", "HPF On", "Input", 0.0, 1.0, 1.0, true, false, true, nullptr, {},
     0.0, 1.0},
    {"hpf_freq", "HPF", "Input", 20.0, 350.0, 90.0, false, false, false, "Hz",
     {}, 70.0, 120.0},

    // Compressor
    {"comp_on", "Comp On", "Comp", 0.0, 1.0, 1.0, true, false, true, nullptr,
     {}, 0.0, 1.0},
    {"comp_mode", "Mode", "Comp", 0.0, 2.0, 0.0, true, false, false, nullptr,
     labels(kCompModeLabels), 0.0, 2.0},
    {"comp_amount", "Comp", "Comp", 0.0, 10.0, 3.0, false, false, false, nullptr,
     {}, 3.0, 7.0},
    {"comp_speed", "Speed", "Comp", 0.0, 10.0, 5.0, false, false, false, nullptr,
     {}, 3.0, 7.0},
    {"comp_ratio", "Ratio", "Comp", 0.0, 2.0, 0.0, true, false, false, nullptr,
     labels(kCompRatioLabels), 0.0, 2.0},
    {"comp_mix", "Mix", "Comp", 0.0, 100.0, 100.0, false, false, false, "%", {},
     60.0, 100.0},
    {"comp_auto", "Auto Makeup", "Comp", 0.0, 1.0, 1.0, true, false, true,
     nullptr, {}, 0.0, 1.0},

    // Tone
    {"tone_on", "Tone On", "Tone", 0.0, 1.0, 1.0, true, false, true, nullptr, {},
     0.0, 1.0},
    {"low_gain", "Low", "Tone", -15.0, 15.0, 0.0, false, false, false, "dB", {},
     -3.0, 3.0},
    {"low_freq", "Low Freq", "Tone", 30.0, 400.0, 150.0, false, false, false,
     "Hz", {}, 100.0, 250.0},
    {"low_bell", "Low Bell", "Tone", 0.0, 1.0, 0.0, true, false, true, nullptr,
     {}, 0.0, 1.0},
    {"mid_gain", "Mid", "Tone", -15.0, 15.0, 0.0, false, false, false, "dB", {},
     -4.0, 4.0},
    {"mid_freq", "Mid Freq", "Tone", 200.0, 5000.0, 1200.0, false, false, false,
     "Hz", {}, 500.0, 3000.0},
    {"mid_q", "Mid Q", "Tone", 0.3, 4.0, 1.0, false, false, false, nullptr, {},
     0.7, 1.5},
    {"high_gain", "High", "Tone", -15.0, 15.0, 0.0, false, false, false, "dB",
     {}, -3.0, 4.0},
    {"high_freq", "High Freq", "Tone", 1500.0, 16000.0, 8000.0, false, false,
     false, "Hz", {}, 6000.0, 12000.0},
    {"high_bell", "High Bell", "Tone", 0.0, 1.0, 0.0, true, false, true, nullptr,
     {}, 0.0, 1.0},
    {"air", "Air", "Tone", 0.0, 12.0, 0.0, false, false, false, "dB", {}, 2.0,
     6.0},

    // Drive
    {"drive_on", "Drive On", "Drive", 0.0, 1.0, 0.0, true, false, true, nullptr,
     {}, 0.0, 1.0},
    {"drive_model", "Model", "Drive", 0.0, 3.0, 0.0, true, false, false, nullptr,
     labels(kDriveModelLabels), 0.0, 3.0},
    {"drive_amount", "Drive", "Drive", 0.0, 10.0, 0.0, false, false, false,
     nullptr, {}, 1.0, 5.0},
    {"drive_char", "Character", "Drive", 0.0, 10.0, 5.0, false, false, false,
     nullptr, {}, 3.0, 7.0},
    {"drive_mix", "Mix", "Drive", 0.0, 100.0, 100.0, false, false, false, "%",
     {}, 50.0, 100.0},

    // De-esser
    {"deess_on", "De-Ess On", "De-Ess", 0.0, 1.0, 1.0, true, false, true,
     nullptr, {}, 0.0, 1.0},
    {"sib_freq", "Sibilance Freq", "De-Ess", 4000.0, 10000.0, 6500.0, false, false,
     false, "Hz", {}, 5500.0, 8000.0},
    {"sib_amount", "Sibilance Amount", "De-Ess", 0.0, 10.0, 2.0, false, false, false,
     nullptr, {}, 2.0, 6.0},
    {"harsh_freq", "Harshness Freq", "De-Ess", 1500.0, 5000.0, 3000.0, false, false,
     false, "Hz", {}, 2500.0, 3500.0},
    {"harsh_amount", "Harshness Amount", "De-Ess", 0.0, 10.0, 1.0, false, false,
     false, nullptr, {}, 1.0, 4.0},

    // Echo
    {"echo_on", "Echo On", "Echo", 0.0, 1.0, 0.0, true, false, true, nullptr,
     {}, 0.0, 1.0},
    {"echo_sync", "Sync", "Echo", 0.0, 1.0, 1.0, true, false, true, nullptr, {},
     0.0, 1.0},
    {"echo_div", "Division", "Echo", 0.0, 9.0, 3.0, true, false, false, nullptr,
     labels(kEchoDivLabels), 0.0, 9.0},
    {"echo_time", "Time", "Echo", 20.0, 4000.0, 375.0, false, false, false,
     "ms", {}, 200.0, 600.0},
    {"echo_feedback", "Feedback", "Echo", 0.0, 95.0, 30.0, false, false, false,
     "%", {}, 20.0, 45.0},
    {"echo_tone", "Tone", "Echo", 0.0, 100.0, 50.0, false, false, false, "%",
     {}, 35.0, 65.0},
    {"echo_width", "Width", "Echo", 0.0, 100.0, 50.0, false, false, false, "%",
     {}, 30.0, 80.0},
    {"echo_mod", "Mod", "Echo", 0.0, 100.0, 20.0, false, false, false, "%", {},
     0.0, 40.0},
    {"echo_duck", "Duck", "Echo", 0.0, 100.0, 60.0, false, false, false, "%",
     {}, 40.0, 80.0},
    {"echo_lofi", "Lo-Fi", "Echo", 0.0, 1.0, 0.0, true, false, true, nullptr, {},
     0.0, 1.0},
    {"echo_mix", "Mix", "Echo", 0.0, 100.0, 0.0, false, false, false, "%", {},
     10.0, 30.0},
    {"tempo", "Tempo", "Echo", 40.0, 300.0, 120.0, false, false, false, "BPM",
     {}, 80.0, 160.0},

    // Space
    {"space_on", "Space On", "Space", 0.0, 1.0, 0.0, true, false, true, nullptr,
     {}, 0.0, 1.0},
    {"space_mode", "Mode", "Space", 0.0, 2.0, 0.0, true, false, false, nullptr,
     labels(kSpaceModeLabels), 0.0, 2.0},
    {"space_size", "Size", "Space", 0.0, 100.0, 50.0, false, false, false, "%",
     {}, 30.0, 70.0},
    {"space_decay", "Decay", "Space", 0.3, 8.0, 1.4, false, false, false, "s",
     {}, 0.8, 2.5},
    {"space_predelay", "Pre-Delay", "Space", 0.0, 150.0, 25.0, false, false,
     false, "ms", {}, 15.0, 40.0},
    {"space_tone", "Tone", "Space", 0.0, 100.0, 50.0, false, false, false, "%",
     {}, 35.0, 65.0},
    {"space_width", "Width", "Space", 0.0, 100.0, 70.0, false, false, false,
     "%", {}, 50.0, 100.0},
    {"space_duck", "Duck", "Space", 0.0, 100.0, 40.0, false, false, false, "%",
     {}, 20.0, 60.0},
    {"space_mix", "Mix", "Space", 0.0, 100.0, 0.0, false, false, false, "%", {},
     8.0, 25.0},

    // Output
    {"output", "Output", "Output", -18.0, 18.0, 0.0, false, false, false, "dB",
     {}, -3.0, 3.0},
    {"limit_on", "Limit", "Output", 0.0, 1.0, 1.0, true, false, true, nullptr,
     {}, 0.0, 1.0},
};

static_assert(sizeof(kParamSpecs) / sizeof(kParamSpecs[0]) == P_COUNT,
              "kParamSpecs must cover every parameter id");

// Values, meters and dirty flags shared between the audio thread, the host
// thread and the editor.  Everything is atomic so the editor can run on the
// GUI thread without locks.
class Shared {
public:
  Shared() {
    for (uint32_t p = 0; p < P_COUNT; ++p) {
      values_[p].store(kParamSpecs[p].def, std::memory_order_relaxed);
      dirty_[p].store(false, std::memory_order_relaxed);
    }
    for (auto* m : {&compGrDb, &sibGrDb, &harshGrDb, &limGrDb, &inPeakDb,
                    &outPeakDb, &inPeakLDb, &inPeakRDb, &outPeakLDb,
                    &outPeakRDb}) {
      m->store(-120.0f, std::memory_order_relaxed);
    }
    bpm.store(120.0f, std::memory_order_relaxed);
    hostTempo.store(false, std::memory_order_relaxed);
    compare.store(false, std::memory_order_relaxed);
    inputLearnActive.store(false, std::memory_order_relaxed);
    sibLearnActive.store(false, std::memory_order_relaxed);
    inputLearnPeakDb.store(-120.0f, std::memory_order_relaxed);
    sibLearnHz.store(6500.0f, std::memory_order_relaxed);
    sibLearnEnergyDb.store(-120.0f, std::memory_order_relaxed);
    sibLearnValid.store(false, std::memory_order_relaxed);
  }

  double get(uint32_t p) const noexcept {
    return values_[p].load(std::memory_order_relaxed);
  }

  // Clamps to the parameter range and marks the value for the host.
  void set(uint32_t p, double v) noexcept {
    if (!std::isfinite(v)) {
      return;
    }
    const ParamSpec& s = kParamSpecs[p];
    v = std::min(std::max(v, s.min), s.max);
    if (s.stepped) {
      v = static_cast<double>(static_cast<int>(v + (v >= 0 ? 0.5 : -0.5)));
    }
    values_[p].store(v, std::memory_order_relaxed);
    dirty_[p].store(true, std::memory_order_release);
  }

  // Stores a value that came from the host, without marking it dirty.
  void setFromHost(uint32_t p, double v) noexcept {
    if (!std::isfinite(v)) {
      return;
    }
    const ParamSpec& s = kParamSpecs[p];
    v = std::min(std::max(v, s.min), s.max);
    if (s.stepped) {
      v = static_cast<double>(static_cast<int>(v + (v >= 0 ? 0.5 : -0.5)));
    }
    values_[p].store(v, std::memory_order_relaxed);
  }

  bool takeDirty(uint32_t p) noexcept {
    return dirty_[p].exchange(false, std::memory_order_acquire);
  }

  bool isDirty(uint32_t p) const noexcept {
    return dirty_[p].load(std::memory_order_acquire);
  }

  bool anyDirty() const noexcept {
    for (uint32_t p = 0; p < P_COUNT; ++p) {
      if (dirty_[p].load(std::memory_order_acquire)) {
        return true;
      }
    }
    return false;
  }

  std::atomic<float> compGrDb;
  std::atomic<float> sibGrDb;
  std::atomic<float> harshGrDb;
  std::atomic<float> limGrDb;
  std::atomic<float> inPeakDb;
  std::atomic<float> outPeakDb;
  std::atomic<float> inPeakLDb;
  std::atomic<float> inPeakRDb;
  std::atomic<float> outPeakLDb;
  std::atomic<float> outPeakRDb;
  std::atomic<float> bpm;
  std::atomic<bool> hostTempo;
  std::atomic<bool> compare;
  std::atomic<bool> inputLearnActive;
  std::atomic<bool> sibLearnActive;
  std::atomic<float> inputLearnPeakDb;
  std::atomic<float> sibLearnHz;
  std::atomic<float> sibLearnEnergyDb;
  std::atomic<bool> sibLearnValid;

private:
  std::atomic<double> values_[P_COUNT];
  std::atomic<bool> dirty_[P_COUNT];
};

inline Controls controlsFrom(const Shared& s) {
  Controls c;
  c.power = s.get(P_POWER) > 0.5;
  c.compare = s.compare.load(std::memory_order_relaxed);
  c.inputLearnActive = s.inputLearnActive.load(std::memory_order_relaxed);
  c.sibLearnActive = s.sibLearnActive.load(std::memory_order_relaxed);
  c.inputDb = static_cast<float>(s.get(P_INPUT));
  c.polarity = s.get(P_POLARITY) > 0.5;
  c.hpfOn = s.get(P_HPF_ON) > 0.5;
  c.hpfHz = static_cast<float>(s.get(P_HPF_FREQ));

  c.compOn = s.get(P_COMP_ON) > 0.5;
  c.compMode = static_cast<int>(s.get(P_COMP_MODE) + 0.5);
  c.compAmount = static_cast<float>(s.get(P_COMP_AMOUNT));
  c.compSpeed = static_cast<float>(s.get(P_COMP_SPEED));
  c.compRatio = static_cast<int>(s.get(P_COMP_RATIO) + 0.5);
  c.compMix = static_cast<float>(s.get(P_COMP_MIX));
  c.compAuto = s.get(P_COMP_AUTO) > 0.5;

  c.toneOn = s.get(P_TONE_ON) > 0.5;
  c.lowGain = static_cast<float>(s.get(P_LOW_GAIN));
  c.lowFreq = static_cast<float>(s.get(P_LOW_FREQ));
  c.lowBell = s.get(P_LOW_BELL) > 0.5;
  c.midGain = static_cast<float>(s.get(P_MID_GAIN));
  c.midFreq = static_cast<float>(s.get(P_MID_FREQ));
  c.midQ = static_cast<float>(s.get(P_MID_Q));
  c.highGain = static_cast<float>(s.get(P_HIGH_GAIN));
  c.highFreq = static_cast<float>(s.get(P_HIGH_FREQ));
  c.highBell = s.get(P_HIGH_BELL) > 0.5;
  c.air = static_cast<float>(s.get(P_AIR));

  c.driveOn = s.get(P_DRIVE_ON) > 0.5;
  c.driveModel = static_cast<int>(s.get(P_DRIVE_MODEL) + 0.5);
  c.driveAmount = static_cast<float>(s.get(P_DRIVE_AMOUNT));
  c.driveChar = static_cast<float>(s.get(P_DRIVE_CHAR));
  c.driveMix = static_cast<float>(s.get(P_DRIVE_MIX));

  c.deessOn = s.get(P_DEESS_ON) > 0.5;
  c.sibFreq = static_cast<float>(s.get(P_SIB_FREQ));
  c.sibAmount = static_cast<float>(s.get(P_SIB_AMOUNT));
  c.harshFreq = static_cast<float>(s.get(P_HARSH_FREQ));
  c.harshAmount = static_cast<float>(s.get(P_HARSH_AMOUNT));

  c.echoOn = s.get(P_ECHO_ON) > 0.5;
  c.echoSync = s.get(P_ECHO_SYNC) > 0.5;
  c.echoDiv = static_cast<int>(s.get(P_ECHO_DIV) + 0.5);
  c.echoTime = static_cast<float>(s.get(P_ECHO_TIME));
  c.echoFeedback = static_cast<float>(s.get(P_ECHO_FEEDBACK));
  c.echoTone = static_cast<float>(s.get(P_ECHO_TONE));
  c.echoWidth = static_cast<float>(s.get(P_ECHO_WIDTH));
  c.echoMod = static_cast<float>(s.get(P_ECHO_MOD));
  c.echoDuck = static_cast<float>(s.get(P_ECHO_DUCK));
  c.echoLofi = s.get(P_ECHO_LOFI) > 0.5;
  c.echoMix = static_cast<float>(s.get(P_ECHO_MIX));
  c.bpm = static_cast<float>(s.get(P_TEMPO));

  c.spaceOn = s.get(P_SPACE_ON) > 0.5;
  c.spaceMode = static_cast<int>(s.get(P_SPACE_MODE) + 0.5);
  c.spaceSize = static_cast<float>(s.get(P_SPACE_SIZE));
  c.spaceDecay = static_cast<float>(s.get(P_SPACE_DECAY));
  c.spacePreDelay = static_cast<float>(s.get(P_SPACE_PREDELAY));
  c.spaceTone = static_cast<float>(s.get(P_SPACE_TONE));
  c.spaceWidth = static_cast<float>(s.get(P_SPACE_WIDTH));
  c.spaceDuck = static_cast<float>(s.get(P_SPACE_DUCK));
  c.spaceMix = static_cast<float>(s.get(P_SPACE_MIX));

  c.outputDb = static_cast<float>(s.get(P_OUTPUT));
  c.limitOn = s.get(P_LIMIT_ON) > 0.5;
  return c;
}

} // namespace vocal
