// Vocal Suite — panel
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/panel.hpp"

#include "presets.hpp"
#include "dsp/drive.hpp"

#include <cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace vocal {

namespace {

constexpr double kPiD = 3.14159265358979323846;

struct Rgb {
  double r, g, b;
};

const Rgb kBgTop{0.115, 0.120, 0.128};
const Rgb kBgBottom{0.035, 0.038, 0.042};
const Rgb kPanel{0.145, 0.150, 0.158};
const Rgb kPanelLight{0.190, 0.196, 0.205};
const Rgb kEdge{0.055, 0.058, 0.062};
const Rgb kText{0.900, 0.890, 0.865};
const Rgb kDim{0.560, 0.550, 0.530};
const Rgb kAccent{0.900, 0.560, 0.270};
const Rgb kGreen{0.340, 0.840, 0.450};
const Rgb kAmber{0.960, 0.800, 0.280};
const Rgb kRed{1.000, 0.300, 0.240};
const Rgb kMeterFace{0.055, 0.058, 0.060};

constexpr uint32_t kCompareButton = P_COUNT;
constexpr uint32_t kLearnLevelButton = P_COUNT + 1;
constexpr uint32_t kLearnSibButton = P_COUNT + 2;

const char* const kPageNames[8] = {"INPUT", "COMP", "TONE", "DRIVE",
                                   "DE-ESS", "ECHO", "SPACE", "OUTPUT"};
const uint32_t kPageOnParam[8] = {P_HPF_ON,   P_COMP_ON, P_TONE_ON,
                                  P_DRIVE_ON, P_DEESS_ON, P_ECHO_ON,
                                  P_SPACE_ON, P_LIMIT_ON};

Rgb mix(Rgb a, Rgb b, double t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
          a.b + (b.b - a.b) * t};
}

void setRgb(cairo_t* cr, Rgb c, double alpha = 1.0) {
  cairo_set_source_rgba(cr, c.r, c.g, c.b, alpha);
}

double clamp01(double v) { return std::min(std::max(v, 0.0), 1.0); }

double knobAngle(double f) { return (135.0 + 270.0 * f) * kPiD / 180.0; }

void roundRect(cairo_t* cr, double x, double y, double w, double h, double r) {
  r = std::min(r, std::min(w, h) * 0.5);
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -kPiD / 2, 0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0, kPiD / 2);
  cairo_arc(cr, x + r, y + h - r, r, kPiD / 2, kPiD);
  cairo_arc(cr, x + r, y + r, r, kPiD, 3 * kPiD / 2);
  cairo_close_path(cr);
}

std::string formatValue(const ParamSpec& s, double value) {
  char buf[64];
  if (s.labels.names) {
    const int n = std::clamp(static_cast<int>(value + 0.5), 0,
                             s.labels.count - 1);
    return s.labels.names[n];
  }
  if (s.toggled) {
    return value > 0.5 ? "On" : "Off";
  }
  if (s.unit) {
    if (std::strcmp(s.unit, "dB") == 0) {
      std::snprintf(buf, sizeof(buf), "%+.1f dB", value);
    } else if (std::strcmp(s.unit, "Hz") == 0) {
      if (value >= 1000.0) {
        std::snprintf(buf, sizeof(buf), "%.2f kHz", value / 1000.0);
      } else {
        std::snprintf(buf, sizeof(buf), "%.0f Hz", value);
      }
    } else if (std::strcmp(s.unit, "%") == 0) {
      std::snprintf(buf, sizeof(buf), "%.0f%%", value);
    } else if (std::strcmp(s.unit, "ms") == 0) {
      std::snprintf(buf, sizeof(buf), "%.0f ms", value);
    } else if (std::strcmp(s.unit, "s") == 0) {
      std::snprintf(buf, sizeof(buf), "%.2f s", value);
    } else if (std::strcmp(s.unit, "BPM") == 0) {
      std::snprintf(buf, sizeof(buf), "%.0f BPM", value);
    } else {
      std::snprintf(buf, sizeof(buf), "%.2f", value);
    }
    return buf;
  }
  std::snprintf(buf, sizeof(buf), "%.1f", value);
  return buf;
}

bool presetMatches(const Preset& p, const PanelState& state) {
  if (state.filterStyle >= 0) {
    static const char* const kStyles[5] = {"Rock / Metal", "Pop / Rap / Podcast",
                                           "Screams / Extreme", "Clean / Acoustic",
                                           "Utility"};
    if (std::strcmp(p.category, kStyles[state.filterStyle]) != 0) {
      return false;
    }
  }
  if (state.filterVoice >= 0 &&
      static_cast<int>(p.voice) != state.filterVoice) {
    return false;
  }
  if (state.filterIntensity >= 0 &&
      static_cast<int>(p.intensity) != state.filterIntensity) {
    return false;
  }
  return true;
}

std::vector<int> visiblePresets(const PanelState& state) {
  std::vector<int> v;
  for (int i = 0; i < presetCount(); ++i) {
    if (presetMatches(factoryPresets()[i], state)) {
      v.push_back(i);
    }
  }
  std::stable_sort(v.begin(), v.end(), [](int a, int b) {
    const int c = std::strcmp(factoryPresets()[a].category,
                              factoryPresets()[b].category);
    if (c != 0) {
      return c < 0;
    }
    return std::strcmp(factoryPresets()[a].name, factoryPresets()[b].name) < 0;
  });
  return v;
}

} // namespace

Panel::Panel() = default;

Panel::~Panel() {
  if (font_) {
    cairo_font_face_destroy(font_);
  }
  if (ftFace_) {
    FT_Done_Face(static_cast<FT_Face>(ftFace_));
  }
  if (ft_) {
    FT_Done_FreeType(static_cast<FT_Library>(ft_));
  }
}

void Panel::loadFont(const std::string& dir) {
  if (ft_) {
    return;
  }
  std::string path = dir;
  if (!path.empty() && path.back() != '/') {
    path += '/';
  }
  path += "Jost-ExtraBold.ttf";
  FT_Library lib = nullptr;
  if (FT_Init_FreeType(&lib) != 0) {
    return;
  }
  FT_Face face = nullptr;
  if (FT_New_Face(lib, path.c_str(), 0, &face) != 0) {
    FT_Done_FreeType(lib);
    return;
  }
  cairo_font_face_t* font = cairo_ft_font_face_create_for_ft_face(face, 0);
  if (cairo_font_face_status(font) != CAIRO_STATUS_SUCCESS) {
    cairo_font_face_destroy(font);
    FT_Done_Face(face);
    FT_Done_FreeType(lib);
    return;
  }
  ft_ = lib;
  ftFace_ = face;
  font_ = font;
}

void Panel::text(cairo_t* cr, const char* s, double x, double y, double size,
                 int align, double r, double g, double b) const {
  if (font_) {
    cairo_set_font_face(cr, font_);
  } else {
    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
  }
  cairo_set_font_size(cr, size);
  cairo_set_source_rgb(cr, r, g, b);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  double tx = x;
  if (align == 1) {
    tx = x - e.x_advance * 0.5;
  } else if (align == 2) {
    tx = x - e.x_advance;
  }
  cairo_move_to(cr, tx, y);
  cairo_show_text(cr, s);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void Panel::buildLayout(const Shared& /*shared*/, int tab) {
  widgets_.clear();

  Widget power{};
  power.kind = Kind::Power;
  power.param = P_POWER;
  power.x = 26;
  power.y = 16;
  power.w = 36;
  power.h = 36;
  power.r = 14;
  power.label = "";
  widgets_.push_back(power);

  Widget compare{};
  compare.kind = Kind::Button;
  compare.param = kCompareButton;
  compare.x = 820;
  compare.y = 18;
  compare.w = 88;
  compare.h = 26;
  compare.label = "COMPARE";
  widgets_.push_back(compare);

  for (int i = 0; i < 8; ++i) {
    Widget t{};
    t.kind = Kind::PageTab;
    t.param = kPageOnParam[i];
    t.value = i;
    t.x = 24 + i * 124;
    t.y = 68;
    t.w = 118;
    t.h = 52;
    t.label = kPageNames[i];
    widgets_.push_back(t);
  }

  if (tab < 7) {
    Widget on{};
    on.kind = Kind::Toggle;
    on.param = kPageOnParam[tab];
    on.x = 748;
    on.y = 144;
    on.w = 96;
    on.h = 26;
    on.label = tab == 0 ? "HPF ON" : "ON";
    widgets_.push_back(on);
  }

  switch (tab) {
    case 0: pageInput(); break;
    case 1: pageComp(); break;
    case 2: pageTone(); break;
    case 3: pageDrive(); break;
    case 4: pageDeEss(); break;
    case 5: pageEcho(); break;
    case 6: pageSpace(); break;
    default: pageOut(); break;
  }
}

void Panel::pageInput() {
  Widget in{};
  in.kind = Kind::Knob;
  in.param = P_INPUT;
  in.x = 180;
  in.y = 300;
  in.r = 62;
  widgets_.push_back(in);

  Widget hpf{};
  hpf.kind = Kind::Knob;
  hpf.param = P_HPF_FREQ;
  hpf.x = 440;
  hpf.y = 300;
  hpf.r = 48;
  widgets_.push_back(hpf);

  Widget setLevel{};
  setLevel.kind = Kind::Button;
  setLevel.param = kLearnLevelButton;
  setLevel.x = 620;
  setLevel.y = 300;
  setLevel.w = 110;
  setLevel.h = 28;
  setLevel.label = "SET LEVEL";
  widgets_.push_back(setLevel);

  Widget pol{};
  pol.kind = Kind::Toggle;
  pol.param = P_POLARITY;
  pol.x = 620;
  pol.y = 250;
  pol.w = 100;
  pol.h = 28;
  pol.label = "POLARITY";
  widgets_.push_back(pol);
}

void Panel::pageComp() {
  for (int i = 0; i < 3; ++i) {
    Widget m{};
    m.kind = Kind::Choice;
    m.param = P_COMP_MODE;
    m.value = i;
    m.x = 60 + i * 96;
    m.y = 160;
    m.w = 90;
    m.h = 28;
    m.label = kCompModeLabels[i];
    widgets_.push_back(m);
  }
  Widget amt{};
  amt.kind = Kind::Knob;
  amt.param = P_COMP_AMOUNT;
  amt.x = 180;
  amt.y = 340;
  amt.r = 58;
  widgets_.push_back(amt);
  Widget speed{};
  speed.kind = Kind::Knob;
  speed.param = P_COMP_SPEED;
  speed.x = 350;
  speed.y = 340;
  speed.r = 44;
  widgets_.push_back(speed);
  Widget ratio{};
  ratio.kind = Kind::Knob;
  ratio.param = P_COMP_RATIO;
  ratio.x = 500;
  ratio.y = 340;
  ratio.r = 40;
  widgets_.push_back(ratio);
  Widget mixw{};
  mixw.kind = Kind::Knob;
  mixw.param = P_COMP_MIX;
  mixw.x = 650;
  mixw.y = 340;
  mixw.r = 40;
  widgets_.push_back(mixw);
  Widget autoM{};
  autoM.kind = Kind::Toggle;
  autoM.param = P_COMP_AUTO;
  autoM.x = 748;
  autoM.y = 250;
  autoM.w = 96;
  autoM.h = 26;
  autoM.label = "AUTO";
  widgets_.push_back(autoM);
}

void Panel::pageTone() {
  Widget lg{};
  lg.kind = Kind::Knob;
  lg.param = P_LOW_GAIN;
  lg.x = 120;
  lg.y = 390;
  lg.r = 28;
  widgets_.push_back(lg);
  Widget lf{};
  lf.kind = Kind::Knob;
  lf.param = P_LOW_FREQ;
  lf.x = 110;
  lf.y = 500;
  lf.r = 22;
  widgets_.push_back(lf);
  Widget lb{};
  lb.kind = Kind::Toggle;
  lb.param = P_LOW_BELL;
  lb.x = 155;
  lb.y = 548;
  lb.w = 62;
  lb.h = 22;
  lb.label = "SHELF";
  widgets_.push_back(lb);

  Widget mg{};
  mg.kind = Kind::Knob;
  mg.param = P_MID_GAIN;
  mg.x = 330;
  mg.y = 390;
  mg.r = 28;
  widgets_.push_back(mg);
  Widget mf{};
  mf.kind = Kind::Knob;
  mf.param = P_MID_FREQ;
  mf.x = 305;
  mf.y = 500;
  mf.r = 22;
  widgets_.push_back(mf);
  Widget mq{};
  mq.kind = Kind::Knob;
  mq.param = P_MID_Q;
  mq.x = 365;
  mq.y = 500;
  mq.r = 22;
  widgets_.push_back(mq);

  Widget hg{};
  hg.kind = Kind::Knob;
  hg.param = P_HIGH_GAIN;
  hg.x = 540;
  hg.y = 390;
  hg.r = 28;
  widgets_.push_back(hg);
  Widget hf{};
  hf.kind = Kind::Knob;
  hf.param = P_HIGH_FREQ;
  hf.x = 520;
  hf.y = 500;
  hf.r = 22;
  widgets_.push_back(hf);
  Widget hb{};
  hb.kind = Kind::Toggle;
  hb.param = P_HIGH_BELL;
  hb.x = 575;
  hb.y = 548;
  hb.w = 62;
  hb.h = 22;
  hb.label = "SHELF";
  widgets_.push_back(hb);

  Widget air{};
  air.kind = Kind::Knob;
  air.param = P_AIR;
  air.x = 760;
  air.y = 470;
  air.r = 36;
  widgets_.push_back(air);
}

void Panel::pageDrive() {
  for (int i = 0; i < 4; ++i) {
    Widget m{};
    m.kind = Kind::Choice;
    m.param = P_DRIVE_MODEL;
    m.value = i;
    m.x = 60 + i * 92;
    m.y = 160;
    m.w = 86;
    m.h = 28;
    m.label = kDriveModelLabels[i];
    widgets_.push_back(m);
  }
  Widget drive{};
  drive.kind = Kind::Knob;
  drive.param = P_DRIVE_AMOUNT;
  drive.x = 180;
  drive.y = 340;
  drive.r = 58;
  widgets_.push_back(drive);
  Widget ch{};
  ch.kind = Kind::Knob;
  ch.param = P_DRIVE_CHAR;
  ch.x = 350;
  ch.y = 340;
  ch.r = 44;
  widgets_.push_back(ch);
  Widget mixw{};
  mixw.kind = Kind::Knob;
  mixw.param = P_DRIVE_MIX;
  mixw.x = 500;
  mixw.y = 340;
  mixw.r = 40;
  widgets_.push_back(mixw);
}

void Panel::pageDeEss() {
  Widget sf{};
  sf.kind = Kind::Knob;
  sf.param = P_SIB_FREQ;
  sf.x = 160;
  sf.y = 300;
  sf.r = 46;
  sf.label = "Freq";
  widgets_.push_back(sf);
  Widget sa{};
  sa.kind = Kind::Knob;
  sa.param = P_SIB_AMOUNT;
  sa.x = 320;
  sa.y = 300;
  sa.r = 46;
  sa.label = "Amount";
  widgets_.push_back(sa);
  Widget hf{};
  hf.kind = Kind::Knob;
  hf.param = P_HARSH_FREQ;
  hf.x = 550;
  hf.y = 300;
  hf.r = 46;
  hf.label = "Freq";
  widgets_.push_back(hf);
  Widget ha{};
  ha.kind = Kind::Knob;
  ha.param = P_HARSH_AMOUNT;
  ha.x = 710;
  ha.y = 300;
  ha.r = 46;
  ha.label = "Amount";
  widgets_.push_back(ha);
  Widget learn{};
  learn.kind = Kind::Button;
  learn.param = kLearnSibButton;
  learn.x = 790;
  learn.y = 400;
  learn.w = 84;
  learn.h = 28;
  learn.label = "LEARN";
  widgets_.push_back(learn);
}

void Panel::pageEcho() {
  Widget sync{};
  sync.kind = Kind::Toggle;
  sync.param = P_ECHO_SYNC;
  sync.x = 60;
  sync.y = 158;
  sync.w = 90;
  sync.h = 26;
  sync.label = "SYNC";
  widgets_.push_back(sync);
  Widget lofi{};
  lofi.kind = Kind::Toggle;
  lofi.param = P_ECHO_LOFI;
  lofi.x = 160;
  lofi.y = 158;
  lofi.w = 80;
  lofi.h = 26;
  lofi.label = "LO-FI";
  widgets_.push_back(lofi);

  for (int i = 0; i < 10; ++i) {
    Widget d{};
    d.kind = Kind::Choice;
    d.param = P_ECHO_DIV;
    d.value = i;
    d.x = 60 + i * 72;
    d.y = 200;
    d.w = 66;
    d.h = 24;
    d.label = kEchoDivLabels[i];
    widgets_.push_back(d);
  }

  const uint32_t params[6] = {P_ECHO_TIME, P_ECHO_FEEDBACK, P_ECHO_TONE,
                              P_ECHO_WIDTH, P_ECHO_MOD, P_ECHO_DUCK};
  for (int i = 0; i < 6; ++i) {
    Widget k{};
    k.kind = Kind::Knob;
    k.param = params[i];
    k.x = 100 + i * 130;
    k.y = 350;
    k.r = 38;
    widgets_.push_back(k);
  }
  Widget mixw{};
  mixw.kind = Kind::Knob;
  mixw.param = P_ECHO_MIX;
  mixw.x = 810;
  mixw.y = 470;
  mixw.r = 30;
  widgets_.push_back(mixw);
  Widget tempo{};
  tempo.kind = Kind::Knob;
  tempo.param = P_TEMPO;
  tempo.x = 820;
  tempo.y = 258;
  tempo.r = 18;
  widgets_.push_back(tempo);
}

void Panel::pageSpace() {
  for (int i = 0; i < 3; ++i) {
    Widget m{};
    m.kind = Kind::Choice;
    m.param = P_SPACE_MODE;
    m.value = i;
    m.x = 60 + i * 96;
    m.y = 160;
    m.w = 90;
    m.h = 28;
    m.label = kSpaceModeLabels[i];
    widgets_.push_back(m);
  }
  const uint32_t params[6] = {P_SPACE_SIZE,     P_SPACE_DECAY,
                              P_SPACE_PREDELAY, P_SPACE_TONE,
                              P_SPACE_WIDTH,    P_SPACE_DUCK};
  for (int i = 0; i < 6; ++i) {
    Widget k{};
    k.kind = Kind::Knob;
    k.param = params[i];
    k.x = 110 + i * 125;
    k.y = 340;
    k.r = 38;
    widgets_.push_back(k);
  }
  Widget mixw{};
  mixw.kind = Kind::Knob;
  mixw.param = P_SPACE_MIX;
  mixw.x = 810;
  mixw.y = 470;
  mixw.r = 30;
  widgets_.push_back(mixw);
}

void Panel::pageOut() {
  Widget fader{};
  fader.kind = Kind::Fader;
  fader.param = P_OUTPUT;
  fader.x = 130;
  fader.y = 280;
  fader.w = 70;
  fader.h = 250;
  widgets_.push_back(fader);
  Widget limit{};
  limit.kind = Kind::Toggle;
  limit.param = P_LIMIT_ON;
  limit.x = 280;
  limit.y = 260;
  limit.w = 100;
  limit.h = 28;
  limit.label = "LIMIT";
  widgets_.push_back(limit);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void Panel::draw(cairo_t* cr, Shared& shared, const PanelState& state) {
  buildLayout(shared, state.tab);

  // Faceplate.
  cairo_pattern_t* grad = cairo_pattern_create_linear(0, 0, 0, kPanelHeight);
  cairo_pattern_add_color_stop_rgb(grad, 0.0, kBgTop.r, kBgTop.g, kBgTop.b);
  cairo_pattern_add_color_stop_rgb(grad, 1.0, kBgBottom.r, kBgBottom.g,
                                   kBgBottom.b);
  cairo_set_source(cr, grad);
  cairo_paint(cr);
  cairo_pattern_destroy(grad);

  setRgb(cr, kEdge);
  cairo_rectangle(cr, 8, 8, kPanelWidth - 16, kPanelHeight - 16);
  cairo_set_line_width(cr, 2);
  cairo_stroke(cr);

  drawHeader(cr, shared, state);
  drawChain(cr, shared, state);
  drawBody(cr, shared, state);
  drawRightRail(cr, shared, state);
  drawFooter(cr, shared, state);

  if (state.presetOpen) {
    drawPresetList(cr, shared, state);
  }

  // Corner screws.
  for (int i = 0; i < 4; ++i) {
    const double x = (i & 1) ? 1012.0 : 28.0;
    const double y = (i & 2) ? 612.0 : 28.0;
    cairo_arc(cr, x, y, 5.5, 0, 2 * kPiD);
    setRgb(cr, {0.28, 0.28, 0.28});
    cairo_fill(cr);
    cairo_arc(cr, x, y, 3.0, 0, 2 * kPiD);
    setRgb(cr, {0.09, 0.09, 0.09});
    cairo_fill(cr);
  }
}

void Panel::drawHeader(cairo_t* cr, const Shared& shared,
                       const PanelState& state) const {
  setRgb(cr, kPanel);
  roundRect(cr, 16, 10, kPanelWidth - 32, 44, 6);
  cairo_fill(cr);

  for (const Widget& w : widgets_) {
    if (w.kind != Kind::Power) {
      continue;
    }
    const bool on = shared.get(P_POWER) > 0.5;
    const double cx = w.x + w.w * 0.5;
    const double cy = w.y + w.h * 0.5;
    const double r = w.r;
    cairo_arc(cr, cx, cy, r, 0, 2 * kPiD);
    setRgb(cr, {0.12, 0.12, 0.12});
    cairo_fill(cr);
    cairo_arc(cr, cx, cy, r - 1.5, 0, 2 * kPiD);
    setRgb(cr, on ? kGreen : Rgb{0.42, 0.42, 0.42});
    cairo_set_line_width(cr, 2.2);
    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, 7.0, -kPiD / 2 + 0.6,
              -kPiD / 2 + 2 * kPiD - 0.6);
    cairo_stroke(cr);
    cairo_move_to(cr, cx, cy - 9.0);
    cairo_line_to(cr, cx, cy - 1.0);
    cairo_stroke(cr);
  }

  text(cr, "VOCAL SUITE", 520, 40, 22, 1, kText.r, kText.g, kText.b);
  text(cr, "SERIES", 520, 53, 8, 1, kDim.r, kDim.g, kDim.b);
  setRgb(cr, kAccent, 0.65);
  cairo_rectangle(cr, 448, 45, 144, 1.4);
  cairo_fill(cr);

  drawPresetBar(cr, shared, state);
}

void Panel::drawPresetBar(cairo_t* cr, const Shared& shared,
                          const PanelState& state) const {
  const double x = 72, y = 14, w = 330, h = 36;
  roundRect(cr, x, y, w, h, 5);
  setRgb(cr, {0.09, 0.095, 0.10});
  cairo_fill(cr);
  setRgb(cr, kEdge);
  cairo_set_line_width(cr, 1);
  roundRect(cr, x + 0.5, y + 0.5, w - 1, h - 1, 5);
  cairo_stroke(cr);

  text(cr, "\u2039", x + 16, y + 25, 22, 1, kDim.r, kDim.g, kDim.b);
  text(cr, "\u203a", x + w - 16, y + 25, 22, 1, kDim.r, kDim.g, kDim.b);

  const char* name = "Init";
  if (state.presetIndex >= 0 && state.presetIndex < presetCount()) {
    name = factoryPresets()[state.presetIndex].name;
  }
  text(cr, name, x + w * 0.5, y + 24, 13, 1, kText.r, kText.g, kText.b);
  if (state.presetDirty) {
    cairo_arc(cr, x + w - 34, y + 12, 3.5, 0, 2 * kPiD);
    setRgb(cr, kAccent);
    cairo_fill(cr);
  }

  // Compare button.
  for (const Widget& ww : widgets_) {
    if (ww.kind != Kind::Button || ww.param != P_COUNT) {
      continue;
    }
    const bool held = shared.compare.load(std::memory_order_relaxed);
    roundRect(cr, ww.x, ww.y, ww.w, ww.h, 4);
    setRgb(cr, held ? kAccent : Rgb{0.20, 0.21, 0.22});
    cairo_fill(cr);
    text(cr, "COMPARE", ww.x + ww.w * 0.5, ww.y + 18, 10, 1,
         held ? 0.10 : 0.80, held ? 0.08 : 0.80, held ? 0.05 : 0.80);
  }
}

void Panel::drawChain(cairo_t* cr, const Shared& shared,
                      const PanelState& state) const {
  setRgb(cr, {0.095, 0.100, 0.108});
  roundRect(cr, 16, 60, kPanelWidth - 32, 66, 6);
  cairo_fill(cr);
  setRgb(cr, {1, 1, 1}, 0.035);
  cairo_rectangle(cr, 24, 61, kPanelWidth - 48, 1);
  cairo_fill(cr);

  for (const Widget& w : widgets_) {
    if (w.kind != Kind::PageTab) {
      continue;
    }
    const bool active = static_cast<int>(w.value) == state.tab;
    drawPageTab(cr, w, active, shared.get(w.param) > 0.5);
    if (w.value < 7) {
      text(cr, "\u203a", w.x + w.w + 3, w.y + 33, 20, 0, kDim.r, kDim.g,
           kDim.b);
    }
  }
}

void Panel::drawPageTab(cairo_t* cr, const Widget& w, bool active,
                        bool moduleOn) const {
  roundRect(cr, w.x, w.y, w.w, w.h, 5);
  setRgb(cr, active ? mix(kPanelLight, kAccent, 0.20) : Rgb{0.145, 0.150, 0.158});
  cairo_fill(cr);
  setRgb(cr, active ? kAccent : kEdge, active ? 0.9 : 1.0);
  cairo_set_line_width(cr, active ? 1.5 : 1.0);
  roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1, w.h - 1, 5);
  cairo_stroke(cr);

  cairo_arc(cr, w.x + 12, w.y + 13, 3.5, 0, 2 * kPiD);
  setRgb(cr, moduleOn ? kGreen : Rgb{0.28, 0.29, 0.30});
  cairo_fill(cr);
  if (active) {
    setRgb(cr, kAccent, 0.9);
    cairo_rectangle(cr, w.x + 12, w.y + w.h - 3, w.w - 24, 2);
    cairo_fill(cr);
  }

  text(cr, w.label, w.x + w.w * 0.5, w.y + 34, 11, 1,
       active ? kText.r : 0.72, active ? kText.g : 0.71,
       active ? kText.b : 0.69);
}

void Panel::drawBody(cairo_t* cr, Shared& shared,
                     const PanelState& state) const {
  setRgb(cr, {0.105, 0.110, 0.118});
  roundRect(cr, 20, 136, 868, 456, 6);
  cairo_fill(cr);
  setRgb(cr, {1, 1, 1}, 0.035);
  cairo_rectangle(cr, 28, 137, 852, 1);
  cairo_fill(cr);
  setRgb(cr, kEdge);
  cairo_set_line_width(cr, 1);
  roundRect(cr, 20.5, 136.5, 867, 455, 6);
  cairo_stroke(cr);

  switch (state.tab) {
    case 0:
      text(cr, "GAIN STAGING", 60, 170, 11, 0, kDim.r, kDim.g, kDim.b);
      {
        const double mx = 790, my = 220, mw = 58, mh = 250;
        drawMeter(cr, mx, my, mw, mh, shared.inPeakLDb.load(),
                  shared.inPeakRDb.load(), "INPUT");
        const double yTop = my + 4 + (mh - 8) * (1.0 - (60.0 - 12.0) / 60.0);
        const double yBot = my + 4 + (mh - 8) * (1.0 - (60.0 - 18.0) / 60.0);
        setRgb(cr, kGreen, 0.16);
        cairo_rectangle(cr, mx + 2, yTop, mw - 4, yBot - yTop);
        cairo_fill(cr);
        const char* dbLabels[6] = {"0", "-12", "-24", "-36", "-48", "-60"};
        for (int i = 0; i < 6; ++i) {
          const double yy = my + 4 + (mh - 8) * i / 5.0;
          text(cr, dbLabels[i], mx + mw + 7, yy + 3, 7.5, 0, kDim.r, kDim.g,
               kDim.b);
        }
      }
      text(cr, "Set the loudest phrase inside the green zone, then cut rumble with the HPF.",
           60, 560, 10, 0, kDim.r, kDim.g, kDim.b);
      break;
    case 1:
      text(cr, "COMPRESSOR", 60, 150, 11, 0, kDim.r, kDim.g, kDim.b);
      drawCompCurve(cr, shared, 60, 430, 500, 130);
      text(cr, "Opto is smooth, FET is fast and aggressive, VCA is controlled and modern.",
           60, 575, 10, 0, kDim.r, kDim.g, kDim.b);
      break;
    case 2:
      text(cr, "TONE", 60, 150, 11, 0, kDim.r, kDim.g, kDim.b);
      drawEqCurve(cr, shared, 60, 155, 520, 180);
      text(cr, "Use broad strokes. Flat is a true bypass.",
           60, 588, 9, 0, kDim.r, kDim.g, kDim.b);
      break;
    case 3:
      text(cr, "DRIVE", 60, 150, 11, 0, kDim.r, kDim.g, kDim.b);
      drawDriveCurve(cr, shared, 60, 430, 500, 130);
      text(cr, "Valve adds weight, Console adds edge, Tape softens, Clip is for screams.",
           60, 575, 10, 0, kDim.r, kDim.g, kDim.b);
      break;
    case 4:
      text(cr, "DE-ESS", 60, 150, 11, 0, kDim.r, kDim.g, kDim.b);
      text(cr, "SIBILANCE", 240, 230, 9, 1, kDim.r, kDim.g, kDim.b);
      text(cr, "HARSHNESS", 630, 230, 9, 1, kDim.r, kDim.g, kDim.b);
      drawGrBar(cr, 790, 240, 22, 110, shared.sibGrDb.load(), 18.0, "SIB");
      drawGrBar(cr, 822, 240, 22, 110, shared.harshGrDb.load(), 18.0, "HRS");
      text(cr, "Start with amount 2-4. Raise until the S's stop poking out.",
           60, 575, 10, 0, kDim.r, kDim.g, kDim.b);
      break;
    case 5:
      text(cr, "ECHO", 60, 150, 11, 0, kDim.r, kDim.g, kDim.b);
      text(cr, "Tempo-synced by default. Duck keeps the repeats under the vocal.",
           60, 575, 10, 0, kDim.r, kDim.g, kDim.b);
      break;
    case 6:
      text(cr, "SPACE", 60, 150, 11, 0, kDim.r, kDim.g, kDim.b);
      {
        const double gx = 60, gy = 400, gw = 520, gh = 140;
        roundRect(cr, gx, gy, gw, gh, 4);
        setRgb(cr, {0.055, 0.060, 0.065});
        cairo_fill(cr);
        setRgb(cr, kDim, 0.22);
        cairo_set_line_width(cr, 1);
        for (int i = 1; i < 6; ++i) {
          cairo_move_to(cr, gx + gw * i / 6.0, gy);
          cairo_line_to(cr, gx + gw * i / 6.0, gy + gh);
          cairo_stroke(cr);
        }
        const double decay = shared.get(P_SPACE_DECAY);
        setRgb(cr, kAccent, 0.95);
        cairo_set_line_width(cr, 2);
        for (int i = 0; i <= 200; ++i) {
          const double t = i / 200.0;
          const double time = t * 4.0;
          const double amp = std::exp(-3.0 * time / std::max(0.3, decay));
          const double y = gy + 10 + (gh - 20) * (1.0 - amp);
          const double px = gx + 10 + (gw - 20) * t;
          if (i == 0) cairo_move_to(cr, px, y);
          else cairo_line_to(cr, px, y);
        }
        cairo_stroke(cr);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "RT60 %.1f s", decay);
        text(cr, buf, gx + gw - 12, gy + 18, 9, 2, kDim.r, kDim.g, kDim.b);
      }
      text(cr, "Plate is bright and dense, Room is tight, Hall is long.",
           60, 575, 10, 0, kDim.r, kDim.g, kDim.b);
      break;
    default:
      text(cr, "OUTPUT", 60, 150, 11, 0, kDim.r, kDim.g, kDim.b);
      drawMeter(cr, 420, 220, 90, 240, shared.inPeakLDb.load(),
                shared.inPeakRDb.load(), "INPUT");
      drawMeter(cr, 540, 220, 90, 240, shared.outPeakLDb.load(),
                shared.outPeakRDb.load(), "OUTPUT");
      drawGrBar(cr, 680, 260, 26, 140, shared.limGrDb.load(), 18.0, "LIMIT");
      text(cr, "The limiter is a safety catch, not a mastering limiter.",
           60, 575, 10, 0, kDim.r, kDim.g, kDim.b);
      break;
  }

  for (const Widget& w : widgets_) {
    if (w.kind == Kind::PageTab) {
      continue;
    }
    if (w.kind == Kind::Button && w.param == kCompareButton) {
      continue;
    }
    if (w.y >= 136.0 && w.y + w.h <= 596.0 && w.x + w.w < 892.0) {
      drawWidget(cr, w, shared);
    }
  }
}

void Panel::drawWidget(cairo_t* cr, const Widget& w,
                       const Shared& shared) const {
  const ParamSpec& spec = kParamSpecs[w.param];
  switch (w.kind) {
    case Kind::Knob:
      drawKnob(cr, w, shared.get(w.param), spec);
      break;
    case Kind::Toggle:
      drawToggle(cr, w, shared.get(w.param) > 0.5);
      break;
    case Kind::Choice:
      drawChoice(cr, w, static_cast<int>(shared.get(w.param) + 0.5));
      break;
    case Kind::Button: {
      const bool active =
          (w.param == kLearnLevelButton &&
           shared.inputLearnActive.load(std::memory_order_relaxed)) ||
          (w.param == kLearnSibButton &&
           shared.sibLearnActive.load(std::memory_order_relaxed));
      roundRect(cr, w.x, w.y, w.w, w.h, 4);
      setRgb(cr, active ? kAccent : Rgb{0.17, 0.18, 0.19});
      cairo_fill(cr);
      setRgb(cr, kEdge);
      cairo_set_line_width(cr, 1);
      roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1, w.h - 1, 4);
      cairo_stroke(cr);
      text(cr, active ? "LISTENING" : w.label, w.x + w.w * 0.5,
           w.y + w.h * 0.5 + 4, 9, 1,
           active ? 0.10 : 0.80, active ? 0.08 : 0.80,
           active ? 0.05 : 0.80);
      break;
    }
    case Kind::Fader: {
      const double f = clamp01((shared.get(w.param) - spec.min) /
                               (spec.max - spec.min));
      roundRect(cr, w.x + w.w * 0.5 - 5, w.y, 10, w.h, 5);
      setRgb(cr, {0.06, 0.06, 0.07});
      cairo_fill(cr);
      const double hy = w.y + 12 + (1.0 - f) * (w.h - 24);
      roundRect(cr, w.x + 6, hy - 9, w.w - 12, 18, 4);
      setRgb(cr, kAccent);
      cairo_fill(cr);
      text(cr, "OUTPUT", w.x + w.w * 0.5, w.y - 12, 10, 1, kText.r, kText.g,
           kText.b);
      text(cr, formatValue(spec, shared.get(w.param)).c_str(),
           w.x + w.w * 0.5, w.y + w.h + 18, 10, 1, kAccent.r, kAccent.g,
           kAccent.b);
      break;
    }
    default:
      break;
  }
}

void Panel::drawKnob(cairo_t* cr, const Widget& w, double value,
                     const ParamSpec& spec) const {
  const double cx = w.x;
  const double cy = w.y;
  const double r = w.r;
  const double f = clamp01((value - spec.min) / (spec.max - spec.min));

  // Label and value.
  const char* name = (w.label && w.label[0]) ? w.label : spec.name;
  text(cr, name, cx, cy - r - 16, 10, 1, kText.r, kText.g, kText.b);
  text(cr, formatValue(spec, value).c_str(), cx, cy + r + 16, 9, 1,
       kAccent.r, kAccent.g, kAccent.b);

  // Ticks sit outside the cap.  Small knobs keep only the arcs so their
  // labels stay clean.
  if (r >= 26.0) {
    setRgb(cr, kDim, 0.50);
    cairo_set_line_width(cr, 1);
    for (int i = 0; i <= 10; ++i) {
      const double a = knobAngle(i / 10.0);
      const double r0 = r + 7;
      const double r1 = r + (i % 5 == 0 ? 13 : 10);
      cairo_move_to(cr, cx + std::cos(a) * r0, cy + std::sin(a) * r0);
      cairo_line_to(cr, cx + std::cos(a) * r1, cy + std::sin(a) * r1);
      cairo_stroke(cr);
    }
  }

  // Skirt and cap, with a soft shadow.
  cairo_arc(cr, cx + 1.5, cy + 2.0, r + 1, 0, 2 * kPiD);
  setRgb(cr, {0.0, 0.0, 0.0}, 0.35);
  cairo_fill(cr);
  cairo_arc(cr, cx, cy, r, 0, 2 * kPiD);
  setRgb(cr, {0.075, 0.078, 0.082});
  cairo_fill(cr);
  cairo_arc(cr, cx, cy, r - 2, 0, 2 * kPiD);
  setRgb(cr, {0.22, 0.23, 0.24});
  cairo_fill(cr);
  cairo_arc(cr, cx, cy, r * 0.72, 0, 2 * kPiD);
  setRgb(cr, {0.31, 0.32, 0.33});
  cairo_fill(cr);
  cairo_arc(cr, cx, cy, r * 0.64, 0, 2 * kPiD);
  setRgb(cr, {0.18, 0.19, 0.20});
  cairo_fill(cr);

  // Outer recommended and value arcs.
  if (spec.recMax > spec.recMin && !spec.stepped) {
    const double rf0 = clamp01((spec.recMin - spec.min) / (spec.max - spec.min));
    const double rf1 = clamp01((spec.recMax - spec.min) / (spec.max - spec.min));
    cairo_set_line_width(cr, 4);
    setRgb(cr, kAccent, 0.30);
    cairo_arc(cr, cx, cy, r + 5, knobAngle(rf0), knobAngle(rf1));
    cairo_stroke(cr);
  }
  cairo_set_line_width(cr, 2.2);
  setRgb(cr, kAccent, 0.95);
  cairo_arc(cr, cx, cy, r + 2, knobAngle(0.0), knobAngle(f));
  cairo_stroke(cr);

  // Indicator.
  const double a = knobAngle(f);
  cairo_set_line_width(cr, 2.5);
  setRgb(cr, kText);
  cairo_move_to(cr, cx + std::cos(a) * r * 0.18,
                cy + std::sin(a) * r * 0.18);
  cairo_line_to(cr, cx + std::cos(a) * r * 0.62,
                cy + std::sin(a) * r * 0.62);
  cairo_stroke(cr);
}

void Panel::drawToggle(cairo_t* cr, const Widget& w, bool on) const {
  roundRect(cr, w.x, w.y, w.w, w.h, 4);
  setRgb(cr, on ? kAccent : Rgb{0.17, 0.18, 0.19});
  cairo_fill(cr);
  setRgb(cr, kEdge);
  cairo_set_line_width(cr, 1);
  roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1, w.h - 1, 4);
  cairo_stroke(cr);
  const char* label = w.label;
  if (w.param == P_LOW_BELL || w.param == P_HIGH_BELL) {
    label = on ? "BELL" : "SHELF";
  } else if (w.param == P_HPF_ON) {
    label = on ? "HPF ON" : "HPF OFF";
  } else if (w.param == P_POLARITY) {
    label = on ? "POLARITY \u00d8" : "POLARITY";
  } else if (w.param == P_ECHO_SYNC) {
    label = on ? "SYNC" : "FREE";
  } else if (w.label && std::strcmp(w.label, "ON") == 0) {
    label = on ? "ON" : "OFF";
  }
  text(cr, label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 4, 9, 1,
       on ? 0.10 : 0.78, on ? 0.08 : 0.78, on ? 0.05 : 0.78);
}

void Panel::drawChoice(cairo_t* cr, const Widget& w, int selected) const {
  const bool on = selected == static_cast<int>(w.value);
  roundRect(cr, w.x, w.y, w.w, w.h, 4);
  setRgb(cr, on ? mix(kPanelLight, kAccent, 0.35) : Rgb{0.16, 0.17, 0.18});
  cairo_fill(cr);
  setRgb(cr, on ? kAccent : kEdge, on ? 0.9 : 1.0);
  cairo_set_line_width(cr, 1);
  roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1, w.h - 1, 4);
  cairo_stroke(cr);
  text(cr, w.label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 4, 9, 1,
       on ? kText.r : 0.72, on ? kText.g : 0.71, on ? kText.b : 0.69);
}

void Panel::drawMeter(cairo_t* cr, double x, double y, double w, double h,
                      double dbL, double dbR, const char* label) const {
  roundRect(cr, x, y, w, h, 3);
  setRgb(cr, kMeterFace);
  cairo_fill(cr);
  const double bw = (w - 8) * 0.5;
  const double minDb = -60.0;
  for (int ch = 0; ch < 2; ++ch) {
    const double db = ch == 0 ? dbL : dbR;
    const double f = clamp01((db - minDb) / 60.0);
    const double bx = x + 4 + ch * (bw + 1);
    const double fill = (h - 8) * f;
    setRgb(cr, f > 0.92 ? kRed : (f > 0.78 ? kAmber : kGreen), 0.9);
    cairo_rectangle(cr, bx, y + h - 4 - fill, bw, fill);
    cairo_fill(cr);
  }
  setRgb(cr, kDim, 0.7);
  cairo_set_line_width(cr, 1);
  for (int i = 0; i <= 6; ++i) {
    const double yy = y + 4 + (h - 8) * i / 6.0;
    cairo_move_to(cr, x + w - 3, yy);
    cairo_line_to(cr, x + w, yy);
    cairo_stroke(cr);
  }
  text(cr, label, x + w * 0.5, y + h + 13, 8, 1, kDim.r, kDim.g, kDim.b);
}

void Panel::drawGrBar(cairo_t* cr, double x, double y, double w, double h,
                      double grDb, double maxDb, const char* label) const {
  roundRect(cr, x, y, w, h, 3);
  setRgb(cr, kMeterFace);
  cairo_fill(cr);
  const double f = clamp01(grDb / maxDb);
  if (f > 0.0) {
    roundRect(cr, x + 3, y + 4 + (h - 8) * (1.0 - f), w - 6,
              (h - 8) * f, 2);
    setRgb(cr, kAccent, 0.9);
    cairo_fill(cr);
  }
  setRgb(cr, kDim, 0.5);
  cairo_set_line_width(cr, 1);
  for (int i = 0; i <= 3; ++i) {
    const double yy = y + 4 + (h - 8) * i / 3.0;
    cairo_move_to(cr, x + w - 3, yy);
    cairo_line_to(cr, x + w, yy);
    cairo_stroke(cr);
  }
  text(cr, label, x + w * 0.5, y + h + 13, 8, 1, kDim.r, kDim.g, kDim.b);
}

void Panel::drawRightRail(cairo_t* cr, const Shared& shared,
                          const PanelState& state) const {
  (void)state;
  setRgb(cr, {0.095, 0.100, 0.108});
  roundRect(cr, 896, 136, 124, 456, 6);
  cairo_fill(cr);
  setRgb(cr, kEdge);
  cairo_set_line_width(cr, 1);
  roundRect(cr, 896.5, 136.5, 123, 455, 6);
  cairo_stroke(cr);

  text(cr, "LEVEL", 906, 148, 7.5, 0, kDim.r, kDim.g, kDim.b);
  drawMeter(cr, 908, 156, 38, 110, shared.inPeakLDb.load(),
            shared.inPeakRDb.load(), "IN");
  {
    const double yTop = 156 + 4 + (110 - 8) * (1.0 - (60.0 - 12.0) / 60.0);
    const double yBot = 156 + 4 + (110 - 8) * (1.0 - (60.0 - 18.0) / 60.0);
    setRgb(cr, kGreen, 0.18);
    cairo_rectangle(cr, 908, yTop, 38, yBot - yTop);
    cairo_fill(cr);
  }
  drawMeter(cr, 956, 156, 38, 110, shared.outPeakLDb.load(),
            shared.outPeakRDb.load(), "OUT");

  setRgb(cr, kDim, 0.18);
  cairo_rectangle(cr, 906, 282, 108, 1);
  cairo_fill(cr);
  text(cr, "GAIN REDUCTION", 906, 294, 7, 0, kDim.r, kDim.g, kDim.b);
  const struct {
    const char* label;
    float db;
    double x;
  } bars[4] = {{"CMP", shared.compGrDb.load(), 908},
               {"SIB", shared.sibGrDb.load(), 930},
               {"HRS", shared.harshGrDb.load(), 952},
               {"LIM", shared.limGrDb.load(), 974}};
  for (const auto& b : bars) {
    const double f = clamp01(b.db / 18.0);
    roundRect(cr, b.x, 300, 16, 70, 2);
    setRgb(cr, {0.06, 0.06, 0.07});
    cairo_fill(cr);
    if (f > 0.0) {
      roundRect(cr, b.x, 300 + 70 * (1.0 - f), 16, 70 * f, 2);
      setRgb(cr, kAccent, 0.9);
      cairo_fill(cr);
    }
    text(cr, b.label, b.x + 8, 386, 7.5, 1, kDim.r, kDim.g, kDim.b);
  }

  text(cr, formatValue(kParamSpecs[P_OUTPUT], shared.get(P_OUTPUT)).c_str(),
       958, 430, 12, 1, kAccent.r, kAccent.g, kAccent.b);
  text(cr, "OUTPUT", 958, 446, 8, 1, kDim.r, kDim.g, kDim.b);

  // Compact output fader in the rail.
  const double f = clamp01((shared.get(P_OUTPUT) - kParamSpecs[P_OUTPUT].min) /
                           (kParamSpecs[P_OUTPUT].max -
                            kParamSpecs[P_OUTPUT].min));
  roundRect(cr, 954, 466, 8, 110, 4);
  setRgb(cr, {0.06, 0.06, 0.07});
  cairo_fill(cr);
  const double hy = 466 + 8 + (1.0 - f) * 94;
  roundRect(cr, 946, hy - 7, 24, 14, 3);
  setRgb(cr, kAccent);
  cairo_fill(cr);
}

void Panel::drawFooter(cairo_t* cr, const Shared& shared,
                       const PanelState& state) const {
  (void)shared;
  const char* blurb = "";
  if (state.presetIndex >= 0 && state.presetIndex < presetCount()) {
    blurb = factoryPresets()[state.presetIndex].blurb;
  } else {
    blurb = "Pick a preset, set the input level, then compare.";
  }
  text(cr, blurb, 28, 622, 9, 0, kDim.r, kDim.g, kDim.b);
  text(cr, "ALL STAGES IN SERIES", 1012, 622, 8, 2, kAccent.r, kAccent.g,
       kAccent.b);
}

void Panel::drawEqCurve(cairo_t* cr, const Shared& shared, double x, double y,
                        double w, double h) const {
  roundRect(cr, x, y, w, h, 4);
  setRgb(cr, {0.055, 0.060, 0.065});
  cairo_fill(cr);
  setRgb(cr, kDim, 0.20);
  cairo_set_line_width(cr, 1);
  for (int i = 1; i < 6; ++i) {
    const double gx = x + w * i / 6.0;
    cairo_move_to(cr, gx, y);
    cairo_line_to(cr, gx, y + h);
    cairo_stroke(cr);
  }
  for (int i = 1; i < 4; ++i) {
    const double gy = y + h * i / 4.0;
    cairo_move_to(cr, x, gy);
    cairo_line_to(cr, x + w, gy);
    cairo_stroke(cr);
  }
  setRgb(cr, kDim, 0.55);
  cairo_set_line_width(cr, 1);
  cairo_move_to(cr, x, y + h * 0.5);
  cairo_line_to(cr, x + w, y + h * 0.5);
  cairo_stroke(cr);

  const double lowGain = shared.get(P_LOW_GAIN);
  const double lowFreq = shared.get(P_LOW_FREQ);
  const double lowBell = shared.get(P_LOW_BELL);
  const double midGain = shared.get(P_MID_GAIN);
  const double midFreq = shared.get(P_MID_FREQ);
  const double midQ = shared.get(P_MID_Q);
  const double highGain = shared.get(P_HIGH_GAIN);
  const double highFreq = shared.get(P_HIGH_FREQ);
  const double highBell = shared.get(P_HIGH_BELL);
  const double air = shared.get(P_AIR);

  setRgb(cr, kAccent, 0.95);
  cairo_set_line_width(cr, 2);
  for (int i = 0; i <= 240; ++i) {
    const double t = i / 240.0;
    const double f = 20.0 * std::pow(1000.0, t);
    double db = 0.0;
    if (lowBell > 0.5) {
      const double d = std::log2(f / lowFreq);
      db += lowGain * std::exp(-0.5 * d * d / (0.7 * 0.7));
    } else {
      db += lowGain / (1.0 + (f / lowFreq) * (f / lowFreq));
    }
    {
      const double q = std::max(0.3, midQ);
      const double d = std::log2(f / midFreq) * q;
      db += midGain * std::exp(-0.5 * d * d);
    }
    if (highBell > 0.5) {
      const double d = std::log2(f / highFreq);
      db += highGain * std::exp(-0.5 * d * d / (0.7 * 0.7));
    } else {
      const double ratio = f / highFreq;
      db += highGain * ratio * ratio / (1.0 + ratio * ratio);
    }
    {
      const double ratio = f / 12000.0;
      db += air * ratio * ratio / (1.0 + ratio * ratio);
    }
    db = std::clamp(db, -18.0, 18.0);
    const double px = x + w * t;
    const double py = y + h * 0.5 - db / 18.0 * h * 0.5;
    if (i == 0) {
      cairo_move_to(cr, px, py);
    } else {
      cairo_line_to(cr, px, py);
    }
  }
  cairo_stroke(cr);
}

void Panel::drawCompCurve(cairo_t* cr, const Shared& shared, double x, double y,
                          double w, double h) const {
  roundRect(cr, x, y, w, h, 4);
  setRgb(cr, {0.055, 0.060, 0.065});
  cairo_fill(cr);
  setRgb(cr, kDim, 0.25);
  cairo_set_line_width(cr, 1);
  cairo_move_to(cr, x, y + h);
  cairo_line_to(cr, x + w, y);
  cairo_stroke(cr);

  const double amount = shared.get(P_COMP_AMOUNT) / 10.0;
  const double ratio = shared.get(P_COMP_RATIO) == 0
                           ? 4.0
                           : (shared.get(P_COMP_RATIO) == 1 ? 8.0 : 20.0);
  setRgb(cr, kAccent, 0.95);
  cairo_set_line_width(cr, 2);
  for (int i = 0; i <= 120; ++i) {
    const double in = i / 120.0;
    const double thr = 0.85 - 0.55 * amount;
    double out = in;
    if (in > thr) {
      out = thr + (in - thr) / ratio;
    }
    const double px = x + w * in;
    const double py = y + h * (1.0 - out);
    if (i == 0) {
      cairo_move_to(cr, px, py);
    } else {
      cairo_line_to(cr, px, py);
    }
  }
  cairo_stroke(cr);
  text(cr, "IN", x + 8, y + h - 8, 8, 0, kDim.r, kDim.g, kDim.b);
  text(cr, "OUT", x + w - 8, y + 12, 8, 2, kDim.r, kDim.g, kDim.b);
}

void Panel::drawDriveCurve(cairo_t* cr, const Shared& shared, double x,
                           double y, double w, double h) const {
  roundRect(cr, x, y, w, h, 4);
  setRgb(cr, {0.055, 0.060, 0.065});
  cairo_fill(cr);
  setRgb(cr, kDim, 0.25);
  cairo_set_line_width(cr, 1);
  cairo_move_to(cr, x, y + h * 0.5);
  cairo_line_to(cr, x + w, y + h * 0.5);
  cairo_stroke(cr);

  const double amt = shared.get(P_DRIVE_AMOUNT) / 10.0;
  const double ch = shared.get(P_DRIVE_CHAR) / 10.0;
  const int model = static_cast<int>(shared.get(P_DRIVE_MODEL) + 0.5);
  setRgb(cr, kAccent, 0.95);
  cairo_set_line_width(cr, 2);
  for (int i = 0; i <= 200; ++i) {
    const double t = i / 200.0;
    const double in = t * 2.0 - 1.0;
    const double out = driveShape(model, amt, ch, in);
    const double px = x + w * t;
    const double py = y + h * 0.5 - std::clamp(out, -1.2, 1.2) * h * 0.4;
    if (i == 0) {
      cairo_move_to(cr, px, py);
    } else {
      cairo_line_to(cr, px, py);
    }
  }
  cairo_stroke(cr);
}

// ---------------------------------------------------------------------------
// Interaction
// ---------------------------------------------------------------------------

bool Panel::hit(const Widget& w, double x, double y) const {
  if (w.kind == Kind::Knob) {
    const double dx = x - w.x;
    const double dy = y - w.y;
    return dx * dx + dy * dy <= (w.r + 8) * (w.r + 8);
  }
  return x >= w.x && x <= w.x + w.w && y >= w.y && y <= w.y + w.h;
}

const Panel::Widget* Panel::find(uint32_t param, Kind kind) const {
  for (const Widget& w : widgets_) {
    if (w.param == param && w.kind == kind) {
      return &w;
    }
  }
  return nullptr;
}

bool Panel::pointerDown(Shared& shared, PanelState& state, double x, double y,
                        bool fine) {
  if (state.presetOpen) {
    if (handlePresetClick(shared, state, x, y)) {
      return true;
    }
    state.presetOpen = 0;
    return true;
  }
  if (y < 58 && x >= 72 && x <= 402) {
    if (x < 100) {
      const int n = presetCount();
      state.presetIndex = (state.presetIndex - 1 + n) % n;
      applyPreset(shared, state, state.presetIndex);
    } else if (x > 374) {
      const int n = presetCount();
      state.presetIndex = (state.presetIndex + 1) % n;
      applyPreset(shared, state, state.presetIndex);
    } else {
      state.presetOpen = 1;
    }
    return true;
  }

  buildLayout(shared, state.tab);
  for (const Widget& w : widgets_) {
    if (!hit(w, x, y)) {
      continue;
    }
    if (w.kind == Kind::PageTab) {
      state.tab = static_cast<int>(w.value);
      return true;
    }
    if (w.kind == Kind::Button) {
      if (w.param == kCompareButton) {
        shared.compare.store(true, std::memory_order_relaxed);
        dragParam_ = static_cast<int>(kCompareButton);
        dragKind_ = Kind::Button;
      } else if (w.param == kLearnLevelButton) {
        shared.inputLearnPeakDb.store(-120.0f, std::memory_order_relaxed);
        shared.inputLearnActive.store(true, std::memory_order_relaxed);
        state.inputLearning = true;
        state.inputLearnTicks = 60;
      } else if (w.param == kLearnSibButton) {
        shared.sibLearnValid.store(false, std::memory_order_relaxed);
        shared.sibLearnActive.store(true, std::memory_order_relaxed);
        state.sibLearning = true;
        state.sibLearnTicks = 60;
      }
      return true;
    }
    if (w.kind == Kind::Toggle || w.kind == Kind::Power) {
      shared.set(w.param, shared.get(w.param) > 0.5 ? 0.0 : 1.0);
      state.presetDirty = true;
      return true;
    }
    if (w.kind == Kind::Choice) {
      shared.set(w.param, w.value);
      state.presetDirty = true;
      return true;
    }
    if (w.kind == Kind::Knob || w.kind == Kind::Fader) {
      dragParam_ = static_cast<int>(w.param);
      dragKind_ = w.kind;
      dragStartY_ = y;
      dragStartX_ = x;
      dragStartValue_ = shared.get(w.param);
      (void)fine;
      return true;
    }
  }
  return false;
}

bool Panel::pointerMove(Shared& shared, PanelState& state, double x, double y,
                        bool fine) {
  if (dragParam_ < 0) {
    return false;
  }
  if (dragKind_ == Kind::Button) {
    return true;
  }
  const ParamSpec& spec = kParamSpecs[dragParam_];
  const double range = spec.max - spec.min;
  double dy = dragStartY_ - y;
  if (fine) {
    dy *= 0.2;
  }
  double value;
  if (dragKind_ == Kind::Fader) {
    value = dragStartValue_ + (dy / 240.0) * range;
  } else {
    value = dragStartValue_ + (dy / 220.0) * range;
  }
  shared.set(static_cast<uint32_t>(dragParam_),
             std::min(std::max(value, spec.min), spec.max));
  state.presetDirty = true;
  (void)x;
  return true;
}

void Panel::pointerUp(Shared& shared) {
  if (dragParam_ == P_COUNT) {
    shared.compare.store(false, std::memory_order_relaxed);
  }
  dragParam_ = -1;
  dragKind_ = Kind::Knob;
}

bool Panel::wheel(Shared& shared, PanelState& state, double x, double y,
                  double dy) {
  if (state.presetOpen) {
    state.presetScroll =
        std::max(0, state.presetScroll + (dy < 0 ? 1 : -1));
    return true;
  }
  buildLayout(shared, state.tab);
  for (const Widget& w : widgets_) {
    if (!hit(w, x, y) || w.kind == Kind::PageTab || w.kind == Kind::Button) {
      continue;
    }
    const ParamSpec& spec = kParamSpecs[w.param];
    if (w.kind == Kind::Toggle || w.kind == Kind::Power) {
      shared.set(w.param, shared.get(w.param) > 0.5 ? 0.0 : 1.0);
    } else if (w.kind == Kind::Choice) {
      const int n = std::max(1, static_cast<int>(spec.max - spec.min + 1));
      int v = static_cast<int>(shared.get(w.param) + 0.5);
      v = (v + (dy < 0 ? 1 : n - 1)) % n;
      shared.set(w.param, spec.min + v);
    } else {
      const double step = spec.stepped ? 1.0 : (spec.max - spec.min) / 100.0;
      shared.set(w.param, shared.get(w.param) + (dy < 0 ? step : -step));
    }
    state.presetDirty = true;
    return true;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------

void Panel::applyPreset(Shared& shared, PanelState& state, int index) const {
  if (index < 0 || index >= presetCount()) {
    return;
  }
  const Preset& p = factoryPresets()[index];
  for (uint32_t i = 0; i < P_COUNT; ++i) {
    shared.set(i, p.values[i]);
  }
  state.presetIndex = index;
  state.presetDirty = false;
}

bool Panel::handlePresetClick(Shared& shared, PanelState& state, double x,
                              double y) {
  const double lx = 56, ly = 58, lw = 580, lh = 520;
  if (x < lx || x > lx + lw || y < ly || y > ly + lh) {
    return false;
  }

  // Filter chips.
  if (y >= 68 && y <= 86 && x >= 140 && x < 140 + 6 * 66) {
    state.filterStyle = static_cast<int>((x - 140) / 66) - 1;
    state.presetScroll = 0;
    return true;
  }
  if (y >= 92 && y <= 110 && x >= 140 && x < 140 + 5 * 60) {
    state.filterVoice = static_cast<int>((x - 140) / 60) - 1;
    state.presetScroll = 0;
    return true;
  }
  if (y >= 116 && y <= 134 && x >= 140 && x < 140 + 4 * 64) {
    state.filterIntensity = static_cast<int>((x - 140) / 64) - 1;
    state.presetScroll = 0;
    return true;
  }

  const double rowH = 22.0;
  const double bottom = 520.0;
  double cy = 146.0;
  const std::vector<int> order = visiblePresets(state);
  int last = -1;
  for (int k = state.presetScroll; k < static_cast<int>(order.size()); ++k) {
    const Preset& p = factoryPresets()[order[k]];
    if (last < 0 || std::strcmp(p.category,
                                factoryPresets()[order[last]].category) != 0) {
      if (cy + 18 + rowH > bottom) {
        break;
      }
      cy += 18;
      last = k;
    }
    if (cy + rowH > bottom) {
      break;
    }
    if (y >= cy && y <= cy + rowH) {
      applyPreset(shared, state, order[k]);
      state.presetOpen = 0;
      return true;
    }
    cy += rowH;
  }
  return true;
}

void Panel::drawPresetList(cairo_t* cr, const Shared& shared,
                           const PanelState& state) const {
  const double x = 56, y = 58, w = 580, h = 520;
  roundRect(cr, x, y, w, h, 6);
  setRgb(cr, {0.06, 0.065, 0.07}, 1.0);
  cairo_fill(cr);
  setRgb(cr, kAccent, 0.8);
  cairo_set_line_width(cr, 1.5);
  roundRect(cr, x + 0.5, y + 0.5, w - 1, h - 1, 6);
  cairo_stroke(cr);

  auto chip = [&](double cx, double cy, double cw, double ch,
                  const char* label, bool on) {
    roundRect(cr, cx, cy, cw, ch, 4);
    setRgb(cr, on ? kAccent : Rgb{0.16, 0.17, 0.18});
    cairo_fill(cr);
    setRgb(cr, on ? kAccent : kEdge);
    cairo_set_line_width(cr, 1);
    roundRect(cr, cx + 0.5, cy + 0.5, cw - 1, ch - 1, 4);
    cairo_stroke(cr);
    text(cr, label, cx + cw * 0.5, cy + ch * 0.5 + 3.5, 8, 1,
         on ? 0.10 : 0.78, on ? 0.08 : 0.78, on ? 0.05 : 0.78);
  };

  text(cr, "STYLE", x + 14, 81, 8, 0, kDim.r, kDim.g, kDim.b);
  const char* styles[6] = {"ALL", "ROCK", "POP", "SCREAM", "CLEAN", "UTIL"};
  for (int i = 0; i < 6; ++i) {
    chip(140 + i * 66, 68, 62, 18, styles[i], state.filterStyle == i - 1);
  }
  text(cr, "VOICE", x + 14, 105, 8, 0, kDim.r, kDim.g, kDim.b);
  const char* voices[5] = {"ALL", "ANY", "LOW", "MID", "HIGH"};
  for (int i = 0; i < 5; ++i) {
    chip(140 + i * 60, 92, 56, 18, voices[i], state.filterVoice == i - 1);
  }
  text(cr, "INT", x + 14, 129, 8, 0, kDim.r, kDim.g, kDim.b);
  const char* intens[4] = {"ALL", "NAT", "FWD", "AGG"};
  for (int i = 0; i < 4; ++i) {
    chip(140 + i * 64, 116, 60, 18, intens[i], state.filterIntensity == i - 1);
  }

  const double rowH = 22.0;
  const double bottom = 520.0;
  double cy = 146.0;
  const std::vector<int> order = visiblePresets(state);
  int last = -1;
  for (int k = state.presetScroll; k < static_cast<int>(order.size()); ++k) {
    const Preset& p = factoryPresets()[order[k]];
    if (last < 0 || std::strcmp(p.category,
                                factoryPresets()[order[last]].category) != 0) {
      if (cy + 18 + rowH > bottom) {
        break;
      }
      text(cr, p.category, x + 14, cy + 13, 8, 0, kAccent.r, kAccent.g,
           kAccent.b);
      cy += 18;
      last = k;
    }
    if (cy + rowH > bottom) {
      break;
    }
    const bool sel = order[k] == state.presetIndex;
    if (sel) {
      roundRect(cr, x + 6, cy, w - 12, rowH - 2, 3);
      setRgb(cr, kAccent, 0.22);
      cairo_fill(cr);
    }
    text(cr, p.name, x + 16, cy + 15, 10, 0, sel ? kText.r : 0.80,
         sel ? kText.g : 0.79, sel ? kText.b : 0.77);
    const char* tag = p.voice == VoiceTag::Low ? "LOW"
                      : p.voice == VoiceTag::High ? "HIGH"
                      : p.voice == VoiceTag::Mid ? "MID"
                                                 : "ANY";
    text(cr, tag, x + w - 14, cy + 15, 7.5, 2, kDim.r, kDim.g, kDim.b);
    cy += rowH;
  }
  if (order.empty()) {
    text(cr, "No presets match these filters.", x + 16, 180, 10, 0,
         kDim.r, kDim.g, kDim.b);
  }
  (void)shared;
}

void Panel::tick(Shared& shared, PanelState& state) const {
  if (state.inputLearning && --state.inputLearnTicks <= 0) {
    const float peak = shared.inputLearnPeakDb.load(std::memory_order_relaxed);
    if (peak > -60.0f) {
      const double target = -12.0;
      const double next = shared.get(P_INPUT) + (target - peak);
      shared.set(P_INPUT, std::min(18.0, std::max(-18.0, next)));
    }
    shared.inputLearnActive.store(false, std::memory_order_relaxed);
    state.inputLearning = false;
  }
  if (state.sibLearning && --state.sibLearnTicks <= 0) {
    if (shared.sibLearnValid.load(std::memory_order_relaxed)) {
      shared.set(P_SIB_FREQ,
                 shared.sibLearnHz.load(std::memory_order_relaxed));
    }
    shared.sibLearnActive.store(false, std::memory_order_relaxed);
    state.sibLearning = false;
  }
  state.inLevelL = shared.inPeakLDb.load(std::memory_order_relaxed);
  state.inLevelR = shared.inPeakRDb.load(std::memory_order_relaxed);
  state.outLevelL = shared.outPeakLDb.load(std::memory_order_relaxed);
  state.outLevelR = shared.outPeakRDb.load(std::memory_order_relaxed);
  state.compGr = shared.compGrDb.load(std::memory_order_relaxed);
  state.sibGr = shared.sibGrDb.load(std::memory_order_relaxed);
  state.harshGr = shared.harshGrDb.load(std::memory_order_relaxed);
  state.limGr = shared.limGrDb.load(std::memory_order_relaxed);
  const float odb = std::max(state.outLevelL, state.outLevelR);
  state.outPeakHold = std::max(odb, state.outPeakHold - 0.15f);
}

} // namespace vocal
