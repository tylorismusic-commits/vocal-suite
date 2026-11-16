// Vocal Suite — meters
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace vocal {

struct Meters {
  float grDb = 0.0f;
  float compGrDb = 0.0f;
  float sibGrDb = 0.0f;
  float harshGrDb = 0.0f;
  float limGrDb = 0.0f;
  float inPeakDb = -120.0f;
  float outPeakDb = -120.0f;
  float inPeakLDb = -120.0f;
  float inPeakRDb = -120.0f;
  float outPeakLDb = -120.0f;
  float outPeakRDb = -120.0f;
  float outVuDb = -120.0f;
  float scPeakDb = -120.0f;
  bool scActive = false;
  float bpm = 120.0f;
  float inputLearnPeakDb = -120.0f;
  float sibLearnHz = 6500.0f;
  float sibLearnEnergyDb = -120.0f;
  bool sibLearnValid = false;
};

} // namespace vocal
