// Vocal Suite — LV2 load/process smoke test
// SPDX-License-Identifier: GPL-3.0-or-later
#include <lilv/lilv.h>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <bundle-dir>\n", argv[0]);
    return 1;
  }
  std::string bundle = argv[1];
  if (!bundle.empty() && bundle.back() != '/') {
    bundle.push_back('/');
  }
  LilvWorld* world = lilv_world_new();
  LilvNode* uri = lilv_new_file_uri(world, nullptr, bundle.c_str());
  lilv_world_load_bundle(world, uri);
  const LilvPlugins* plugins = lilv_world_get_all_plugins(world);
  const LilvPlugin* plugin = lilv_plugins_get_by_uri(
      plugins, lilv_new_uri(world, "https://github.com/tylorismusic-commits/vocal-suite"));
  if (!plugin) {
    std::fprintf(stderr, "plugin not found\n");
    return 1;
  }
  std::printf("ports: %u\n", lilv_plugin_get_num_ports(plugin));

  LilvInstance* inst = lilv_plugin_instantiate(plugin, 48000.0, nullptr);
  assert(inst);

  static float audioInL[64], audioInR[64], audioOutL[64], audioOutR[64];
  static float values[128] = {};
  for (int i = 0; i < 64; ++i) {
    audioInL[i] = audioInR[i] =
        0.1f * std::sin(2.0 * 3.141592653589793 * 220.0 * i / 48000.0);
  }
  lilv_instance_connect_port(inst, 0, audioInL);
  lilv_instance_connect_port(inst, 1, audioInR);
  lilv_instance_connect_port(inst, 2, audioOutL);
  lilv_instance_connect_port(inst, 3, audioOutR);

  const uint32_t nports = lilv_plugin_get_num_ports(plugin);
  for (uint32_t i = 4; i < nports; ++i) {
    values[i] = 0.0f;
    lilv_instance_connect_port(inst, i, &values[i]);
  }
  // Compressor is on by default in Shared, but the host writes every control
  // port, so set the defaults that matter for this smoke test.
  values[4 + 0] = 1.0f;   // power
  values[4 + 1] = 0.0f;   // input
  values[4 + 5] = 1.0f;   // comp on
  values[4 + 6] = 0.0f;   // opto
  values[4 + 7] = 3.0f;   // comp amount
  values[4 + 8] = 5.0f;   // speed
  values[4 + 10] = 100.0f; // comp mix
  values[4 + 28] = 1.0f;  // de-ess on
  values[4 + 33] = 0.0f;  // echo off
  values[4 + 45] = 0.0f;  // space off
  values[4 + 55] = 1.0f;  // limiter on

  lilv_instance_activate(inst);
  lilv_instance_run(inst, 64);
  for (int i = 0; i < 64; ++i) {
    assert(std::isfinite(audioOutL[i]) && std::isfinite(audioOutR[i]));
  }
  std::printf("LV2 comp GR port: %.2f dB\n", values[4 + 56 + 3]);
  assert(values[4 + 56 + 3] > 0.0f);

  // The compare port is a dry monitor: output must equal input.
  values[4 + 56 + 2] = 1.0f;  // compare
  lilv_instance_run(inst, 64);
  std::printf("compare port=%.1f out0=%.6f in0=%.6f out1=%.6f in1=%.6f\n",
              values[4 + 56 + 2], audioOutL[0], audioInL[0], audioOutL[1],
              audioInL[1]);
  for (int i = 0; i < 64; ++i) {
    assert(std::fabs(audioOutL[i] - audioInL[i]) < 1.0e-6f);
    assert(std::fabs(audioOutR[i] - audioInR[i]) < 1.0e-6f);
  }
  values[4 + 56 + 2] = 0.0f;
  lilv_instance_deactivate(inst);
  lilv_instance_free(inst);
  lilv_world_free(world);
  std::printf("LV2 load/process ok\n");
  return 0;
}
