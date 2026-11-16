# Vocal Suite — quick manual

## Three-step start

1. **INPUT → SET LEVEL.** Sing the loudest phrase for two seconds. The trim
   moves so the peak sits in the green zone (about −12 dBFS).
2. **Preset browser.** Filter by **STYLE**, **VOICE** and **INTENSITY**, then
   pick the closest preset.
3. **COMPARE.** Click it to toggle the dry signal; click again to return.

Most presets need one or two moves after that. The recommended-range arc on
each knob shows where it usually sits.

## The chain

```
INPUT → COMP → TONE → DRIVE → DE-ESS → ECHO → SPACE → OUT
```

All eight stages are always in series. Tabs are pages. The LED on each tab is
the stage IN/OUT. The right rail always shows input, output, compressor GR and
de-esser GR.

## INPUT

- **Input** — trim before everything. Aim for the green zone.
- **HPF** — 90 Hz is a safe default. Low voices 70–85, high voices 100–120.
- **Polarity** — flip if the vocal sounds thin against another take.
- **SET LEVEL** — one-shot two-second level learn.

## COMP

- **Opto** — smooth, natural, program-dependent. Best for cleans, ballads,
  podcast and spoken word.
- **FET** — fast and aggressive. Best for rock, metal and screams.
- **VCA** — controlled and modern. Best for pop, rap and dense mixes.
- **Comp** — start at 3–5. 6–8 is forward; 8–10 is aggressive.
- **Speed** — fast for punch, slow for smooth leveling.
- **Ratio** — 4:1 general, 8:1 rock/metal, 20:1 screams and peaks.
- **Mix** — 100% for most vocals; 60–80% for a more open sound.
- **Auto** — leave on. It restores the level after compression.

## TONE

Broad strokes, not surgery.

- **Low** — 100–250 Hz. Cut a little for mud, add a little for body.
- **Mid** — 500–3000 Hz. 2–3 kHz adds presence; 300–600 Hz adds boxiness.
- **High** — 6–12 kHz. Add air, or cut if the vocal is brittle.
- **Air** — 12 kHz shelf. 2–6 dB is plenty on most voices.
- **Shelves vs bells** — shelves for broad tilt; bells for a focused move.

## DRIVE

- **Valve** — weight and harmonic thickness.
- **Console** — edge and midrange attitude.
- **Tape** — soft top and gentle compression.
- **Clip** — hard edge for screams and aggressive metal.
- **Drive** — 1–3 subtle, 3–5 audible, 5+ obvious.
- **Character** — even-harmonic bias; higher is denser.
- **Mix** — 50–100%; lower keeps the clean transient.

Auto gain keeps the module level-neutral.

## DE-ESS

- **Sibilance** — 5.5–8 kHz on most voices. **LEARN** finds it for you.
- **Harshness** — 2.5–3.5 kHz. Use it when the midrange stings.
- Start at amount 2–4. Raise until the S's stop poking out.
- The two bars show the current cut on each band.

## ECHO

- **Sync** on, **1/8D** is the rock/metal default.
- **Feedback** 20–45%; more becomes a effect, not a vocal.
- **Duck** 40–80% keeps the repeats under the vocal.
- **Width** 0 is mono, 100 is full ping-pong.
- **LoFi** band-limits the repeats.

## SPACE

- **Plate** — bright and dense; the default for leads.
- **Room** — tight; good for verses, screams and dry mixes.
- **Hall** — long; good for ballads and backgrounds.
- **Decay** 0.8–2.5 s for most vocals.
- **Pre-delay** 15–40 ms keeps the consonant clear.
- **Duck** 20–60% tucks the reverb behind the voice.

## OUT

- Output trim ±18 dB.
- **Limit** is a zero-latency safety clip at −0.3 dBFS. It is not a true-peak
  mastering limiter.

## Voice-type starting points

| Voice | HPF | Sibilance | Air |
|---|---|---|---|
| Low | 70–85 Hz | 5–6 kHz | 0–3 dB |
| Mid | 90–100 Hz | 6–7.5 kHz | 2–5 dB |
| High | 100–120 Hz | 7–9 kHz | 2–6 dB |

## Genre starting points

- **Modern metal** — FET, 5–7 comp, valve/clip drive, 1/8D ducked echo, short
  plate.
- **Metalcore / deathcore** — FET, 7–9 comp, clip drive, strong harshness
  de-ess, tight room.
- **Classic rock** — FET or VCA, 4–5 comp, valve drive, 1/4 echo, medium
  plate.
- **Pop** — Opto or VCA, 3–5 comp, console drive, 3–6 dB air, 1/8 or 1/4
  echo, 1.2–2 s plate.
- **Rap** — VCA, 4–7 comp, controlled low mids, little drive, short slap.
- **Podcast** — Opto, 3–5 comp, 2–3 de-ess, no drive, no echo, no reverb.
- **Screams** — FET, 7–9 comp, clip, 4–6 harshness, short room.

## Troubleshooting

- **S's poke out** — raise DE-ESS sibilance, or press LEARN again.
- **Too much mud** — raise the HPF, cut 250–400 Hz a little.
- **Too brittle** — lower Air and High, raise the de-ess harshness.
- **Too compressed** — lower Comp, lower Mix, or switch to Opto.
- **Reverb hides the words** — raise Duck, lower Mix, shorten Decay.
- **No input** — press SET LEVEL; check the host input routing.
- **Preset jumps level** — the factory set is level-matched on a synthetic
  reference; final tuning against real takes is still pending.
