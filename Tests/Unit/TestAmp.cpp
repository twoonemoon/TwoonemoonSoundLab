// SPDX-License-Identifier: GPL-3.0-or-later
#include "../TestHarness.h"
#include "../TestSignals.h"

#include "../../Source/Amps/Amp.h"
#include "../../Source/Amps/ToneStack.h"

using namespace tsl;
using namespace tsltest;

namespace {

ParamValues ampDefaults(int model)
{
  ParamValues p{};
  for (int i = 0; i < kNumParams; ++i) p[static_cast<size_t>(i)] = paramSpec(i).defaultValue;
  p[kAmpModel] = static_cast<float>(model);
  return p;
}

std::vector<float> runAmp(Amp& a, const std::vector<float>& in, const ParamValues& p, int block = 128,
                          double sr = 48000.0, std::vector<float>* rightOut = nullptr)
{
  a.prepare({ sr, 512 });
  a.update(p);
  std::vector<float> l = in, r(in.size(), 0.f);
  for (size_t pos = 0; pos < l.size(); pos += static_cast<size_t>(block))
  {
    const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(block), l.size() - pos));
    AudioBlock b;
    b.l = l.data() + pos;
    b.r = r.data() + pos;
    b.numFrames = n;
    a.process(b);
  }
  if (rightOut) *rightOut = r;
  return l;
}

} // namespace

TEST_CASE("tonestack: finite, mid-scoop and knob monotonicity")
{
  const double sr = 48000;
  const ToneStackComponents c{ 250e3, 1e6, 25e3, 56e3, 250e-12, 22e-9, 22e-9 };
  ToneStack ts;
  ts.design(c, 0.5, 0.5, 0.5, sr);
  const double lo = ts.magnitudeAt(100, sr), mid = ts.magnitudeAt(600, sr), hi = ts.magnitudeAt(4000, sr);
  CHECK(std::isfinite(lo) && std::isfinite(mid) && std::isfinite(hi));
  CHECK(mid < lo && mid < hi); // характерный «провал» середины
  ToneStack t2;
  t2.design(c, 0.5, 0.5, 1.0, sr);
  CHECK(t2.magnitudeAt(5000, sr) > hi * 1.2);
  ToneStack t3;
  t3.design(c, 1.0, 0.5, 0.5, sr);
  ToneStack t3b;
  t3b.design(c, 0.0, 0.5, 0.5, sr);
  CHECK(t3.magnitudeAt(80, sr) > t3b.magnitudeAt(80, sr) * 1.5);
  ToneStack t4;
  t4.design(c, 0.5, 1.0, 0.5, sr);
  CHECK(t4.magnitudeAt(600, sr) > mid * 1.2);
  // Импульсная характеристика затухает
  std::vector<float> imp(48000, 0.f);
  imp[0] = 1.f;
  for (auto& v : imp) v = ts.process(v);
  CHECK(allFinite(imp));
  CHECK(std::fabs(imp.back()) < 1e-6f);
}

TEST_CASE("amp: all models silent on silence, finite on hot input, stable under extremes")
{
  for (int m = 0; m < kNumAmpModels; ++m)
  {
    auto p = ampDefaults(m);
    Amp a;
    auto out = runAmp(a, std::vector<float>(24000, 0.f), p);
    CHECK_MSG(peak(out) < 1e-5f, "model %d silence %g", m, peak(out));

    // Максимальные настройки — не должно быть самовозбуждения
    p[kAmpGain] = 10.f; p[kAmpMaster] = 10.f; p[kAmpFeedback] = 10.f; p[kAmpPresence] = 10.f;
    p[kAmpDepth] = 10.f; p[kAmpSag] = 10.f; p[kAmpBias] = 10.f; p[kAmpBass] = 10.f; p[kAmpTreble] = 10.f;
    Amp a2;
    auto hot = noise(24000, 2.f);
    auto out2 = runAmp(a2, hot, p, 97);
    CHECK(allFinite(out2));
    CHECK_MSG(peak(out2) < 20.f, "model %d peak %g", m, peak(out2));
    // После прекращения сигнала выход затухает (нет автоколебаний)
    std::vector<float> tail = hot;
    std::fill(tail.begin() + 12000, tail.end(), 0.f);
    Amp a3;
    auto out3 = runAmp(a3, tail, p, 64);
    CHECK_MSG(rms(out3, 22000) < 1e-3f, "model %d tail %g", m, rms(out3, 22000));
  }
}

TEST_CASE("amp: gain increases harmonic distortion")
{
  for (int m : { 0, 4, 7 })
  {
    auto s = sine(220.f, dbToGain(-30.f), 48000.f, 48000);
    auto thd = [&](float gainKnob) {
      auto p = ampDefaults(m);
      p[kAmpGain] = gainKnob;
      Amp a;
      auto out = runAmp(a, s, p);
      const float h1 = toneAmplitude(out, 24000, 220.f, 48000.f);
      float hs = 0.f;
      for (int h = 2; h <= 6; ++h) hs += std::pow(toneAmplitude(out, 24000, 220.f * h, 48000.f), 2.f);
      return std::sqrt(hs) / std::max(h1, 1e-9f);
    };
    const float lo = thd(0.f), hi = thd(9.f);
    CHECK_MSG(hi > lo * 1.5f, "model %d THD lo %.4f hi %.4f", m, lo, hi);
  }
}

TEST_CASE("amp: clean models stay cleaner than high-gain at default settings")
{
  auto s = sine(196.f, dbToGain(-14.f), 48000.f, 48000);
  auto thd = [&](int m) {
    auto p = ampDefaults(m);
    Amp a;
    auto out = runAmp(a, s, p);
    const float h1 = toneAmplitude(out, 24000, 196.f, 48000.f);
    float hs = 0.f;
    for (int h = 2; h <= 7; ++h) hs += std::pow(toneAmplitude(out, 24000, 196.f * h, 48000.f), 2.f);
    return std::sqrt(hs) / std::max(h1, 1e-9f);
  };
  const float clean = thd(0), crunch = thd(4), heavy = thd(8);
  CHECK_MSG(clean < crunch && clean < heavy, "clean %.4f crunch %.4f heavy %.4f", clean, crunch, heavy);
  CHECK_MSG(clean < 0.15f, "clean THD %.4f", clean);
  CHECK_MSG(heavy > 0.3f, "heavy THD %.4f", heavy);
}

TEST_CASE("amp: bias changes power-stage crossover behaviour")
{
  // Холодное смещение → переходные искажения на малых уровнях на входе оконечника
  auto s = sine(330.f, dbToGain(-22.f), 48000.f, 48000);
  auto h3 = [&](float bias) {
    auto p = ampDefaults(0);
    p[kAmpBias] = bias;
    p[kAmpGain] = 3.f;
    p[kAmpMaster] = 6.f;
    Amp a;
    auto out = runAmp(a, s, p);
    return toneAmplitude(out, 24000, 990.f, 48000.f) / std::max(toneAmplitude(out, 24000, 330.f, 48000.f), 1e-9f);
  };
  const float cold = h3(0.f), hot = h3(10.f);
  CHECK_MSG(cold > hot * 1.3f, "cold %.5f hot %.5f", cold, hot);
}

TEST_CASE("amp: presence and depth act through the feedback loop")
{
  auto n = noise(48000, dbToGain(-24.f), 99);
  auto band = [&](float pres, float depth, float f) {
    auto p = ampDefaults(1);
    p[kAmpPresence] = pres;
    p[kAmpDepth] = depth;
    p[kAmpGain] = 2.f;
    Amp a;
    auto out = runAmp(a, n, p);
    return toneAmplitude(out, 12000, f, 48000.f);
  };
  // Шумовой сигнал → сравниваем усреднённые полосы через отношение
  float hiUp = 0, hiDn = 0, loUp = 0, loDn = 0;
  for (float f : { 5000.f, 6000.f, 7000.f }) { hiUp += band(10.f, 5.f, f); hiDn += band(0.f, 5.f, f); }
  for (float f : { 70.f, 90.f, 110.f }) { loUp += band(5.f, 10.f, f); loDn += band(5.f, 0.f, f); }
  CHECK_MSG(hiUp > hiDn * 1.1f, "presence hi %.5f vs %.5f", hiUp, hiDn);
  CHECK_MSG(loUp > loDn * 1.1f, "depth lo %.5f vs %.5f", loUp, loDn);
}

TEST_CASE("amp: model loudness is matched within a reasonable window")
{
  auto g = pluckedGuitar(48000.f, 4.f, -12.f);
  float minDb = 1e9f, maxDb = -1e9f;
  int minM = 0, maxM = 0;
  for (int m = 0; m < kNumAmpModels; ++m)
  {
    Amp a;
    auto out = runAmp(a, g, ampDefaults(m));
    const float l = db(rms(out, 4800));
    std::printf("    model %-24s rms %6.1f dBFS  peak %6.1f dBFS\n", ampVoice(m).name, l, db(peak(out, 4800)));
    if (l < minDb) { minDb = l; minM = m; }
    if (l > maxDb) { maxDb = l; maxM = m; }
  }
  CHECK_MSG(maxDb - minDb < 8.f, "spread %.1f dB (%s .. %s)", maxDb - minDb, ampVoice(minM).name, ampVoice(maxM).name);
  CHECK_MSG(maxDb < -3.f && minDb > -30.f, "range %.1f .. %.1f", minDb, maxDb);
}

TEST_CASE("amp: model and oversampling switch without blowups")
{
  Amp a;
  a.prepare({ 48000.0, 256 });
  auto p = ampDefaults(0);
  auto s = pluckedGuitar(48000.f, 2.f);
  std::vector<float> r(256);
  float maxAbs = 0.f;
  for (size_t pos = 0, k = 0; pos + 256 <= s.size(); pos += 256, ++k)
  {
    if (k % 20 == 0)
    {
      p[kAmpModel] = static_cast<float>((k / 20) % kNumAmpModels);
      p[kOversampling] = static_cast<float>((k / 20) % 3);
    }
    a.update(p);
    AudioBlock b;
    b.l = s.data() + pos;
    b.r = r.data();
    b.numFrames = 256;
    a.process(b);
    for (int i = 0; i < 256; ++i) maxAbs = std::max(maxAbs, std::fabs(s[pos + i]));
  }
  CHECK(allFinite(s));
  CHECK(maxAbs < 8.f);
}

TEST_CASE("amp: acoustic room produces stereo, tame reduces piezo region")
{
  auto p = ampDefaults(11);
  p[kAcRoom] = 8.f;
  Amp a;
  std::vector<float> right;
  auto g = pluckedGuitar(48000.f, 2.f);
  auto l = runAmp(a, g, p, 128, 48000.0, &right);
  CHECK(allFinite(l) && allFinite(right));
  double diff = 0;
  for (size_t i = 4800; i < l.size(); ++i) diff += std::fabs(l[i] - right[i]);
  CHECK(diff > 1.0);

  auto n = noise(48000, dbToGain(-20.f));
  auto piezoBand = [&](float tame) {
    auto pp = ampDefaults(10);
    pp[kAcPiezoTame] = tame;
    pp[kAcRoom] = 0.f;
    Amp aa;
    auto out = runAmp(aa, n, pp);
    return toneAmplitude(out, 12000, 1500.f, 48000.f) + toneAmplitude(out, 12000, 1400.f, 48000.f);
  };
  CHECK(piezoBand(10.f) < piezoBand(0.f) * 0.7f);
}

TEST_CASE("amp: sample rates 44.1/48/96 kHz")
{
  for (double sr : { 44100.0, 48000.0, 96000.0 })
  {
    auto g = pluckedGuitar(static_cast<float>(sr), 1.f);
    Amp a;
    auto out = runAmp(a, g, ampDefaults(6), 64, sr);
    CHECK(allFinite(out));
    CHECK(rms(out, 2000) > 1e-3f);
  }
}
