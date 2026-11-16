// hearth-dsp — module tests
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <cstdio>
#include <vector>

#include <hdsp/hdsp.hpp>

namespace {

int gFailures = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) {
    ++gFailures;
  }
}

constexpr float kSr = 48000.0f;

} // namespace

int main() {
  using namespace hdsp;

  // db / util
  check(std::fabs(dbToGain(0.0f) - 1.0f) < 1.0e-6f, "0 dB is unity");
  check(std::fabs(gainToDb(dbToGain(6.0f)) - 6.0f) < 1.0e-4f,
        "dB round-trips");
  check(clampf(5.0f, 0.0f, 1.0f) == 1.0f, "clamp high");
  check(clampf(-1.0f, 0.0f, 1.0f) == 0.0f, "clamp low");
  check(timeCoeff(10.0f, kSr) > 0.0f && timeCoeff(10.0f, kSr) < 1.0f,
        "time coefficient in range");

  // Smoothed
  {
    Smoothed s;
    s.setTime(1.0f, kSr);
    s.reset(0.0f);
    s.setTarget(1.0f);
    for (int i = 0; i < 20000; ++i) {
      s.next();
    }
    check(std::fabs(s.current() - 1.0f) < 0.001f, "smoothed converges");
  }

  // settleK
  check(settleK(0.0, kSr) == 1.0f, "settleK(0) is instantaneous");
  {
    const float k = settleK(100.0, kSr);
    check(k > 0.0f && k < 1.0f, "settleK is a fraction");
  }

  // Biquad responses
  {
    Biquad lp;
    lp.lowPass(1000.0f, 0.7071f, kSr);
    float y = 0.0f;
    for (int i = 0; i < 4000; ++i) y = lp.process(1.0f);
    check(std::fabs(y - 1.0f) < 0.01f, "biquad low-pass passes DC");

    Biquad hp;
    hp.highPass(1000.0f, 0.7071f, kSr);
    for (int i = 0; i < 4000; ++i) y = hp.process(1.0f);
    check(std::fabs(y) < 0.01f, "biquad high-pass rejects DC");
  }

  // One-pole envelope
  {
    OnePoleLP lp;
    lp.set(200.0f, kSr);
    float y = 0.0f;
    for (int i = 0; i < 4000; ++i) y = lp.process(1.0f);
    check(std::fabs(y - 1.0f) < 0.01f, "one-pole low-pass settles to DC");

    HighPass2 hp;
    hp.set(100.0f, kSr);
    for (int i = 0; i < 8000; ++i) y = hp.process(1.0f);
    check(std::fabs(y) < 0.01f, "12 dB high-pass rejects DC");
  }

  // Ratio ladder and gain computer
  {
    check(ratioFromStep(0) == 1.5f && ratioFromStep(3) == 4.0f &&
              ratioFromStep(8) == 20.0f,
          "ratio ladder values");
    check(ratioStepFrom(4.6f) == 5, "ratio step rounds");

    const float below = gainReductionDb(-30.0f, -20.0f, 4.0f, 0.0f);
    const float above = gainReductionDb(-10.0f, -20.0f, 4.0f, 0.0f);
    check(below == 0.0f, "gain computer is 0 below threshold");
    check(std::fabs(above - 7.5f) < 0.01f,
          "4:1 gives 0.75 dB per dB over threshold");
    const float knee = gainReductionDb(-20.0f, -20.0f, 4.0f, 6.0f);
    check(knee > 0.0f && knee < 1.0f, "soft knee is gentle at threshold");
  }

  // ADAA shaper
  {
    AsymShaper shaper;
    shaper.reset();
    bool finite = true;
    float peak = 0.0f;
    for (int i = 0; i < 2000; ++i) {
      const float x = 0.8f * std::sin(2.0f * kPi * 100.0f * i / kSr);
      const float y = shaper.process(x, 1.6f, 0.1f);
      finite = finite && std::isfinite(y);
      peak = std::max(peak, std::fabs(y));
    }
    check(finite && peak > 0.1f && peak < 3.0f, "ADAA shaper is bounded");
    AsymShaper bypass;
    check(std::fabs(bypass.process(0.5f, 0.0f, 0.0f) - 0.5f) < 1.0e-6f,
          "ADAA shaper with zero drive is a bypass");
  }

  // True-peak detector
  {
    TruePeakDetector tp;
    tp.reset();
    float peak = 0.0f;
    for (int i = 0; i < 2000; ++i) {
      const float x = 0.5f * std::sin(2.0f * kPi * 997.0f * i / kSr);
      peak = std::max(peak, tp.process(x));
    }
    check(peak >= 0.49f && peak < 2.0f, "true-peak detector sees the signal");
  }

  // Oversampler: unity, latency, channel isolation
  for (int factor : {1, 2, 4}) {
    Oversampler os;
    os.prepare(factor);
    const int expect = factor == 1 ? 0 : (factor == 2 ? 30 : 45);
    char label[96];
    std::snprintf(label, sizeof(label), "oversampler %dx reports %d samples",
                  factor, expect);
    check(os.latencySamples() == expect, label);

    const int n = 512;
    std::vector<float> inL(n, 0.0f), inR(n, 0.0f);
    std::vector<float> upL(n * factor), upR(n * factor), outL(n), outR(n);

    // DC gain after settling.
    for (int b = 0; b < 12; ++b) {
      for (int i = 0; i < n; ++i) {
        inL[i] = inR[i] = 1.0f;
      }
      os.up(inL.data(), inR.data(), n, upL.data(), upR.data());
      os.down(upL.data(), upR.data(), n, outL.data(), outR.data());
    }
    std::snprintf(label, sizeof(label), "oversampler %dx passes DC", factor);
    check(std::fabs(outL[n - 1] - 1.0f) < 0.01f, label);

    // Left-only input must leave the right channel silent.
    os.reset();
    for (int b = 0; b < 8; ++b) {
      for (int i = 0; i < n; ++i) {
        inL[i] = 0.5f * std::sin(2.0f * kPi * 1000.0f * (b * n + i) / kSr);
        inR[i] = 0.0f;
      }
      os.up(inL.data(), inR.data(), n, upL.data(), upR.data());
      os.down(upL.data(), upR.data(), n, outL.data(), outR.data());
    }
    double right = 0.0;
    for (float v : outR) {
      right += static_cast<double>(v) * v;
    }
    std::snprintf(label, sizeof(label),
                  "oversampler %dx keeps L/R separate", factor);
    check(right < 1.0e-12, label);

    // Impulse latency.
    os.reset();
    std::fill(inL.begin(), inL.end(), 0.0f);
    std::fill(inR.begin(), inR.end(), 0.0f);
    inL[100] = inR[100] = 1.0f;
    os.up(inL.data(), inR.data(), n, upL.data(), upR.data());
    os.down(upL.data(), upR.data(), n, outL.data(), outR.data());
    int arg = 0;
    float peak = 0.0f;
    for (int i = 0; i < n; ++i) {
      if (std::fabs(outL[i]) > peak) {
        peak = std::fabs(outL[i]);
        arg = i;
      }
    }
    std::snprintf(label, sizeof(label), "oversampler %dx delay is %d", factor,
                  arg - 100);
    check(arg - 100 == expect, label);
  }

  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
