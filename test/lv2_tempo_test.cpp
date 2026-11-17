// Vocal Suite — LV2 host-tempo test
// SPDX-License-Identifier: GPL-3.0-or-later
#include <lilv/lilv.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "params.hpp"

namespace {

struct Urids {
  LV2_URID position = 1;
  LV2_URID bpm = 2;
  LV2_URID floatType = 3;
  LV2_URID object = 4;
  LV2_URID sequence = 5;
};

LV2_URID mapUrid(LV2_URID_Map_Handle handle, const char* uri) {
  auto* u = static_cast<Urids*>(handle);
  if (!std::strcmp(uri, LV2_TIME__Position)) return u->position;
  if (!std::strcmp(uri, LV2_TIME__beatsPerMinute)) return u->bpm;
  if (!std::strcmp(uri, LV2_ATOM__Float)) return u->floatType;
  if (!std::strcmp(uri, LV2_ATOM__Object)) return u->object;
  if (!std::strcmp(uri, LV2_ATOM__Sequence)) return u->sequence;
  return 100;
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <bundle-dir>\n", argv[0]);
    return 1;
  }
  std::string bundle = argv[1];
  if (!bundle.empty() && bundle.back() != '/') bundle.push_back('/');

  LilvWorld* world = lilv_world_new();
  LilvNode* bundleUri = lilv_new_file_uri(world, nullptr, bundle.c_str());
  lilv_world_load_bundle(world, bundleUri);
  const LilvPlugin* plugin = lilv_plugins_get_by_uri(
      lilv_world_get_all_plugins(world),
      lilv_new_uri(world,
                   "https://github.com/tylorismusic-commits/vocal-suite"));
  assert(plugin);

  Urids urids;
  LV2_URID_Map map{&urids, mapUrid};
  LV2_Feature mapFeature{LV2_URID__map, &map};
  const LV2_Feature* features[] = {&mapFeature, nullptr};
  LilvInstance* inst = lilv_plugin_instantiate(plugin, 48000.0, features);
  assert(inst);

  constexpr uint32_t n = 48000;
  std::vector<float> inL(n, 0.0f), inR(n, 0.0f);
  std::vector<float> outL(n, 0.0f), outR(n, 0.0f);
  inL[0] = inR[0] = 0.5f;
  lilv_instance_connect_port(inst, 0, inL.data());
  lilv_instance_connect_port(inst, 1, inR.data());
  lilv_instance_connect_port(inst, 2, outL.data());
  lilv_instance_connect_port(inst, 3, outR.data());

  static float values[128] = {};
  const uint32_t nports = lilv_plugin_get_num_ports(plugin);
  for (uint32_t i = 4; i < nports; ++i) {
    values[i] = 0.0f;
    lilv_instance_connect_port(inst, i, &values[i]);
  }

  values[4 + vocal::P_POWER] = 1.0f;
  values[4 + vocal::P_COMP_ON] = 0.0f;
  values[4 + vocal::P_TONE_ON] = 0.0f;
  values[4 + vocal::P_DEESS_ON] = 0.0f;
  values[4 + vocal::P_DRIVE_ON] = 0.0f;
  values[4 + vocal::P_SPACE_ON] = 0.0f;
  values[4 + vocal::P_LIMIT_ON] = 0.0f;
  values[4 + vocal::P_ECHO_ON] = 1.0f;
  values[4 + vocal::P_ECHO_SYNC] = 1.0f;
  values[4 + vocal::P_ECHO_DIV] = 3.0f;  // 1/8D
  values[4 + vocal::P_ECHO_MIX] = 100.0f;
  values[4 + vocal::P_ECHO_FEEDBACK] = 0.0f;
  values[4 + vocal::P_ECHO_DUCK] = 0.0f;
  values[4 + vocal::P_ECHO_MOD] = 0.0f;
  values[4 + vocal::P_ECHO_TONE] = 100.0f;
  values[4 + vocal::P_TEMPO] = 120.0f;

  // Build a time:Position event carrying 90 BPM.
  uint8_t seqBuf[256] = {};
  auto* seq = reinterpret_cast<LV2_Atom_Sequence*>(seqBuf);
  seq->atom.type = urids.sequence;
  seq->atom.size = sizeof(LV2_Atom_Sequence_Body);
  seq->body.unit = 0;
  seq->body.pad = 0;

  uint8_t evBuf[64] = {};
  auto* ev = reinterpret_cast<LV2_Atom_Event*>(evBuf);
  ev->time.frames = 0;
  ev->body.type = urids.position;
  ev->body.size = sizeof(LV2_Atom_Object_Body) +
                  sizeof(LV2_Atom_Property_Body) + sizeof(float);
  auto* obj = reinterpret_cast<LV2_Atom_Object_Body*>(
      evBuf + sizeof(LV2_Atom_Event));
  obj->id = urids.position;
  obj->otype = urids.position;
  auto* prop = reinterpret_cast<LV2_Atom_Property_Body*>(
      reinterpret_cast<uint8_t*>(obj) + sizeof(LV2_Atom_Object_Body));
  prop->key = urids.bpm;
  prop->context = 0;
  prop->value.type = urids.floatType;
  prop->value.size = sizeof(float);
  *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(&prop->value) +
                            sizeof(LV2_Atom)) = 90.0f;
  assert(lv2_atom_sequence_append_event(seq, sizeof(seqBuf), ev));
  lilv_instance_connect_port(inst, 4 + vocal::P_COUNT + 17, seqBuf);

  lilv_instance_activate(inst);
  lilv_instance_run(inst, n);
  lilv_instance_deactivate(inst);
  std::printf("host_bpm port=%.1f host_tempo=%.1f\n",
              values[4 + vocal::P_COUNT + 14],
              values[4 + vocal::P_COUNT + 15]);
  std::printf("power=%.1f echo_on=%.1f sync=%.1f mix=%.1f timeout0=%.3f peak_out=%.3f\n",
              values[4 + vocal::P_POWER], values[4 + vocal::P_ECHO_ON],
              values[4 + vocal::P_ECHO_SYNC], values[4 + vocal::P_ECHO_MIX],
              outL[0], outL[20000]);
  {
    float mx = 0.0f;
    int mi = 0;
    for (uint32_t i = 100; i < n; ++i) {
      if (std::fabs(outL[i]) > mx) { mx = std::fabs(outL[i]); mi = static_cast<int>(i); }
    }
    std::printf("max after 100: %.6f at %d, at 18000=%.6f at 24000=%.6f\n",
                mx, mi, outL[18000], outL[24000]);
  }

  int peak = 0;
  float peakV = 0.0f;
  for (uint32_t i = 100; i < n; ++i) {
    if (std::fabs(outL[i]) > peakV) {
      peakV = std::fabs(outL[i]);
      peak = static_cast<int>(i);
    }
  }
  std::printf("LV2 host-tempo echo peak: %d samples (%.1f ms)\n", peak,
              peak / 48.0);
  assert(std::fabs(peak - 24000) < 250);

  lilv_instance_free(inst);
  lilv_world_free(world);
  std::printf("LV2 host-tempo sync ok\n");
  return 0;
}
