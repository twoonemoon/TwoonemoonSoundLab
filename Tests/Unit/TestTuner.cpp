// SPDX-License-Identifier: GPL-3.0-or-later
#include "../TestHarness.h"
#include "../TestSignals.h"

#include "../../Source/DSP/PitchDetector.h"

using namespace tsl;
using namespace tsltest;

namespace {

float detect(const std::vector<float>& x, double sr)
{
  PitchDetector d;
  d.prepare(sr);
  d.push(x.data(), static_cast<int>(x.size()));
  return d.estimate().frequency;
}

float centsOff(float f, float ref) { return 1200.f * std::log2(f / ref); }

} // namespace

TEST_CASE("tuner: sine accuracy within 1 cent across guitar range")
{
  for (double sr : { 44100.0, 48000.0, 96000.0 })
    for (float f : { 41.2f, 61.74f, 82.41f, 110.f, 146.83f, 196.f, 246.94f, 329.63f, 440.f, 987.77f })
    {
      auto s = sine(f, 0.3f, static_cast<float>(sr), static_cast<int>(sr * 0.3));
      const float est = detect(s, sr);
      CHECK_MSG(std::fabs(centsOff(est, f)) < 1.f, "sr %.0f f %.2f est %.3f (%.2f ct)", sr, f, est, centsOff(est, f));
    }
}

TEST_CASE("tuner: harmonic-rich and plucked signals, no octave errors")
{
  // Пила (сильные гармоники)
  const float sr = 48000.f;
  for (float f : { 82.41f, 110.f, 196.f })
  {
    std::vector<float> saw(static_cast<size_t>(sr * 0.3));
    for (size_t i = 0; i < saw.size(); ++i)
    {
      float v = 0.f;
      for (int h = 1; h <= 12; ++h) v += std::sin(kTwoPi * f * h * i / sr) / h;
      saw[i] = 0.2f * v;
    }
    CHECK_MSG(std::fabs(centsOff(detect(saw, sr), f)) < 2.f, "saw %.1f -> %.2f", f, detect(saw, sr));
  }
  // Щипок Karplus-Strong: период по задержке
  auto g = pluckedGuitar(sr, 1.f, -12.f, 5); // аккорд — пропускаем; ниже одиночная нота
  (void)g;
  const float note = 110.f;
  const int period = static_cast<int>(sr / note);
  std::vector<float> buf(static_cast<size_t>(period));
  FastRandom rnd(3);
  for (auto& v : buf) v = rnd.nextBipolar() * 0.5f;
  std::vector<float> out(static_cast<size_t>(sr * 0.4));
  int idx = 0;
  for (auto& o : out)
  {
    const int nx = (idx + 1) % period;
    const float y = buf[static_cast<size_t>(idx)];
    buf[static_cast<size_t>(idx)] = 0.499f * (y + buf[static_cast<size_t>(nx)]);
    o = y;
    idx = nx;
  }
  const float est = detect(out, sr);
  const float expected = sr / (period - 0.5f); // x[n+P] = ½(x[n] + x[n+1]) → эффективный период P − ½
  CHECK_MSG(std::fabs(centsOff(est, expected)) < 3.f, "pluck est %.2f exp %.2f", est, expected);
}

TEST_CASE("tuner: silence and noise give no confident pitch")
{
  PitchDetector d;
  d.prepare(48000.0);
  std::vector<float> z(16000, 0.f);
  d.push(z.data(), 16000);
  CHECK(d.estimate().frequency == 0.f);
  auto n = noise(16000, 0.2f);
  d.push(n.data(), 16000);
  const auto r = d.estimate();
  CHECK(r.frequency == 0.f || r.clarity < 0.8f);
}

TEST_CASE("tuner: note naming and cents")
{
  auto n = PitchDetector::noteFor(440.f);
  CHECK(n.midi == 69 && std::string(n.name) == "A" && n.octave == 4);
  CHECK_NEAR(n.cents, 0.0, 1e-3);
  auto e = PitchDetector::noteFor(82.41f * std::pow(2.f, 10.f / 1200.f));
  CHECK(std::string(e.name) == "E" && e.octave == 2);
  CHECK_NEAR(e.cents, 10.0, 0.2);
  auto r = PitchDetector::noteFor(432.f, 432.f);
  CHECK(r.midi == 69);
}
