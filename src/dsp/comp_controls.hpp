// Vocal Suite — controls for the adapted compressor models
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The FET, optical and VCA cores are adapted from Comp Suite.  They keep their
// own control struct so their tested behaviour is not disturbed by the vocal
// strip's physical-unit surface.  comp.hpp translates between the two.
#pragma once

namespace vocal {

struct CompControls {
  int model = 0;
  bool enabled = true;
  float input = 0.5f;
  float threshold = 0.5f;
  float ratio = 0.5f;
  float attack = 0.5f;
  float release = 0.5f;
  float makeup = 0.5f;
  float output = 0.5f;
  float mix = 1.0f;
  float sidechainHpf = 0.0f;
  float distortion = 0.0f;
  bool limiterOn = false;
  float limitThreshold = 0.5f;
  bool autoMakeup = false;
  bool externalSidechain = false;
  bool truePeak = false;
  float link = 1.0f;
  bool vcaAutoRelease = false;
  float knee = 0.5f;
  float satDrive = 0.5f;
  float attackMode = 0.0f;
  float stereoMode = 5.0f;
  float beef = 0.35f;
  float air = 0.2f;
  float drumType = 2.0f;
  float easyMode = 0.0f;
  float easyChar = 0.0f;
  float satType = 1.0f;
  float oversample = 0.0f;
};

} // namespace vocal
