// Vocal Suite — panel
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cairo.h>

#include <chrono>
#include <string>
#include <vector>

#include "params.hpp"

namespace vocal {

constexpr int kPanelWidth = 1040;
constexpr int kPanelHeight = 640;

struct PanelState {
  PanelState() {
    for (float& v : inHistory) v = -120.0f;
    for (float& v : outHistory) v = -120.0f;
  }

  int tab = 0;             // 0 INPUT .. 7 OUT, UI-only
  int presetIndex = -1;
  bool presetDirty = false;
  int presetOpen = 0;
  int presetScroll = 0;
  int filterStyle = -1;
  int filterVoice = -1;
  int filterIntensity = -1;
  float inLevelL = -120.0f;
  float inLevelR = -120.0f;
  float outLevelL = -120.0f;
  float outLevelR = -120.0f;
  float compGr = 0.0f;
  float sibGr = 0.0f;
  float harshGr = 0.0f;
  float limGr = 0.0f;
  float outPeakHold = -120.0f;
  bool inputLearning = false;
  int inputLearnTicks = 0;
  bool sibLearning = false;
  int sibLearnTicks = 0;
  int editParam = -1;
  std::string editText;
  static constexpr int kHistory = 240;
  float inHistory[kHistory] = {};
  int inHistoryHead = 0;
  float outHistory[kHistory] = {};
  int outHistoryHead = 0;
};

class Panel {
public:
  Panel();
  ~Panel();
  Panel(const Panel&) = delete;
  Panel& operator=(const Panel&) = delete;

  void loadFont(const std::string& dir);
  void tick(Shared& shared, PanelState& state) const;

  void draw(cairo_t* cr, Shared& shared, const PanelState& state);

  bool pointerDown(Shared& shared, PanelState& state, double x, double y,
                   bool fine);
  bool contextDown(Shared& shared, PanelState& state, double x, double y);
  bool keyEvent(Shared& shared, PanelState& state, int key, const char* text);
  bool pointerMove(Shared& shared, PanelState& state, double x, double y,
                   bool fine);
  void pointerUp(Shared& shared);
  bool handlePresetClick(Shared& shared, PanelState& state, double x, double y);
  void applyPreset(Shared& shared, PanelState& state, int index) const;
  void drawPresetBar(cairo_t* cr, const Shared& shared,
                     const PanelState& state) const;
  void drawPresetList(cairo_t* cr, const Shared& shared,
                      const PanelState& state) const;
  bool wheel(Shared& shared, PanelState& state, double x, double y, double dy);

  bool dragging() const { return dragParam_ >= 0; }

private:
  enum class Kind {
    Knob,
    Power,
    Toggle,
    Choice,
    PageTab,
    Button,
    Fader,
  };

  struct Widget {
    Kind kind = Kind::Knob;
    uint32_t param = 0;
    double value = 0.0;
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
    double r = 0.0;
    const char* label = "";
  };

  void buildLayout(const Shared& shared, int tab);
  void drawHeader(cairo_t* cr, const Shared& shared,
                  const PanelState& state) const;
  void drawChain(cairo_t* cr, const Shared& shared,
                 const PanelState& state) const;
  void drawBody(cairo_t* cr, Shared& shared, const PanelState& state) const;
  void drawRightRail(cairo_t* cr, const Shared& shared,
                     const PanelState& state) const;
  void drawFooter(cairo_t* cr, const Shared& shared,
                  const PanelState& state) const;
  void drawWidget(cairo_t* cr, const Widget& w, const Shared& shared) const;
  void drawKnob(cairo_t* cr, const Widget& w, double value,
                const ParamSpec& spec) const;
  void drawToggle(cairo_t* cr, const Widget& w, bool on) const;
  void drawChoice(cairo_t* cr, const Widget& w, int selected) const;
  void drawPageTab(cairo_t* cr, const Widget& w, bool active,
                   bool moduleOn) const;
  void drawMeter(cairo_t* cr, double x, double y, double w, double h,
                 double dbL, double dbR, const char* label) const;
  void drawGrBar(cairo_t* cr, double x, double y, double w, double h,
                 double grDb, double maxDb, const char* label) const;
  void drawHistory(cairo_t* cr, const float* history, int head, double x,
                   double y, double w, double h, const char* label,
                   double targetLo, double targetHi) const;
  void drawEqCurve(cairo_t* cr, const Shared& shared, double x, double y,
                   double w, double h) const;
  void drawCompCurve(cairo_t* cr, const Shared& shared, double x, double y,
                     double w, double h) const;
  void drawDriveCurve(cairo_t* cr, const Shared& shared, double x, double y,
                      double w, double h) const;
  void text(cairo_t* cr, const char* s, double x, double y, double size,
            int align, double r, double g, double b) const;

  bool hit(const Widget& w, double x, double y) const;
  const Widget* find(uint32_t param, Kind kind) const;
  void pageInput();
  void pageComp();
  void pageTone();
  void pageDrive();
  void pageDeEss();
  void pageEcho();
  void pageSpace();
  void pageOut();

  std::vector<Widget> widgets_;
  int dragParam_ = -1;
  Kind dragKind_ = Kind::Knob;
  double dragStartY_ = 0.0;
  double dragStartValue_ = 0.0;
  double dragStartX_ = 0.0;
  uint32_t lastClickParam_ = 0;
  bool hasLastClick_ = false;
  std::chrono::steady_clock::time_point lastClickTime_{};
  cairo_font_face_t* font_ = nullptr;
  void* ft_ = nullptr;
  void* ftFace_ = nullptr;
};

} // namespace vocal
