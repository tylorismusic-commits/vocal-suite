// Vocal Suite — DSP controls
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace vocal {

// The shared control surface.  Values are physical units (dB, Hz, %, ms, s)
// rather than normalised 0..1, so the UI, the host parameter text and the DSP
// all agree without three different mappings.
struct Controls {
  bool power = true;
  bool compare = false;  // hold-to-compare dry monitoring, not a parameter
  bool inputLearnActive = false;
  bool sibLearnActive = false;

  // Input
  float inputDb = 0.0f;
  bool polarity = false;
  bool hpfOn = true;
  float hpfHz = 90.0f;

  // Compressor
  bool compOn = true;
  int compMode = 0;      // 0 Opto, 1 FET, 2 VCA
  float compAmount = 3.0f;  // 0..10
  float compSpeed = 5.0f;   // 0 fast .. 10 slow
  int compRatio = 0;        // 0 4:1, 1 8:1, 2 20:1
  float compMix = 100.0f;   // percent
  bool compAuto = true;

  // Tone
  bool toneOn = true;
  float lowGain = 0.0f;
  float lowFreq = 150.0f;
  bool lowBell = false;
  float midGain = 0.0f;
  float midFreq = 1200.0f;
  float midQ = 1.0f;
  float highGain = 0.0f;
  float highFreq = 8000.0f;
  bool highBell = false;
  float air = 0.0f;

  // Drive
  bool driveOn = false;
  int driveModel = 0;       // 0 Valve, 1 Console, 2 Tape, 3 Clip
  float driveAmount = 0.0f; // 0..10
  float driveChar = 5.0f;   // 0..10
  float driveMix = 100.0f;  // percent

  // De-esser
  bool deessOn = true;
  float sibFreq = 6500.0f;
  float sibAmount = 2.0f;   // 0..10
  float harshFreq = 3000.0f;
  float harshAmount = 1.0f; // 0..10

  // Echo
  bool echoOn = false;
  bool echoSync = true;
  int echoDiv = 3;          // index into the division table
  float echoTime = 375.0f;  // ms
  float echoFeedback = 30.0f;
  float echoTone = 50.0f;
  float echoWidth = 50.0f;
  float echoMod = 20.0f;
  float echoDuck = 60.0f;
  bool echoLofi = false;
  float echoMix = 0.0f;
  float bpm = 120.0f;

  // Space
  bool spaceOn = false;
  int spaceMode = 0;        // 0 Plate, 1 Room, 2 Hall
  float spaceSize = 50.0f;
  float spaceDecay = 1.4f;
  float spacePreDelay = 25.0f;
  float spaceTone = 50.0f;
  float spaceWidth = 70.0f;
  float spaceDuck = 40.0f;
  float spaceMix = 0.0f;

  // Output
  float outputDb = 0.0f;
  bool limitOn = true;
  float limitCeilingDb = -0.3f;
};

} // namespace vocal
