// Vocal Suite — panel smoke test
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cairo.h>

#include <cassert>
#include <cstdio>

#include "presets.hpp"
#include "ui/panel.hpp"

int main() {
  using namespace vocal;
  Panel panel;
  panel.loadFont("./assets/fonts");
  Shared shared;
  PanelState state;

  // Draw every page into an image surface.
  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kPanelWidth, kPanelHeight);
  cairo_t* cr = cairo_create(surface);
  for (int tab = 0; tab < 8; ++tab) {
    state.tab = tab;
    panel.draw(cr, shared, state);
  }
  cairo_destroy(cr);
  cairo_surface_destroy(surface);

  // Preset apply changes parameters.
  panel.applyPreset(shared, state, 0);
  assert(state.presetIndex == 0);
  assert(shared.get(P_COMP_MODE) == 1.0);
  assert(shared.get(P_ECHO_ON) == 1.0);

  // A knob drag changes the value.
  state.tab = 0;
  const double before = shared.get(P_INPUT);
  panel.pointerDown(shared, state, 180, 300, false);
  panel.pointerMove(shared, state, 180, 250, false);
  panel.pointerUp(shared);
  assert(shared.get(P_INPUT) != before);

  // The SET LEVEL button starts a learn, and tick applies the result.
  state.tab = 0;
  shared.set(P_INPUT, 0.0);
  panel.pointerDown(shared, state, 650, 310, false);
  assert(state.inputLearning);
  shared.inputLearnPeakDb.store(-20.0f);
  for (int i = 0; i < 61; ++i) {
    panel.tick(shared, state);
  }
  assert(!state.inputLearning);
  assert(shared.get(P_INPUT) > 7.0);

  // A chain tab click changes the page.
  state.tab = 0;
  panel.pointerDown(shared, state, 24 + 124 + 40, 90, false);
  assert(state.tab == 1);

  std::printf("vocal panel: %d presets, pages draw\n", presetCount());
  return 0;
}
