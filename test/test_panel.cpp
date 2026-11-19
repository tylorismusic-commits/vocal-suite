// Vocal Suite — panel smoke test
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cairo.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

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

  // Preset filters and the filtered list apply a matching preset.
  state.presetOpen = 1;
  state.filterStyle = 0;  // Rock / Metal
  state.filterVoice = -1;
  state.filterIntensity = -1;
  cairo_surface_t* filterSurface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kPanelWidth, kPanelHeight);
  cairo_t* filterCr = cairo_create(filterSurface);
  panel.draw(filterCr, shared, state);  // rebuilds the list geometry
  cairo_destroy(filterCr);
  cairo_surface_destroy(filterSurface);
  panel.pointerDown(shared, state, 200, 164, false);
  assert(state.presetIndex >= 0);
  assert(std::strcmp(factoryPresets()[state.presetIndex].category,
                     "Rock / Metal") == 0);

  // Every page tab selects its page.
  for (int tab = 0; tab < 8; ++tab) {
    state.tab = 0;
    panel.pointerDown(shared, state, 24 + tab * 124 + 59, 94, false);
    assert(state.tab == tab);
  }

  // Every stage IN/OUT toggle flips its own parameter.
  {
    const struct {
      int page;
      uint32_t param;
    } kStages[7] = {{0, P_HPF_ON},   {1, P_COMP_ON}, {2, P_TONE_ON},
                    {3, P_DRIVE_ON}, {4, P_DEESS_ON}, {5, P_ECHO_ON},
                    {6, P_SPACE_ON}};
    for (const auto& s : kStages) {
      state.tab = s.page;
      const double before = shared.get(s.param);
      panel.pointerDown(shared, state, 796, 157, false);
      panel.pointerUp(shared);
      assert(shared.get(s.param) != before);
      panel.pointerDown(shared, state, 796, 157, false);
      panel.pointerUp(shared);
      assert(shared.get(s.param) == before);
    }
  }

  // Mode and effect choices select the requested value.
  {
    state.tab = 1;
    panel.pointerDown(shared, state, 297, 174, false);
    assert(shared.get(P_COMP_MODE) == 2.0);
    panel.pointerDown(shared, state, 105, 174, false);
    assert(shared.get(P_COMP_MODE) == 0.0);

    state.tab = 3;
    panel.pointerDown(shared, state, 379, 174, false);
    assert(shared.get(P_DRIVE_MODEL) == 3.0);

    state.tab = 6;
    panel.pointerDown(shared, state, 297, 174, false);
    assert(shared.get(P_SPACE_MODE) == 2.0);

    state.tab = 5;
    panel.pointerDown(shared, state, 93 + 9 * 72, 212, false);
    assert(shared.get(P_ECHO_DIV) == 9.0);
  }

  // Ordinary toggles and one-shot buttons.
  state.tab = 0;
  const double polarity = shared.get(P_POLARITY);
  panel.pointerDown(shared, state, 670, 264, false);
  assert(shared.get(P_POLARITY) != polarity);
  panel.pointerDown(shared, state, 675, 314, false);
  assert(state.inputLearning);

  state.tab = 1;
  const double autoMakeup = shared.get(P_COMP_AUTO);
  panel.pointerDown(shared, state, 796, 193, false);
  assert(shared.get(P_COMP_AUTO) != autoMakeup);

  state.tab = 2;
  const double lowBell = shared.get(P_LOW_BELL);
  panel.pointerDown(shared, state, 186, 559, false);
  assert(shared.get(P_LOW_BELL) != lowBell);

  state.tab = 4;
  panel.pointerDown(shared, state, 832, 414, false);
  assert(state.sibLearning);
  shared.sibLearnHz.store(7200.0f);
  shared.sibLearnValid.store(true);
  for (int i = 0; i < 61; ++i) {
    panel.tick(shared, state);
  }
  assert(shared.get(P_SIB_FREQ) > 7100.0 && shared.get(P_SIB_FREQ) < 7300.0);

  state.tab = 5;
  const double sync = shared.get(P_ECHO_SYNC);
  panel.pointerDown(shared, state, 105, 171, false);
  assert(shared.get(P_ECHO_SYNC) != sync);

  state.tab = 7;
  const double limit = shared.get(P_LIMIT_ON);
  panel.pointerDown(shared, state, 700, 314, false);
  assert(shared.get(P_LIMIT_ON) != limit);

  // Power is a normal toggle.
  const double power = shared.get(P_POWER);
  panel.pointerDown(shared, state, 44, 34, false);
  assert(shared.get(P_POWER) != power);
  panel.pointerDown(shared, state, 44, 34, false);
  assert(shared.get(P_POWER) == power);

  // Preset bar: arrows change presets, the name opens the list, filters
  // narrow it, a row applies a preset, and the wheel scrolls.
  state.presetOpen = 0;
  const int presetBefore = state.presetIndex;
  panel.pointerDown(shared, state, 386, 32, false);
  assert(state.presetIndex != presetBefore);
  panel.pointerDown(shared, state, 88, 32, false);
  panel.pointerDown(shared, state, 200, 32, false);
  assert(state.presetOpen);
  panel.pointerDown(shared, state, 140 + 66 + 31, 77, false);
  assert(state.filterStyle == 0);
  panel.pointerDown(shared, state, 200, 164, false);
  assert(state.presetIndex >= 0);
  assert(std::strcmp(factoryPresets()[state.presetIndex].category,
                     "Rock / Metal") == 0);
  state.presetOpen = 1;
  state.presetScroll = 0;
  panel.wheel(shared, state, 200, 300, -1.0);
  assert(state.presetScroll >= 1);
  state.presetOpen = 0;

  // Right-click opens text entry; Enter applies; double-click resets.
  state.tab = 0;
  shared.set(P_INPUT, 0.0);
  assert(panel.contextDown(shared, state, 180, 300));
  assert(state.editParam == static_cast<int>(P_INPUT));
  panel.keyEvent(shared, state, 0, "1");
  panel.keyEvent(shared, state, 0, "2");
  panel.keyEvent(shared, state, 13, "");
  assert(std::fabs(shared.get(P_INPUT) - 12.0) < 1.0e-9);
  panel.pointerDown(shared, state, 180, 300, false);
  panel.pointerUp(shared);
  panel.pointerDown(shared, state, 180, 300, false);
  panel.pointerUp(shared);
  assert(std::fabs(shared.get(P_INPUT) - 0.0) < 1.0e-9);

  // Limiter ceiling knob is interactive.
  state.tab = 7;
  const double ceilingBefore = shared.get(P_LIMIT_CEILING);
  panel.pointerDown(shared, state, 700, 390, false);
  panel.pointerMove(shared, state, 700, 360, false);
  panel.pointerUp(shared);
  assert(std::fabs(shared.get(P_LIMIT_CEILING) - ceilingBefore) > 1.0e-6);

  // In tempo-sync mode the Time knob is display-only.
  state.tab = 5;
  shared.set(P_ECHO_SYNC, 1.0);
  panel.pointerDown(shared, state, 100, 350, false);
  assert(!panel.dragging());
  shared.set(P_ECHO_SYNC, 0.0);

  // COMPARE is a sticky dry monitor, not a momentary-only button.
  state.presetOpen = 0;
  state.tab = 0;
  assert(!shared.compare.load());
  panel.pointerDown(shared, state, 850, 30, false);
  panel.pointerUp(shared);
  assert(shared.compare.load());
  panel.pointerDown(shared, state, 850, 30, false);
  panel.pointerUp(shared);
  assert(!shared.compare.load());

  // A chain tab click changes the page.
  state.tab = 0;
  panel.pointerDown(shared, state, 24 + 124 + 40, 90, false);
  assert(state.tab == 1);


  {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "vocal-suite-preset-test";
    fs::remove_all(root);
    fs::create_directories(root / "config");
    fs::create_directories(root / "home");
    const std::string oldXdg =
        std::getenv("XDG_CONFIG_HOME") ? std::getenv("XDG_CONFIG_HOME") : "";
    const std::string oldHome =
        std::getenv("HOME") ? std::getenv("HOME") : "";
    setenv("XDG_CONFIG_HOME", (root / "config").string().c_str(), 1);
    setenv("HOME", (root / "home").string().c_str(), 1);

    Shared us;
    PanelState st;
    Panel p;
    p.loadFont("./assets/fonts");
    us.set(P_INPUT, 0.63);
    const double saved = us.get(P_INPUT);
    p.pointerDown(us, st, 237, 30, false);
    p.pointerUp(us);
    p.pointerDown(us, st, 90, 470, false);
    p.pointerUp(us);
    assert(st.savePreset);
    p.keyEvent(us, st, 0, "My Vocal Preset");
    p.keyEvent(us, st, 13, nullptr);
    assert(st.userPresets.size() == 1);
    assert(st.presetIndex == presetCount());
    us.set(P_INPUT, 0.1);
    p.applyPreset(us, st, st.presetIndex);
    assert(std::fabs(us.get(P_INPUT) - saved) < 1e-9);

    p.pointerDown(us, st, 237, 30, false);
    p.pointerUp(us);
    p.pointerDown(us, st, 90, 500, false);
    p.pointerUp(us);
    p.keyEvent(us, st, 0, "Renamed");
    p.keyEvent(us, st, 13, nullptr);
    assert(st.userPresets.size() == 1 && st.userPresets[0].name == "Renamed");

    p.pointerDown(us, st, 237, 30, false);
    p.pointerUp(us);
    p.pointerDown(us, st, 150, 470, false);
    p.pointerUp(us);
    assert(st.userPresets.size() == 2);

    p.pointerDown(us, st, 237, 30, false);
    p.pointerUp(us);
    p.pointerDown(us, st, 230, 500, false);
    p.pointerUp(us);
    const fs::path exported = userPresetExportDir() /
                              (std::string("Renamed copy") +
                               kUserPresetExt);
    assert(fs::exists(exported));

    p.pointerDown(us, st, 237, 30, false);
    p.pointerUp(us);
    p.pointerDown(us, st, 160, 500, false);
    p.pointerUp(us);
    p.pointerDown(us, st, 160, 500, false);
    p.pointerUp(us);
    assert(st.userPresets.size() == 1);
    assert(p.importDroppedFile(us, st, exported.string()));
    assert(st.userPresets.size() == 2);

    if (!oldXdg.empty()) setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    else unsetenv("XDG_CONFIG_HOME");
    if (!oldHome.empty()) setenv("HOME", oldHome.c_str(), 1);
    else unsetenv("HOME");
    fs::remove_all(root);
  }

  std::printf("vocal panel: %d presets, pages draw\n", presetCount());
  return 0;
}
