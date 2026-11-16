// hearth-dsp — sidechain.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace hdsp {

// Where a compressor's detector taps the signal.  Feedback models put the gain
// cell inside the detector loop; feed-forward models tap the input.
enum class Topology { FeedForward, Feedback };

// Per-sample external side-chain input.  `active` is false when the host has
// not routed anything, in which case models detect from their own input.
struct SidechainFrame {
  float left = 0.0f;
  float right = 0.0f;
  bool active = false;
};

} // namespace hdsp
