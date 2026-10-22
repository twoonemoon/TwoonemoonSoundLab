// SPDX-License-Identifier: GPL-3.0-or-later
#include "../TestHarness.h"
#include "../TestSignals.h"

#include "../../Source/Audio/Params.h"
#include "../../Source/DSP/Biquad.h"
#include "../../Source/DSP/DelayLine.h"
#include "../../Source/DSP/Meter.h"
#include "../../Source/DSP/Oversampler.h"
#include "../../Source/DSP/Smoother.h"
#include "../../Source/DSP/SpscRing.h"
#include "../../Source/DSP/Svf.h"

#include <set>
#include <string>

using namespace tsl;
using namespace tsltest;

TEST_CASE("params: keys unique and specs valid")
{
  std::set<std::string> keys;
  for (int i = 0; i < kNumParams; ++i)
  {
    const auto& s = paramSpec(i);
    CHECK_MSG(keys.insert(s.key).second, "duplicate key %s", s.key);
    CHECK_MSG(s.minValue < s.maxValue, "range %s", s.key);
    CHECK_MSG(s.defaultValue >= s.minValue && s.defaultValue <= s.maxValue, "default %s", s.key);
    if (s.kind == ParamKind::Enum) CHECK_MSG(s.numLabels == static_cast<int>(s.maxValue - s.minValue) + 1, "labels %s", s.key);
    if (s.shape == ParamShape::Exp) CHECK_MSG(s.minValue > 0.f, "exp needs min>0 %s", s.key);
    CHECK(paramIdForKey(s.key) == i);
  }
}

TEST_CASE("params: normalization round trip")
{
  for (int i = 0; i < kNumParams; ++i)
  {
    const auto& s = paramSpec(i);
    for (float n : { 0.f, 0.1f, 0.37f, 0.5f, 0.9f, 1.f })
    {
      const float v = paramFromNormalized(i, n);
      CHECK(v >= s.minValue - 1e-4f && v <= s.maxValue + 1e-4f);
      if (s.kind == ParamKind::Float) CHECK_NEAR(paramToNormalized(i, v), n, 1e-3);
    }
    CHECK(paramClamp(i, NAN) == s.defaultValue);
    CHECK(paramClamp(i, 1e9f) == s.maxValue);
  }
}

TEST_CASE("biquad: lowpass response and stability")
{
  const float sr = 48000.f;
  Biquad bq;
  bq.setCoefs(design::make(FilterType::LowPass, sr, 1000.0, 0.7071));
  CHECK_NEAR(bq.coefs().magnitudeAt(1000.0, sr), 0.7071, 0.01);
  CHECK(bq.coefs().magnitudeAt(100.0, sr) > 0.99);
  CHECK(bq.coefs().magnitudeAt(10000.0, sr) < 0.02);
  auto s = sine(100.f, 0.5f, sr, 9600);
  bq.processBlock(s.data(), 9600);
  CHECK_NEAR(toneAmplitude(s, 4800, 100.f, sr), 0.5, 0.01);
  auto imp = impulse(48000);
  bq.reset();
  bq.processBlock(imp.data(), 48000);
  CHECK(allFinite(imp));
  CHECK(std::fabs(imp.back()) < 1e-9f);
}

TEST_CASE("biquad: shelves and peak gains")
{
  const double sr = 48000;
  CHECK_NEAR(design::make(FilterType::Peak, sr, 1000, 1.0, 6.0).magnitudeAt(1000, sr), std::pow(10, 6.0 / 20), 0.01);
  CHECK_NEAR(design::make(FilterType::LowShelf, sr, 200, 0.7, -9.0).magnitudeAt(20, sr), std::pow(10, -9.0 / 20), 0.02);
  CHECK_NEAR(design::make(FilterType::HighShelf, sr, 4000, 0.7, 4.0).magnitudeAt(20000, sr), std::pow(10, 4.0 / 20), 0.03);
  CHECK_NEAR(design::make(FilterType::HighPass1, sr, 100, 0.7).magnitudeAt(100, sr), 0.7071, 0.01);
}

TEST_CASE("svf: stable under fast modulation")
{
  Svf f;
  auto n = noise(48000, 0.5f);
  float maxAbs = 0;
  for (int i = 0; i < 48000; ++i)
  {
    const float fc = 200.f + 3000.f * (0.5f + 0.5f * std::sin(i * 0.05f));
    f.set(fc, 8.f, 48000.f);
    const float y = f.process(n[static_cast<size_t>(i)]).bp;
    maxAbs = std::max(maxAbs, std::fabs(y));
  }
  CHECK(std::isfinite(maxAbs));
  CHECK(maxAbs < 20.f);
}

TEST_CASE("smoothers: linear reaches target exactly")
{
  LinearSmoother s;
  s.prepare(48000.f, 0.01f);
  s.snap(0.f);
  s.setTarget(1.f);
  float v = 0;
  for (int i = 0; i < 480; ++i) v = s.next();
  CHECK(v == 1.f);
  CHECK(!s.isSmoothing());
  s.setTarget(0.5f);
  CHECK(s.skip(10000) == 0.5f);
}

TEST_CASE("halfband: attenuation estimate for stage designs")
{
  CHECK(halfband::estimateAttenuationDb(10, 0.04) > 90.0);
  CHECK(halfband::estimateAttenuationDb(6, 0.12) > 90.0);
  CHECK(halfband::estimateAttenuationDb(4, 0.18) > 70.0);
  auto c = halfband::designCoefs(10, 0.04);
  for (double v : c) CHECK(v > 0.0 && v < 1.0);
}

TEST_CASE("oversampler: passband flat, images rejected, block sizes")
{
  const float sr = 48000.f;
  for (int factor : { 2, 4, 8 })
  {
    Oversampler os;
    os.prepare(256, 8);
    os.setFactor(factor);
    CHECK(os.factor() == factor);
    // Тон 1 кГц: апсемплинг → даунсемплинг ≈ без изменения амплитуды
    auto x = sine(1000.f, 0.5f, sr, 24000);
    std::vector<float> y(x.size());
    for (size_t p = 0; p < x.size(); p += 200)
    {
      const int n = static_cast<int>(std::min<size_t>(200, x.size() - p));
      os.upsample(x.data() + p, n);
      os.downsample(y.data() + p, n);
    }
    CHECK_NEAR(toneAmplitude(y, 4800, 1000.f, sr), 0.5, 0.005);
    // Тон 15 кГц тоже в полосе пропускания
    os.reset();
    auto x2 = sine(15000.f, 0.5f, sr, 24000);
    for (size_t p = 0; p < x2.size(); p += 128)
    {
      const int n = static_cast<int>(std::min<size_t>(128, x2.size() - p));
      os.upsample(x2.data() + p, n);
      os.downsample(y.data() + p, n);
    }
    CHECK_NEAR(toneAmplitude(y, 4800, 15000.f, sr), 0.5, 0.01);
    CHECK(os.groupDelaySamples() < 8.f);
  }
}

TEST_CASE("oversampler: image suppression after upsampling")
{
  // Апсемплинг 2x тона 5 кГц: зеркальная компонента (48k-5k=43 кГц при 96 кГц) должна быть подавлена.
  Oversampler os;
  os.prepare(4096, 2);
  os.setFactor(2);
  auto x = sine(5000.f, 1.f, 48000.f, 4096);
  float* up = os.upsample(x.data(), 4096);
  std::vector<float> u(up, up + 8192);
  const float main = toneAmplitude(u, 2048, 5000.f, 96000.f);
  const float image = toneAmplitude(u, 2048, 43000.f, 96000.f);
  CHECK_NEAR(main, 1.0, 0.01);
  CHECK_MSG(db(image / main) < -80.f, "image %.1f dB", db(image / main));
}

TEST_CASE("delay line: integer and fractional reads")
{
  DelayLine d;
  d.prepare(100);
  for (int i = 0; i < 50; ++i) d.push(static_cast<float>(i));
  CHECK(d.readInt(1) == 49.f);
  CHECK(d.readInt(10) == 40.f);
  CHECK_NEAR(d.readLinear(10.5f), 39.5, 1e-5);
  CHECK_NEAR(d.readCubic(10.5f), 39.5, 1e-4);
}

TEST_CASE("meter: peak and rms of sine")
{
  LevelMeter m;
  m.prepare(48000.f);
  auto s = sine(1000.f, 0.5f, 48000.f, 48000);
  m.process(s.data(), 48000);
  CHECK_NEAR(m.peak(), 0.5, 0.01);
  CHECK_NEAR(m.rms(), 0.5 / std::sqrt(2.0), 0.01);
  CHECK(!m.clipped());
  std::vector<float> loud(64, 1.2f);
  m.process(loud.data(), 64);
  CHECK(m.clipped());
}

TEST_CASE("spsc ring: push/pop and overflow")
{
  SpscRing<float> r;
  r.prepare(8);
  float a[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
  CHECK(r.push(a, 10) == 8);
  float b[10] = {};
  CHECK(r.pop(b, 3) == 3);
  CHECK(b[0] == 1.f && b[2] == 3.f);
  r.discardOldest(2);
  CHECK(r.available() == 2);
  CHECK(r.pop(b, 10) == 2);
  CHECK(b[0] == 7.f && b[1] == 8.f);
}
