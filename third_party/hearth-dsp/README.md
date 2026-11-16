# hearth-dsp

Header-only DSP modules shared by Comp Suite and future plugins. GPL-3.0-or-later.

Extracted from Comp Suite 1.0.0 so the hard-won pieces — ADAA saturation,
per-channel oversampling, the ratio ladder — live in one place instead of
being copied into every plugin.

## Modules

| Header | Contents |
|---|---|
| `hdsp/util.hpp` | `kPi`, `clampf` |
| `hdsp/db.hpp` | `dbToGain`, `gainToDb`, `timeCoeff` |
| `hdsp/smoother.hpp` | `Smoothed` one-pole parameter smoother |
| `hdsp/envelope.hpp` | `OnePoleLP`, `HighPass2`, `settleK` |
| `hdsp/biquad.hpp` | RBJ `Biquad` (shelves, peak, LP, HP) |
| `hdsp/adaa.hpp` | `Adaa`, `logCosh`, `AsymShaper` (ADAA saturator) |
| `hdsp/dynamics.hpp` | ratio ladder, soft-knee gain computer |
| `hdsp/sidechain.hpp` | `Topology`, `SidechainFrame` |
| `hdsp/truepeak.hpp` | 4× Catmull-Rom `TruePeakDetector` |
| `hdsp/oversample.hpp` | Off/2×/4× half-band `Oversampler` |

`hdsp/hdsp.hpp` includes all of them.

## Usage

```cmake
add_subdirectory(hearth-dsp)
target_link_libraries(my_plugin PRIVATE hdsp)
```

```cpp
#include <hdsp/hdsp.hpp>

hdsp::Smoothed gain;
gain.setTime(15.0f, 48000.0f);
gain.setTarget(hdsp::dbToGain(-6.0f));
```

Namespace is `hdsp`. The headers have no dependencies beyond the C++17
standard library and no global state.

## Oversampling

`hdsp::Oversampler` is a stateful 2×/4× converter. Call `prepare(1|2|4)`, feed
it blocks of at most `kMaxBaseFrames` base-rate frames with `up()`, run the
model at the returned higher rate, then call `down()`. It keeps separate filter
state per channel. Reported latency is 0 / 30 / 45 base-rate samples.

## Tests

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The tests cover DC unity, latency, left/right isolation (the regression that
caused a right-side buzz in Comp Suite), biquad responses, ADAA bounds, the
gain computer, smoothing and true-peak detection.

## Provenance

See `THIRD_PARTY_NOTICES.md`. The code was extracted from Comp Suite 1.0.0 and
keeps its GPL-3.0-or-later licence. `Comp Suite` itself lives at
<https://github.com/tylorismusic-commits/comp-suite>.
