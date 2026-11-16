// Vocal Suite — LV2 UI open/idle smoke test
// SPDX-License-Identifier: GPL-3.0-or-later
#include <lv2/ui/ui.h>
#include <X11/Xlib.h>
#include <dlfcn.h>

#include <cassert>
#include <cstdio>
#include <cstring>

namespace {
void writeFn(LV2UI_Handle, uint32_t, uint32_t, uint32_t, const void*) {}
} // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <vocal_suite_ui.so>\n", argv[0]);
    return 1;
  }
  void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!lib) {
    std::fprintf(stderr, "dlopen: %s\n", dlerror());
    return 1;
  }
  auto* descriptorFn = reinterpret_cast<const LV2UI_Descriptor* (*)(uint32_t)>(
      dlsym(lib, "lv2ui_descriptor"));
  assert(descriptorFn);
  const LV2UI_Descriptor* desc = descriptorFn(0);
  assert(desc);

  Display* dpy = XOpenDisplay(nullptr);
  assert(dpy);
  const Window parent = XCreateSimpleWindow(
      dpy, RootWindow(dpy, DefaultScreen(dpy)), 0, 0, 1040, 640, 0, 0, 0);
  XMapWindow(dpy, parent);
  XFlush(dpy);

  LV2_Feature parentFeature{LV2_UI__parent,
                            reinterpret_cast<void*>(
                                static_cast<uintptr_t>(parent))};
  const LV2_Feature* features[] = {&parentFeature, nullptr};
  LV2UI_Widget widget = nullptr;
  LV2UI_Handle ui = desc->instantiate(
      desc, "https://github.com/tylorismusic-commits/vocal-suite", ".",
      writeFn, nullptr, &widget, features);
  assert(ui && widget);

  const auto* idle = static_cast<const LV2UI_Idle_Interface*>(
      desc->extension_data(LV2_UI__idleInterface));
  assert(idle);
  for (int i = 0; i < 5; ++i) {
    idle->idle(ui);
  }

  float v = 3.5f;
  desc->port_event(ui, 4 + 56 + 3, sizeof(float), 0, &v);
  desc->port_event(ui, 4 + 56 + 11, sizeof(float), 0, &v);
  desc->port_event(ui, 4 + 56 + 13, sizeof(float), 0, &v);
  for (int i = 0; i < 5; ++i) {
    idle->idle(ui);
  }

  desc->cleanup(ui);
  XDestroyWindow(dpy, parent);
  XCloseDisplay(dpy);
  dlclose(lib);
  std::printf("LV2 UI opened and idled\n");
  return 0;
}
