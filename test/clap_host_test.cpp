// Vocal Suite — CLAP load/process smoke test
// SPDX-License-Identifier: GPL-3.0-or-later
#include <clap/all.h>

#include <dlfcn.h>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "params.hpp"

namespace {
struct EventList {
  const clap_event_header_t* event = nullptr;
};

uint32_t eventCount(const clap_input_events_t*) { return 1; }

const clap_event_header_t* eventGet(const clap_input_events_t* list,
                                    uint32_t index) {
  if (index != 0) {
    return nullptr;
  }
  return static_cast<const EventList*>(list->ctx)->event;
}

int64_t streamWrite(const clap_ostream_t* stream, const void* buffer,
                    uint64_t size) {
  auto* out = static_cast<std::vector<uint8_t>*>(stream->ctx);
  const auto* bytes = static_cast<const uint8_t*>(buffer);
  out->insert(out->end(), bytes, bytes + size);
  return static_cast<int64_t>(size);
}

int64_t streamRead(const clap_istream_t* stream, void* buffer, uint64_t size) {
  auto* in = static_cast<std::vector<uint8_t>*>(stream->ctx);
  static size_t offset = 0;
  const size_t available = in->size() - offset;
  const size_t n = std::min<size_t>(available, size);
  std::memcpy(buffer, in->data() + offset, n);
  offset += n;
  return static_cast<int64_t>(n);
}
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

  // With the editor absent, host parameter events must still update the
  // visible parameter value and the DSP.
  {
    clap_event_param_value_t ev{};
    ev.header.size = sizeof(ev);
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type = CLAP_EVENT_PARAM_VALUE;
    ev.param_id = 7;  // P_COMP_AMOUNT
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.value = 9.0;
    EventList list{&ev.header};
    clap_input_events_t events{};
    events.ctx = &list;
    events.size = eventCount;
    events.get = eventGet;
    params->flush(plugin, &events, nullptr);
    double reported = 0.0;
    assert(params->get_value(plugin, 7, &reported));
    std::printf("reported comp amount: %.2f\n", reported);
    assert(std::fabs(reported - 9.0) < 1.0e-9);
  }

  // State save/load works without the editor, and parameters report the
  // restored values afterwards.
  {
    const auto* state = static_cast<const clap_plugin_state_t*>(
        plugin->get_extension(plugin, CLAP_EXT_STATE));
    assert(state && state->save && state->load);
    std::vector<uint8_t> saved;
    clap_ostream_t out{};
    out.ctx = &saved;
    out.write = streamWrite;
    assert(state->save(plugin, &out));
    assert(saved.size() > 8);

    clap_event_param_value_t ev{};
    ev.header.size = sizeof(ev);
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type = CLAP_EVENT_PARAM_VALUE;
    ev.param_id = 7;
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.value = 1.0;
    EventList list{&ev.header};
    clap_input_events_t events{};
    events.ctx = &list;
    events.size = eventCount;
    events.get = eventGet;
    params->flush(plugin, &events, nullptr);
    double changed = 0.0;
    assert(params->get_value(plugin, 7, &changed));
    assert(std::fabs(changed - 1.0) < 1.0e-9);

    clap_istream_t in{};
    in.ctx = &saved;
    in.read = streamRead;
    assert(state->load(plugin, &in));
    double restored = 0.0;
    assert(params->get_value(plugin, 7, &restored));
    assert(std::fabs(restored - 9.0) < 1.0e-9);
  }

  // The compressor must publish gain reduction through the CLAP gain
  // adjustment metering extension, with the editor closed.
  {
    const auto* gainMeter = static_cast<
        const clap_plugin_gain_adjustment_metering_t*>(
        plugin->get_extension(plugin, CLAP_EXT_GAIN_ADJUSTMENT_METERING));
    assert(gainMeter && gainMeter->get);
    for (int i = 0; i < 64; ++i) {
      inL[i] = inR[i] =
          0.35f * std::sin(2.0 * 3.141592653589793 * 220.0 * i / 48000.0);
    }
    for (int block = 0; block < 8; ++block) {
      assert(plugin->process(plugin, &proc) == CLAP_PROCESS_CONTINUE);
    }
    const double gr = gainMeter->get(plugin);
    std::printf("host GR: %.2f dB\n", gr);
    assert(gr < -0.2);
  }

  // Host transport tempo must drive the echo division.  The manual tempo
  // fallback is 120 BPM; the host says 90, so 1/8D is 500 ms (24000 samples).
  {
    auto setParam = [&](uint32_t id, double value) {
      clap_event_param_value_t ev{};
      ev.header.size = sizeof(ev);
      ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      ev.header.type = CLAP_EVENT_PARAM_VALUE;
      ev.param_id = id;
      ev.note_id = -1;
      ev.port_index = -1;
      ev.channel = -1;
      ev.key = -1;
      ev.value = value;
      EventList list{&ev.header};
      clap_input_events_t events{};
      events.ctx = &list;
      events.size = eventCount;
      events.get = eventGet;
      params->flush(plugin, &events, nullptr);
    };
    setParam(vocal::P_COMP_ON, 0.0);
    setParam(vocal::P_TONE_ON, 0.0);
    setParam(vocal::P_DEESS_ON, 0.0);
    setParam(vocal::P_DRIVE_ON, 0.0);
    setParam(vocal::P_SPACE_ON, 0.0);
    setParam(vocal::P_LIMIT_ON, 0.0);
    setParam(vocal::P_ECHO_ON, 1.0);
    setParam(vocal::P_ECHO_SYNC, 1.0);
    setParam(vocal::P_ECHO_DIV, 3.0);
    setParam(vocal::P_ECHO_MIX, 100.0);
    setParam(vocal::P_ECHO_FEEDBACK, 0.0);
    setParam(vocal::P_ECHO_DUCK, 0.0);
    setParam(vocal::P_ECHO_MOD, 0.0);
    setParam(vocal::P_ECHO_TONE, 100.0);

    const uint32_t n = 48000;
    std::vector<float> a(n, 0.0f), b(n, 0.0f), c(n, 0.0f), d(n, 0.0f);
    a[0] = b[0] = 0.5f;
    float* inP[2] = {a.data(), b.data()};
    float* outP[2] = {c.data(), d.data()};
    clap_audio_buffer_t tempoIn{};
    tempoIn.data32 = inP;
    tempoIn.channel_count = 2;
    clap_audio_buffer_t tempoOut{};
    tempoOut.data32 = outP;
    tempoOut.channel_count = 2;
    clap_event_transport_t transport{};
    transport.header.size = sizeof(transport);
    transport.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    transport.header.type = CLAP_EVENT_TRANSPORT;
    transport.flags = CLAP_TRANSPORT_HAS_TEMPO;
    transport.tempo = 90.0;
    clap_process_t tempoProc{};
    tempoProc.frames_count = n;
    tempoProc.audio_inputs = &tempoIn;
    tempoProc.audio_inputs_count = 1;
    tempoProc.audio_outputs = &tempoOut;
    tempoProc.audio_outputs_count = 1;
    tempoProc.transport = &transport;
    assert(plugin->process(plugin, &tempoProc) == CLAP_PROCESS_CONTINUE);
    int peak = 0;
    float peakV = 0.0f;
    for (uint32_t i = 100; i < n; ++i) {
      if (std::fabs(c[i]) > peakV) {
        peakV = std::fabs(c[i]);
        peak = static_cast<int>(i);
      }
    }
    std::printf("host-tempo echo peak: %d samples (%.1f ms)\n", peak,
                peak / 48.0);
    assert(std::fabs(peak - 24000) < 250);
  }

  plugin->deactivate(plugin);
  plugin->destroy(plugin);
  entry->deinit();
  dlclose(lib);
  std::printf("CLAP load/process ok\n");
  return 0;
}
