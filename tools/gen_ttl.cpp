// Vocal Suite — LV2 metadata generator
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Emits manifest.ttl, vocal_suite.ttl and presets.ttl from params.hpp and
// presets.hpp.  Run from the repository root:
//   ./build/vocal_ttl_gen
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "params.hpp"
#include "presets.hpp"

namespace {

constexpr const char* kUri =
    "https://github.com/tylorismusic-commits/vocal-suite";

std::string esc(const char* s) {
  std::string out;
  for (const char* p = s; p && *p; ++p) {
    if (*p == '"' || *p == '\\') {
      out.push_back('\\');
    }
    out.push_back(*p);
  }
  return out;
}

std::string slug(const char* s) {
  std::string out;
  for (const char* p = s; p && *p; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9')) {
      out.push_back(static_cast<char>(c));
    } else if (!out.empty() && out.back() != '_') {
      out.push_back('_');
    }
  }
  return out;
}

const char* unitUri(const char* unit) {
  if (!unit) return nullptr;
  if (!std::strcmp(unit, "dB")) return "units:db";
  if (!std::strcmp(unit, "Hz")) return "units:hz";
  if (!std::strcmp(unit, "%")) return "units:pc";
  if (!std::strcmp(unit, "ms")) return "units:ms";
  if (!std::strcmp(unit, "s")) return "units:s";
  if (!std::strcmp(unit, "BPM")) return "units:bpm";
  return nullptr;
}

void printPort(uint32_t index, const vocal::ParamSpec& s, bool output) {
  std::printf("<#%s> a lv2:%sPort, lv2:ControlPort ;\n", s.symbol,
              output ? "Output" : "Input");
  std::printf("  lv2:index %u ;\n", index);
  std::printf("  lv2:symbol \"%s\" ;\n", s.symbol);
  std::printf("  lv2:name \"%s\" ;\n", esc(s.name).c_str());
  if (!output) {
    std::printf("  lv2:default %g ;\n", s.def);
    std::printf("  lv2:minimum %g ;\n", s.min);
    std::printf("  lv2:maximum %g ;\n", s.max);
    if (s.stepped) {
      std::printf("  lv2:portProperty lv2:integer ;\n");
      for (int i = 0; i < s.labels.count; ++i) {
        std::printf("  lv2:scalePoint [ rdfs:label \"%s\" ; rdf:value %d ] ;\n",
                    s.labels.names[i], i);
      }
    }
    if (s.toggled) {
      std::printf("  lv2:portProperty lv2:toggled ;\n");
    }
  }
  if (const char* u = unitUri(s.unit)) {
    std::printf("  units:unit %s ;\n", u);
  }
  std::printf("  rdfs:comment \"Recommended %g to %g\" .\n\n", s.recMin,
              s.recMax);
}

} // namespace

int main() {
  std::printf("@prefix lv2:  <http://lv2plug.in/ns/lv2core#> .\n");
  std::printf("@prefix rdf:  <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .\n");
  std::printf("@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .\n");
  std::printf("@prefix units: <http://lv2plug.in/ns/extensions/units#> .\n");
  std::printf("@prefix doap: <http://usefulinc.com/ns/doap#> .\n");
  std::printf("@prefix foaf: <http://xmlns.com/foaf/0.1/> .\n");
  std::printf("@prefix ui:   <http://lv2plug.in/ns/extensions/ui#> .\n");
  std::printf("@prefix pset: <http://lv2plug.in/ns/ext/presets#> .\n");
  std::printf("@prefix atom: <http://lv2plug.in/ns/ext/atom#> .\n");
  std::printf("@prefix time: <http://lv2plug.in/ns/ext/time#> .\n");
  std::printf("@prefix pg:   <http://lv2plug.in/ns/ext/port-groups#> .\n");
  std::printf("@prefix idpy: <http://harrisonconsoles.com/lv2/inlinedisplay#> .\n\n");
  std::printf("idpy:interface a lv2:ExtensionData .\n");
  std::printf("idpy:queue_draw a lv2:Feature .\n\n");

  // manifest.ttl
  std::printf("<%s>\n", kUri);
  std::printf("  a lv2:Plugin, doap:Project ;\n");
  std::printf("  lv2:binary <vocal_suite.so> ;\n");
  std::printf("  rdfs:seeAlso <vocal_suite.ttl> .\n\n");
  std::printf("<%s#ui>\n", kUri);
  std::printf("  a ui:X11UI ;\n");
  std::printf("  lv2:binary <vocal_suite_ui.so> ;\n");
  std::printf("  rdfs:seeAlso <vocal_suite.ttl> .\n");

  // plugin ttls
  std::printf("\n# ---- plugin ----\n");
  std::printf("@prefix vocal: <%s> .\n\n", kUri);
  std::printf("<%s> a lv2:Plugin ;\n", kUri);
  std::printf("  doap:name \"Vocal Suite\" ;\n");
  std::printf("  doap:license <https://spdx.org/licenses/GPL-3.0-or-later.html> ;\n");
  std::printf("  doap:maintainer [ foaf:name \"tylorismusic\" ] ;\n");
  std::printf("  lv2:optionalFeature lv2:hardRTCapable , idpy:queue_draw ;\n");
  std::printf("  lv2:extensionData idpy:interface ;\n");
  std::printf("  lv2:port\n");
  std::printf("    <#audio_in_l> , <#audio_in_r> , <#audio_out_l> , <#audio_out_r> ,\n");
  for (uint32_t i = 0; i < vocal::P_COUNT; ++i) {
    std::printf("    <#%s>%s\n", vocal::kParamSpecs[i].symbol,
                i + 1 < vocal::P_COUNT ? " ," : " ;");
  }
  std::printf("  lv2:port\n");
  std::printf("    <#learn_input> , <#learn_sib> , <#compare> ,\n");
  std::printf("    <#comp_gr> , <#sib_gr> , <#harsh_gr> , <#lim_gr> ,\n");
  std::printf("    <#in_l> , <#in_r> , <#out_l> , <#out_r> ,\n");
  std::printf("    <#input_learn_peak> , <#sib_learn_hz> , <#sib_learn_valid> ,\n");
  std::printf("    <#latency> , <#time> ;\n");
  std::printf("  ui:ui <#ui> .\n\n");

  std::printf("<#ui> a ui:X11UI ;\n");
  std::printf("  ui:binary <vocal_suite_ui.so> ;\n");
  std::printf("  lv2:optionalFeature ui:resize ;\n");
  std::printf("  lv2:requiredFeature ui:idleInterface ;\n");
  std::printf("  lv2:extensionData ui:idleInterface .\n\n");

  std::printf("<#audio_in_l> a lv2:AudioPort, lv2:InputPort ;\n");
  std::printf("  lv2:index 0 ; lv2:symbol \"audio_in_l\" ; lv2:name \"Audio In L\" .\n\n");
  std::printf("<#audio_in_r> a lv2:AudioPort, lv2:InputPort ;\n");
  std::printf("  lv2:index 1 ; lv2:symbol \"audio_in_r\" ; lv2:name \"Audio In R\" .\n\n");
  std::printf("<#audio_out_l> a lv2:AudioPort, lv2:OutputPort ;\n");
  std::printf("  lv2:index 2 ; lv2:symbol \"audio_out_l\" ; lv2:name \"Audio Out L\" .\n\n");
  std::printf("<#audio_out_r> a lv2:AudioPort, lv2:OutputPort ;\n");
  std::printf("  lv2:index 3 ; lv2:symbol \"audio_out_r\" ; lv2:name \"Audio Out R\" .\n\n");

  for (uint32_t i = 0; i < vocal::P_COUNT; ++i) {
    printPort(4 + i, vocal::kParamSpecs[i], false);
  }

  std::printf("<#learn_input> a lv2:InputPort, lv2:ControlPort ;\n");
  std::printf("  lv2:index %u ; lv2:symbol \"learn_input\" ; lv2:name \"Learn Input\" ;\n", 4 + vocal::P_COUNT);
  std::printf("  lv2:default 0 ; lv2:minimum 0 ; lv2:maximum 1 ;\n");
  std::printf("  lv2:portProperty lv2:toggled .\n\n");
  std::printf("<#learn_sib> a lv2:InputPort, lv2:ControlPort ;\n");
  std::printf("  lv2:index %u ; lv2:symbol \"learn_sib\" ; lv2:name \"Learn Sibilance\" ;\n", 4 + vocal::P_COUNT + 1);
  std::printf("  lv2:default 0 ; lv2:minimum 0 ; lv2:maximum 1 ;\n");
  std::printf("  lv2:portProperty lv2:toggled .\n\n");
  std::printf("<#compare> a lv2:InputPort, lv2:ControlPort ;\n");
  std::printf("  lv2:index %u ; lv2:symbol \"compare\" ; lv2:name \"Compare Dry\" ;\n", 4 + vocal::P_COUNT + 2);
  std::printf("  lv2:default 0 ; lv2:minimum 0 ; lv2:maximum 1 ;\n");
  std::printf("  lv2:portProperty lv2:toggled .\n\n");

  const char* meters[] = {"comp_gr", "sib_gr", "harsh_gr", "lim_gr",
                          "out_l", "out_r", "in_l", "in_r",
                          "input_learn_peak", "sib_learn_hz", "sib_learn_valid"};
  for (int i = 0; i < 11; ++i) {
    const uint32_t index = 4 + vocal::P_COUNT + 3 + i;
    std::printf("<#%s> a lv2:OutputPort, lv2:ControlPort ;\n", meters[i]);
    std::printf("  lv2:index %u ;\n", index);
    std::printf("  lv2:symbol \"%s\" ;\n", meters[i]);
    std::printf("  lv2:name \"%s\" ;\n", meters[i]);
    std::printf("  lv2:minimum -120 ;\n");
    std::printf("  lv2:maximum 12 ;\n");
    std::printf("  lv2:default -120 ;\n");
    std::printf("  units:unit units:db .\n\n");
  }
  const uint32_t latencyIndex = 4 + vocal::P_COUNT + 14;
  std::printf("<#latency> a lv2:OutputPort, lv2:ControlPort ;\n");
  std::printf("  lv2:index %u ;\n", latencyIndex);
  std::printf("  lv2:symbol \"latency\" ;\n");
  std::printf("  lv2:name \"Latency\" ;\n");
  std::printf("  lv2:designation lv2:latency ;\n");
  std::printf("  lv2:minimum 0 ;\n");
  std::printf("  lv2:maximum 0 ;\n");
  std::printf("  lv2:default 0 .\n\n");
  std::printf("<#time> a lv2:InputPort, atom:Sequence ;\n");
  std::printf("  lv2:index %u ;\n", 4 + vocal::P_COUNT + 15);
  std::printf("  lv2:symbol \"time\" ;\n");
  std::printf("  lv2:name \"Time\" ;\n");
  std::printf("  lv2:designation time:position ;\n");
  std::printf("  atom:supports time:Position .\n");

  // presets
  std::printf("\n# ---- presets ----\n");
  const auto& presets = vocal::factoryPresets();
  std::vector<std::string> banks;
  for (const auto& p : presets) {
    bool seen = false;
    for (const auto& b : banks) {
      if (b == p.category) {
        seen = true;
        break;
      }
    }
    if (!seen) {
      banks.push_back(p.category);
      std::printf("<%s#bank_%s> a pset:Bank ;\n", kUri, slug(p.category).c_str());
      std::printf("  rdfs:label \"%s\" .\n\n", esc(p.category).c_str());
    }
  }
  for (const auto& p : presets) {
    std::printf("<%s#preset_%s> a pset:Preset ;\n", kUri, slug(p.name).c_str());
    std::printf("  lv2:appliesTo <%s> ;\n", kUri);
    std::printf("  rdfs:label \"%s\" ;\n", esc(p.name).c_str());
    std::printf("  rdfs:comment \"%s\" ;\n", esc(p.blurb).c_str());
    std::printf("  pset:bank <%s#bank_%s> ;\n", kUri,
                slug(p.category).c_str());
    for (uint32_t i = 0; i < vocal::P_COUNT; ++i) {
      std::printf("  lv2:port [ lv2:symbol \"%s\" ; pset:value %g ]%s\n",
                  vocal::kParamSpecs[i].symbol, p.values[i],
                  i + 1 < vocal::P_COUNT ? " ;" : " .");
    }
    std::printf("\n");
  }
  return 0;
}
