// Vocal Suite — LV2 plugin
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Eight-stage vocal strip.  Control ports mirror the CLAP parameter ids:
// port = PORT_FIRST_PARAM + ParamId.  Two extra controls after the parameters
// are one-shot learn triggers from the UI.
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/time/time.h>
#include <lv2/urid/urid.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "dsp/engine.hpp"
#include "dsp/inline_display.hpp"
#include "params.hpp"

namespace {

constexpr const char* kPluginUri =
    "https://github.com/tylorismusic-commits/vocal-suite";

enum Port : uint32_t {
  PORT_AUDIO_IN_L = 0,
  PORT_AUDIO_IN_R = 1,
  PORT_AUDIO_OUT_L = 2,
  PORT_AUDIO_OUT_R = 3,
  PORT_FIRST_PARAM = 4,
  PORT_LEARN_INPUT = PORT_FIRST_PARAM + vocal::P_COUNT,
  PORT_LEARN_SIB,
  PORT_COMP_GR,
  PORT_SIB_GR,
  PORT_HARSH_GR,
  PORT_LIM_GR,
  PORT_OUT_PEAK_L,
  PORT_OUT_PEAK_R,
  PORT_IN_PEAK_L,
  PORT_IN_PEAK_R,
  PORT_INPUT_LEARN_PEAK,
  PORT_SIB_LEARN_HZ,
  PORT_SIB_LEARN_VALID,
  PORT_LATENCY,
  PORT_TIME,
  PORT_COUNT
};

struct Plugin {
  float sampleRate = 48000.0f;
  float lastPort[vocal::P_COUNT] = {};
  bool portsSeen = false;
  vocal::Shared shared;
  vocal::Engine engine;
  vocal::Meters meters;

  const LV2_URID_Map* map = nullptr;
  uint32_t uridPosition = 0;
  uint32_t uridBpm = 0;
  uint32_t uridFloat = 0;
  const LV2_Atom_Sequence* timePort = nullptr;

  std::unique_ptr<float*[]> ports;

  float* audioInL = nullptr;
  float* audioInR = nullptr;
  float* audioOutL = nullptr;
  float* audioOutR = nullptr;
  float* learnInput = nullptr;
  float* learnSib = nullptr;
  float* compGr = nullptr;
  float* sibGr = nullptr;
  float* harshGr = nullptr;
  float* limGr = nullptr;
  float* outPeakL = nullptr;
  float* outPeakR = nullptr;
  float* inPeakL = nullptr;
  float* inPeakR = nullptr;
  float* inputLearnPeak = nullptr;
  float* sibLearnHz = nullptr;
  float* sibLearnValid = nullptr;
  float* latency = nullptr;

  vocal::InlineDisplay display;
  const vocal::InlineDisplayQueueDraw* queueDraw = nullptr;
  uint32_t drawCounter = 0;
};

LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*,
                       const LV2_Feature* const* features) {
  auto* p = new Plugin();
  p->sampleRate = static_cast<float>(rate);
  p->engine.prepare(p->sampleRate);
  p->engine.setControls(vocal::controlsFrom(p->shared));

  for (const LV2_Feature* const* f = features; f && *f; ++f) {
    if (!std::strcmp((*f)->URI, LV2_URID__map)) {
      p->map = static_cast<const LV2_URID_Map*>(f[0]->data);
    } else if (!std::strcmp((*f)->URI, vocal::kInlineDisplayQueueDrawURI) &&
               (*f)->data) {
      p->queueDraw = static_cast<const vocal::InlineDisplayQueueDraw*>(
          static_cast<const void*>((*f)->data));
    }
  }
  if (p->map && p->map->map) {
    p->uridPosition = p->map->map(p->map->handle, LV2_TIME__Position);
    p->uridBpm = p->map->map(p->map->handle, LV2_TIME__beatsPerMinute);
    p->uridFloat = p->map->map(p->map->handle, LV2_ATOM__Float);
  }
  return p;
}

void connectPort(LV2_Handle handle, uint32_t port, void* data) {
  auto* p = static_cast<Plugin*>(handle);
  switch (port) {
    case PORT_AUDIO_IN_L: p->audioInL = static_cast<float*>(data); break;
    case PORT_AUDIO_IN_R: p->audioInR = static_cast<float*>(data); break;
    case PORT_AUDIO_OUT_L: p->audioOutL = static_cast<float*>(data); break;
    case PORT_AUDIO_OUT_R: p->audioOutR = static_cast<float*>(data); break;
    case PORT_LEARN_INPUT: p->learnInput = static_cast<float*>(data); break;
    case PORT_LEARN_SIB: p->learnSib = static_cast<float*>(data); break;
    case PORT_COMP_GR: p->compGr = static_cast<float*>(data); break;
    case PORT_SIB_GR: p->sibGr = static_cast<float*>(data); break;
    case PORT_HARSH_GR: p->harshGr = static_cast<float*>(data); break;
    case PORT_LIM_GR: p->limGr = static_cast<float*>(data); break;
    case PORT_OUT_PEAK_L: p->outPeakL = static_cast<float*>(data); break;
    case PORT_OUT_PEAK_R: p->outPeakR = static_cast<float*>(data); break;
    case PORT_IN_PEAK_L: p->inPeakL = static_cast<float*>(data); break;
    case PORT_IN_PEAK_R: p->inPeakR = static_cast<float*>(data); break;
    case PORT_INPUT_LEARN_PEAK:
      p->inputLearnPeak = static_cast<float*>(data);
      break;
    case PORT_SIB_LEARN_HZ: p->sibLearnHz = static_cast<float*>(data); break;
    case PORT_SIB_LEARN_VALID:
      p->sibLearnValid = static_cast<float*>(data);
      break;
    case PORT_LATENCY: p->latency = static_cast<float*>(data); break;
    case PORT_TIME:
      p->timePort = static_cast<const LV2_Atom_Sequence*>(data);
      break;
    default:
      if (!p->ports) {
        p->ports = std::make_unique<float*[]>(PORT_COUNT);
        std::memset(p->ports.get(), 0, sizeof(float*) * PORT_COUNT);
      }
      if (port < PORT_COUNT) {
        p->ports[port] = static_cast<float*>(data);
      }
      break;
  }
}

void updateTempo(Plugin* p) {
  if (!p->timePort || !p->uridPosition || !p->uridBpm) {
    return;
  }
  auto* seq = const_cast<LV2_Atom_Sequence*>(p->timePort);
  LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
    if (ev->body.type != p->uridPosition) {
      continue;
    }
    const auto* obj = reinterpret_cast<const LV2_Atom_Object*>(&ev->body);
    const LV2_Atom* bpmAtom = nullptr;
    LV2_Atom_Object_Query query[] = {
        {p->uridBpm, &bpmAtom},
        LV2_ATOM_OBJECT_QUERY_END,
    };
    lv2_atom_object_query(obj, query);
    if (bpmAtom && bpmAtom->type == p->uridFloat &&
        bpmAtom->size == sizeof(float)) {
      const float bpm = reinterpret_cast<const LV2_Atom_Float*>(bpmAtom)->body;
      if (std::isfinite(bpm) && bpm > 1.0f) {
        p->engine.setTempo(bpm, true);
      }
    }
  }
}

void activate(LV2_Handle handle) {
  auto* p = static_cast<Plugin*>(handle);
  p->engine.reset();
  if (p->latency) {
    *p->latency = static_cast<float>(p->engine.latencySamples());
  }
}

void deactivate(LV2_Handle) {}

void run(LV2_Handle handle, uint32_t frames) {
  auto* p = static_cast<Plugin*>(handle);
  if (!p->audioInL || !p->audioOutL) {
    return;
  }
  if (!p->audioInR) {
    p->audioInR = p->audioInL;
  }
  if (!p->audioOutR) {
    p->audioOutR = p->audioOutL;
  }

  if (p->ports) {
    for (uint32_t i = 0; i < vocal::P_COUNT; ++i) {
      float* v = p->ports[PORT_FIRST_PARAM + i];
      if (!v) {
        continue;
      }
      if (!p->portsSeen || *v != p->lastPort[i]) {
        p->shared.setFromHost(i, static_cast<double>(*v));
        p->lastPort[i] = *v;
      }
    }
    p->portsSeen = true;
  }
  if (p->learnInput) {
    p->shared.inputLearnActive.store(*p->learnInput > 0.5f,
                                     std::memory_order_relaxed);
  }
  if (p->learnSib) {
    p->shared.sibLearnActive.store(*p->learnSib > 0.5f,
                                   std::memory_order_relaxed);
  }
  updateTempo(p);
  p->engine.setControls(vocal::controlsFrom(p->shared));

  if (p->audioOutL != p->audioInL) {
    std::memcpy(p->audioOutL, p->audioInL, frames * sizeof(float));
  }
  if (p->audioOutR != p->audioInR) {
    std::memcpy(p->audioOutR, p->audioInR, frames * sizeof(float));
  }

  p->engine.process(p->audioOutL, p->audioOutR, static_cast<int>(frames),
                    p->meters);

  if (p->compGr) *p->compGr = p->meters.compGrDb;
  if (p->sibGr) *p->sibGr = p->meters.sibGrDb;
  if (p->harshGr) *p->harshGr = p->meters.harshGrDb;
  if (p->limGr) *p->limGr = p->meters.limGrDb;
  if (p->outPeakL) *p->outPeakL = p->meters.outPeakLDb;
  if (p->outPeakR) *p->outPeakR = p->meters.outPeakRDb;
  if (p->inPeakL) *p->inPeakL = p->meters.inPeakLDb;
  if (p->inPeakR) *p->inPeakR = p->meters.inPeakRDb;
  if (p->inputLearnPeak) *p->inputLearnPeak = p->meters.inputLearnPeakDb;
  if (p->sibLearnHz) *p->sibLearnHz = p->meters.sibLearnHz;
  if (p->sibLearnValid) {
    *p->sibLearnValid = p->meters.sibLearnValid ? 1.0f : 0.0f;
  }
  if (p->latency) {
    *p->latency = static_cast<float>(p->engine.latencySamples());
  }

  p->display.setGainReductionDb(p->meters.compGrDb);
  if (p->queueDraw && p->queueDraw->queue_draw &&
      ((p->drawCounter++ & 31u) == 0u)) {
    p->queueDraw->queue_draw(p->queueDraw->handle);
  }
}

void cleanup(LV2_Handle handle) { delete static_cast<Plugin*>(handle); }

vocal::InlineDisplaySurface* renderInlineDisplay(void* instance, uint32_t width,
                                                uint32_t height) {
  auto* p = static_cast<Plugin*>(instance);
  return p ? p->display.render(width, height) : nullptr;
}

const vocal::InlineDisplayInterface kInlineDisplay = {&renderInlineDisplay};

const void* extensionData(const char* uri) {
  if (uri && !std::strcmp(uri, vocal::kInlineDisplayInterfaceURI)) {
    return &kInlineDisplay;
  }
  return nullptr;
}

const LV2_Descriptor kDescriptor = {
    kPluginUri, instantiate, connectPort, activate, run, deactivate,
    cleanup, extensionData};

} // namespace

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
  return index == 0 ? &kDescriptor : nullptr;
}
