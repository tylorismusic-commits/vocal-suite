// Vocal Suite — inline_display.hpp
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace vocal {

// Ardour / Harrison inline-display extension.
//
// The host calls render() from its GUI thread and draws the returned ARGB32
// image in the mixer strip.  The audio thread only publishes the current gain
// reduction through an atomic float, so no allocation or drawing happens on
// the audio thread.

struct InlineDisplaySurface {
  unsigned char* data = nullptr;
  int width = 0;
  int height = 0;
  int stride = 0;
};

struct InlineDisplayQueueDraw {
  void* handle = nullptr;
  void (*queue_draw)(void* handle) = nullptr;
};

struct InlineDisplayInterface {
  InlineDisplaySurface* (*render)(void* instance, uint32_t width,
                                  uint32_t height) = nullptr;
};

inline constexpr const char* kInlineDisplayInterfaceURI =
    "http://harrisonconsoles.com/lv2/inlinedisplay#interface";
inline constexpr const char* kInlineDisplayQueueDrawURI =
    "http://harrisonconsoles.com/lv2/inlinedisplay#queue_draw";
inline constexpr const char* kInlineDisplayInGuiURI =
    "http://harrisonconsoles.com/lv2/inlinedisplay#in_gui";

inline constexpr const char* kInlineDisplayInterfaceSymbol =
    "http://harrisonconsoles.com/lv2/inlinedisplay#interface";

class InlineDisplay {
public:
  void setGainReductionDb(float db) noexcept {
    grDb_.store(std::min(std::max(db, 0.0f), 40.0f),
                std::memory_order_relaxed);
  }

  InlineDisplaySurface* render(uint32_t width, uint32_t height) {
    if (width < 24 || height < 14) {
      return nullptr;
    }
    width = std::min(width, 512u);
    height = std::min(height, 128u);

    const size_t count = static_cast<size_t>(width) * height;
    if (pixels_.size() != count) {
      pixels_.assign(count, kBg);
    }

    surface_.data = reinterpret_cast<unsigned char*>(pixels_.data());
    surface_.width = static_cast<int>(width);
    surface_.height = static_cast<int>(height);
    surface_.stride = static_cast<int>(width * 4);

    const float gr = grDb_.load(std::memory_order_relaxed);
    draw(static_cast<int>(width), static_cast<int>(height), gr);
    return &surface_;
  }

private:
  // Neutral graphite with the family's blue accent.
  static constexpr uint32_t kBg = 0xff0e1013u;
  static constexpr uint32_t kPanel = 0xff1a1d21u;
  static constexpr uint32_t kBorder = 0xff4a4f56u;
  static constexpr uint32_t kText = 0xffe8e6e1u;
  static constexpr uint32_t kMuted = 0xff8a9099u;
  static constexpr uint32_t kTrack = 0xff06080au;
  static constexpr uint32_t kGreen = 0xff4f9e69u;
  static constexpr uint32_t kAmber = 0xffd3a04cu;
  static constexpr uint32_t kRed = 0xffc0392bu;

  static const uint8_t* glyph(char c) {
    static const uint8_t gG[7] = {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0e};
    static const uint8_t gR[7] = {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11};
    static const uint8_t g0[7] = {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e};
    static const uint8_t g1[7] = {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e};
    static const uint8_t g2[7] = {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f};
    static const uint8_t g3[7] = {0x1f, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0e};
    static const uint8_t g4[7] = {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02};
    static const uint8_t g5[7] = {0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e};
    static const uint8_t g6[7] = {0x06, 0x08, 0x10, 0x1e, 0x11, 0x11, 0x0e};
    static const uint8_t g7[7] = {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    static const uint8_t g8[7] = {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e};
    static const uint8_t g9[7] = {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x02, 0x0c};
    static const uint8_t gDot[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06};
    static const uint8_t gMinus[7] = {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00};
    static const uint8_t gSpace[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    switch (c) {
      case 'G': return gG;
      case 'R': return gR;
      case '0': return g0;
      case '1': return g1;
      case '2': return g2;
      case '3': return g3;
      case '4': return g4;
      case '5': return g5;
      case '6': return g6;
      case '7': return g7;
      case '8': return g8;
      case '9': return g9;
      case '.': return gDot;
      case '-': return gMinus;
      default: return gSpace;
    }
  }

  void put(int x, int y, uint32_t c, int w, int h) {
    if (x >= 0 && y >= 0 && x < w && y < h) {
      pixels_[static_cast<size_t>(y) * w + x] = c;
    }
  }

  void fill(int x, int y, int w, int h, uint32_t c, int sw, int sh) {
    for (int yy = 0; yy < h; ++yy) {
      for (int xx = 0; xx < w; ++xx) {
        put(x + xx, y + yy, c, sw, sh);
      }
    }
  }

  int drawText(int x, int y, const char* text, uint32_t c, int scale, int sw,
               int sh) {
    int cursor = x;
    for (const char* p = text; *p; ++p) {
      const uint8_t* rows = glyph(*p);
      for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
          if (rows[row] & (1u << (4 - col))) {
            fill(cursor + col * scale, y + row * scale, scale, scale, c, sw, sh);
          }
        }
      }
      cursor += 6 * scale;
    }
    return cursor;
  }

  void draw(int w, int h, float gr) {
    fill(0, 0, w, h, kBg, w, h);
    fill(0, 0, w, 1, kBorder, w, h);
    fill(0, h - 1, w, 1, kBorder, w, h);
    fill(0, 0, 1, h, kBorder, w, h);
    fill(w - 1, 0, 1, h, kBorder, w, h);
    fill(1, 1, w - 2, 1, kPanel, w, h);

    const int scale = (h >= 42 && w >= 150) ? 2 : 1;
    const int textH = 7 * scale;
    const int textY = (h - textH) / 2;

    char value[8];
    std::snprintf(value, sizeof(value), "%.1f", gr);

    int barX0 = 5;
    int barX1 = w - 6;
    if (w >= 90) {
      drawText(5, textY, "GR", kMuted, scale, w, h);
      barX0 = 5 + 18 * scale;
    }
    if (w >= 140) {
      const int valueW = static_cast<int>(std::strlen(value)) * 6 * scale;
      barX1 = w - 8 - valueW;
      drawText(w - 6 - valueW, textY, value, kText, scale, w, h);
    }

    const int barY = h / 2 - 3;
    const int barH = 6;
    if (barX1 <= barX0 + 4) {
      return;
    }
    fill(barX0, barY, barX1 - barX0, barH, kTrack, w, h);
    fill(barX0, barY, barX1 - barX0, 1, kTrack, w, h);

    // 0 .. 20 dB, filled from the right.
    const float n = std::min(std::max(gr / 20.0f, 0.0f), 1.0f);
    const int span = barX1 - barX0;
    const int filled = static_cast<int>(std::lround(n * span));
    if (filled > 0) {
      const uint32_t color = gr >= 8.0f ? kRed : (gr >= 3.0f ? kAmber : kGreen);
      fill(barX1 - filled, barY + 1, filled, barH - 2, color, w, h);
      fill(barX1 - filled, barY + 1, filled, 1, kText, w, h);
    }

    // Ticks at 3, 6, 12 and 20 dB.
    for (float mark : {3.0f, 6.0f, 12.0f, 20.0f}) {
      const int tx =
          barX1 - static_cast<int>(std::lround((mark / 20.0f) * span));
      fill(tx, barY - 2, 1, barH + 4, kMuted, w, h);
    }
    fill(barX1 - 1, barY - 3, 1, barH + 6, kMuted, w, h);
  }

  std::atomic<float> grDb_{0.0f};
  std::vector<uint32_t> pixels_;
  InlineDisplaySurface surface_;
};

} // namespace vocal
