# Vocal Suite

An eight-stage vocal strip for Linux, built for fast rock and metal vocals and
usable on pop, rap, podcast and spoken word. One plugin, CLAP and LV2, with a
single fixed serial chain.

```
INPUT → COMP → TONE → DRIVE → DE-ESS → ECHO → SPACE → OUT
```

Every stage is always in the chain. The tabs are pages, not separate
processors; each stage has its own IN/OUT switch.

## Status

`0.1.0` is a working development build:

- CLAP and LV2 load and process audio.
- The 1040×640 panel draws all eight pages, the chain strip, meters, preset
  browser and filters.
- 40 factory presets across Rock/Metal, Pop/Rap/Podcast, Screams, Clean and
  Utility.
- Input level learn, sibilance-frequency learn and auto makeup.
- Zero reported latency.
- DSP tests, panel tests and CLAP/LV2 host load tests pass.
- `lv2_validate` reports zero errors.

Still planned before a 1.0 release: final preset tuning against real vocal
takes, full host matrix testing, CI publishing, and the remaining UI polish.

## Install

Build and install into the per-user plugin directories:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCLAP_DIR="$HOME/.clap" \
  -DLV2_DIR="$HOME/.lv2"
cmake --build build -j"$(nproc)"
cmake --install build
```

The default install prefixes are under `/usr`; pass `-DCLAP_DIR` and
`-DLV2_DIR` for a home-directory install.

Requirements: CMake 3.16+, a C++17 compiler, pkg-config, LV2, Cairo, FreeType
and X11 development headers. `lilv` is optional and only needed for the LV2
host load test. The vendored `hearth-dsp` headers are included.

## Using it

1. Press **SET LEVEL** on the INPUT page and sing for two seconds. The input
   trim moves so the loudest phrase lands in the green zone.
2. Open the preset browser and filter by **STYLE**, **VOICE** and
   **INTENSITY**.
3. Adjust **COMP** for control, **DRIVE** for attitude, and **ECHO/SPACE** for
   depth. The recommended-range arc on each knob shows where it usually sits.
4. Hold **COMPARE** to hear the dry signal.

The default **Init — Natural** preset is a clean, level-neutral starting
point. **Modern Metal Lead** is the flagship rock/metal preset.

### Modules

- **INPUT** — ±18 dB trim, polarity, 12 dB/oct high-pass at 20–350 Hz.
- **COMP** — Opto, FET and VCA modes. Comp amount, speed, ratio, mix and auto
  makeup. The FET and VCA modes use 4:1, 8:1 and 20:1 ratio settings; Opto is
  program-dependent.
- **TONE** — low shelf/bell, mid bell with Q, high shelf/bell, and a 12 kHz air
  shelf. The curve display updates from the controls.
- **DRIVE** — Valve, Console, Tape and Clip voicings, ADAA anti-aliased, with
  automatic gain compensation.
- **DE-ESS** — two dynamic peaking bands: sibilance (4–10 kHz) and harshness
  (1.5–5 kHz). **LEARN** finds the dominant sibilance band.
- **ECHO** — tempo-synced or free stereo delay, 20 ms–4 s, ducking, width,
  modulation and LoFi feedback filtering.
- **SPACE** — eight-line FDN plate, room and hall with size, decay, pre-delay,
  tone, width, ducking and mix.
- **OUT** — ±18 dB trim and a zero-latency safety limiter at −0.3 dBFS.

## Presets

The browser filters by:

- **Style** — Rock, Metal, Pop, Scream, Clean, Utility.
- **Voice** — Any, Low, Mid, High.
- **Intensity** — Natural, Forward, Aggressive.

Factory presets are level-matched on a synthetic vocal reference. Final
matching against real takes is the last step before 1.0. Presets never contain
artist or song names.

## Formats and limits

- **CLAP** and **LV2** for Linux x86_64.
- Stereo in/out; mono input is duplicated.
- **Zero latency.** No oversampling and no lookahead. DRIVE uses ADAA.
- The output limiter is a safety clip, not a true-peak mastering limiter.
- Not in 0.1.0: gate/expander, doubler/width module, external sidechain,
  oversampling, VST3/AAX, user preset saving, IR loading.
- ECHO and SPACE stop when their stage is switched out; tail-preserving bypass
  is a later refinement.

## Plug-in URI

The CLAP and LV2 identity is
`https://github.com/tylorismusic-commits/vocal-suite`. It embeds the repository
path and is part of the saved-session format. **Do not rename the repository or
the GitHub account after release.**

## Licence

GPL-3.0-or-later. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.

Vocal Suite is an independent design. Workflow inspiration only: no
third-party plug-in code, artwork, control labels or preset names are used.
