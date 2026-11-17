// Vocal Suite — render a panel preview PNG
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cairo.h>

#include <string>

#include "presets.hpp"
#include "ui/panel.hpp"

int main(int argc, char** argv) {
  using namespace vocal;
  const int tab = argc > 1 ? std::stoi(argv[1]) : 0;
  const std::string out = argc > 2 ? argv[2] : "vocal-suite.png";

  Panel panel;
  panel.loadFont("./assets/fonts");
  Shared shared;
  PanelState state;
  state.tab = tab;
  state.presetOpen = argc > 3 ? std::stoi(argv[3]) : 0;
  panel.applyPreset(shared, state, 0);
  if (argc > 4 && std::stoi(argv[4]) != 0) {
    shared.compare.store(true, std::memory_order_relaxed);
  }
  if (argc > 5 && std::stoi(argv[5]) != 0) {
    state.editParam = static_cast<int>(P_INPUT);
    state.editText = "12.5";
  }
  if (argc > 6 && std::stoi(argv[6]) != 0) {
    shared.bpm.store(90.0f, std::memory_order_relaxed);
    shared.hostTempo.store(true, std::memory_order_relaxed);
  }

  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kPanelWidth, kPanelHeight);
  cairo_t* cr = cairo_create(surface);
  panel.draw(cr, shared, state);
  cairo_destroy(cr);
  cairo_surface_write_to_png(surface, out.c_str());
  cairo_surface_destroy(surface);
  return 0;
}
