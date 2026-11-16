// Vocal Suite — CLAP load/process smoke test
// SPDX-License-Identifier: GPL-3.0-or-later
#include <clap/all.h>

#include <dlfcn.h>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

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
  auto* entry = static_cast<const clap_plugin_entry_t*>(
      dlsym(lib, "clap_entry"));
  if (!entry) {
    std::fprintf(stderr, "no clap_entry\n");
    return 1;
  }
  if (!entry->init(argv[1])) {
    std::fprintf(stderr, "entry init failed\n");
    return 1;
  }
  const auto* factory = static_cast<const clap_plugin_factory_t*>(
      entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
  assert(factory);
  assert(factory->get_plugin_count(factory) == 1);
  const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);
  assert(desc);
  std::printf("plugin: %s\n", desc->name);

  const clap_plugin_t* plugin = factory->create_plugin(factory, nullptr, desc->id);
  assert(plugin);
  assert(plugin->init(plugin));
  assert(plugin->activate(plugin, 48000.0, 1, 512));

  const auto* params = static_cast<const clap_plugin_params_t*>(
      plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  assert(params);
  const uint32_t count = params->count(plugin);
  std::printf("params: %u\n", count);
  assert(count > 40);
  for (uint32_t i = 0; i < count; ++i) {
    clap_param_info_t info{};
    assert(params->get_info(plugin, i, &info));
    assert(info.id == i);
    double value = 0.0;
    assert(params->get_value(plugin, i, &value));
    assert(std::isfinite(value));
  }

  float inL[64] = {};
  float inR[64] = {};
  float outL[64] = {};
  float outR[64] = {};
  for (int i = 0; i < 64; ++i) {
    inL[i] = inR[i] = 0.1f * std::sin(2.0 * 3.141592653589793 * 220.0 * i / 48000.0);
  }
  float* inPtrs[2] = {inL, inR};
  float* outPtrs[2] = {outL, outR};
  clap_audio_buffer_t inBuf{};
  inBuf.data32 = inPtrs;
  inBuf.channel_count = 2;
  clap_audio_buffer_t outBuf{};
  outBuf.data32 = outPtrs;
  outBuf.channel_count = 2;
  clap_process_t proc{};
  proc.frames_count = 64;
  proc.audio_inputs = &inBuf;
  proc.audio_inputs_count = 1;
  proc.audio_outputs = &outBuf;
  proc.audio_outputs_count = 1;
  assert(plugin->process(plugin, &proc) == CLAP_PROCESS_CONTINUE);
  for (int i = 0; i < 64; ++i) {
    assert(std::isfinite(outL[i]) && std::isfinite(outR[i]));
  }

  plugin->deactivate(plugin);
  plugin->destroy(plugin);
  entry->deinit();
  dlclose(lib);
  std::printf("CLAP load/process ok\n");
  return 0;
}
