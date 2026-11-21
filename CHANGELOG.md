# Changelog

## Unreleased

- Echo and Space Mix default to 15% while both pages stay off, so switching a
  page on is immediately audible.
- The Compare button slowly breathes while engaged so the DRY state is obvious.

## 0.2.0-alpha.1 — 2026-11-19

- New **GATE** stage before the compressor: threshold, range, attack and
  release, with a 3 dB hysteresis window.
- New **DOUBLE** stage before the echo: amount, delay, width and mix using
  two short modulated delay taps.
- Double-click now restores the active preset value, falling back to the
  factory default when no preset is selected.
- Two new factory presets: Tight Gate Vocal and Double Wide.
- Panel now has ten stage pages.

## 0.1.0-alpha.2 — 2026-11-19

- User preset bank: save, rename, duplicate, delete, export and drag-drop
  import, alongside the 40 factory presets.
- Preset files live in `~/.config/VocalSuite/presets` and export to
  `~/Documents/Vocal Suite Presets`.

## 0.1.0-alpha.1 — first public alpha

First public Linux alpha build.

- Limiter ceiling control (−6…0 dBFS) on the OUTPUT page.
- Right-click a knob/fader to type a value; double-click resets it.
- Echo host-tempo sync now follows the project BPM in CLAP and LV2. Time is a
  display-only readout of the selected division when Sync is on.
- Compressor gain reduction is published to hosts (CLAP gain metering, LV2 GR
  port and inline display).

- New Vocal Suite project: CLAP + LV2 eight-stage vocal strip.
- Fixed serial chain: INPUT → COMP → TONE → DRIVE → DE-ESS → ECHO → SPACE →
  OUT, with per-stage IN/OUT.
- COMP modes: Opto, FET and VCA, with auto makeup.
- TONE: low shelf/bell, mid bell with Q, high shelf/bell and air.
- DRIVE: Valve, Console, Tape and Clip, ADAA, auto gain.
- DE-ESS: sibilance and harshness dynamic cuts, with a one-shot LEARN action.
- ECHO: tempo-synced/free stereo delay with ducking, width, modulation and
  LoFi feedback.
- SPACE: eight-line FDN plate/room/hall with ducking and width.
- OUT: output trim and zero-latency safety limiter.
- 1040×640 panel with chain strip, right-hand meters, preset browser and
  Style/Voice/Intensity filters.
- 40 factory presets across Rock/Metal, Pop/Rap/Podcast, Screams, Clean and
  Utility.
- Input SET LEVEL learn, auto makeup and hold-to-compare.
- CLAP and LV2 host load tests, DSP tests, panel tests, LV2 metadata
  validation.
