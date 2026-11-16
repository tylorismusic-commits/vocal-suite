// Vocal Suite — CLAP editor open/close smoke test
// SPDX-License-Identifier: GPL-3.0-or-later
#include <clap/all.h>
#include <X11/Xlib.h>
#include <dlfcn.h>

#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

namespace {
const clap_host_t kHost = [] {
  clap_host_t h{};
  h.clap_version = CLAP_VERSION;
  h.name = "vocal-suite-test";
  h.vendor = "test";
  h.url = "";
  h.version = "0";
  h.get_extension = [](const clap_host_t*, const char*) -> const void* {
    return nullptr;
  };
  return h;
}();
} // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <VocalSuite.clap>\n", argv[0]);
    return 1;
  }
  void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!lib) {
    std::fprintf(stderr, "dlopen: %s\n", dlerror());
    return 1;
  }
  auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(lib, "clap_entry"));
  assert(entry && entry->init(argv[1]));
  const auto* factory = static_cast<const clap_plugin_factory_t*>(
      entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
  const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);
  const clap_plugin_t* plugin = factory->create_plugin(factory, &kHost, desc->id);
  assert(plugin && plugin->init(plugin));

  const auto* gui = static_cast<const clap_plugin_gui_t*>(
      plugin->get_extension(plugin, CLAP_EXT_GUI));
  assert(gui && gui->is_api_supported(plugin, CLAP_WINDOW_API_X11, false));

  Display* dpy = XOpenDisplay(nullptr);
  assert(dpy);
  const Window parent = XCreateSimpleWindow(
      dpy, RootWindow(dpy, DefaultScreen(dpy)), 0, 0, 1040, 640, 0, 0, 0);
  XMapWindow(dpy, parent);
  XFlush(dpy);

  assert(gui->create(plugin, CLAP_WINDOW_API_X11, false));
  clap_window_t window{};
  window.api = CLAP_WINDOW_API_X11;
  window.x11 = parent;
  assert(gui->set_parent(plugin, &window));
  uint32_t w = 0, h = 0;
  assert(gui->get_size(plugin, &w, &h));
  assert(w >= 520 && h >= 320);

  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  gui->show(plugin);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  gui->hide(plugin);
  gui->destroy(plugin);

  XDestroyWindow(dpy, parent);
  XCloseDisplay(dpy);
  plugin->destroy(plugin);
  entry->deinit();
  dlclose(lib);
  std::printf("CLAP editor opened at %ux%u\n", w, h);
  return 0;
}
