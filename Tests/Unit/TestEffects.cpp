// SPDX-License-Identifier: GPL-3.0-or-later
#include "../TestHarness.h"
#include "../TestSignals.h"

#include "../../Source/Audio/Engine.h"
#include "../../Source/Effects/Compressor.h"
#include "../../Source/Effects/Drives.h"
#include "../../Source/Effects/NoiseGate.h"

using namespace tsl;
using namespace tsltest;

namespace {

ParamValues defaults()
{
  ParamValues p{};
  for (int i = 0; i < kNumParams; ++i) p[static_cast<size_t>(i)] = paramSpec(i).defaultValue;
  return p;
}

/** Прогон моно-сигнала через модуль блоками заданного размера. */
std::vector<float> runModule(Module& m, const std::vector<float>& in, int block, const ParamValues& p,
                             double sr = 48000.0)
{
  m.prepare({ sr, std::max(block, 64) });
  m.update(p);
  std::vector<float> out = in, r(in.size(), 0.f);
  TransportInfo t;
  for (size_t pos = 0; pos < out.size(); pos += static_cast<size_t>(block))
  {
    const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(block), out.size() - pos));
    AudioBlock b;
    b.l = out.data() + pos;
    b.r = r.data() + pos;
    b.numFrames = n;
    b.keyInput = in.data() + pos;
    b.transport = &t;
    m.update(p);
    m.process(b);
  }
  return out;
}

} // namespace

TEST_CASE("gate: closes on low noise, opens on loud signal")
{
  NoiseGate g;
  auto p = defaults();
  p[kGateThreshold] = -50.f;
  p[kGateRange] = -96.f;
  auto quiet = noise(48000, dbToGain(-70.f));
  auto out = runModule(g, quiet, 128, p);
  CHECK_MSG(rms(out, 24000) < dbToGain(-120.f), "residual %.1f dB", db(rms(out, 24000)));

  NoiseGate g2;
  auto loud = sine(220.f, 0.3f, 48000.f, 48000);
  auto out2 = runModule(g2, loud, 128, p);
  CHECK_NEAR(rms(out2, 4800), rms(loud, 4800), 1e-3);
}

TEST_CASE("gate: hysteresis prevents chatter on decaying note")
{
  NoiseGate g;
  auto p = defaults();
  p[kGateThreshold] = -40.f;
  p[kGateHysteresis] = 10.f;
  p[kGateHold] = 20.f;
  p[kGateRelease] = 50.f;
  g.prepare({ 48000.0, 256 });
  g.update(p);
  // Затухающая нота: от -10 дБ до -60 дБ за 2 с
  const int n = 96000;
  std::vector<float> x(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i)
  {
    const float envDb = -10.f - 50.f * static_cast<float>(i) / n;
    x[static_cast<size_t>(i)] = dbToGain(envDb) * std::sin(kTwoPi * 110.f * i / 48000.f);
  }
  int transitions = 0;
  bool wasOpen = true;
  std::vector<float> r(256);
  for (int pos = 0; pos < n; pos += 256)
  {
    AudioBlock b;
    b.l = x.data() + pos;
    b.r = r.data();
    b.numFrames = 256;
    g.process(b);
    const bool open = g.isOpen();
    if (open != wasOpen) ++transitions;
    wasOpen = open;
  }
  CHECK_MSG(transitions <= 1, "transitions=%d", transitions);
}

TEST_CASE("gate: range limits attenuation")
{
  NoiseGate g;
  auto p = defaults();
  p[kGateThreshold] = -30.f;
  p[kGateRange] = -20.f;
  auto quiet = sine(500.f, dbToGain(-50.f), 48000.f, 48000);
  auto out = runModule(g, quiet, 64, p);
  CHECK_NEAR(db(rms(out, 24000) / rms(quiet, 24000)), -20.0, 0.5);
}

TEST_CASE("compressor: static curve and gain reduction")
{
  CHECK_NEAR(Compressor::staticCurve(-40.f, -20.f, 4.f, 0.f), -40.0, 1e-5);
  CHECK_NEAR(Compressor::staticCurve(0.f, -20.f, 4.f, 0.f), -15.0, 1e-5);
  CHECK_NEAR(Compressor::staticCurve(-20.f, -20.f, 4.f, 6.f), -20.0 - 0.75 * 9.0 / 12.0, 1e-4);

  Compressor c;
  auto p = defaults();
  p[kCompThreshold] = -20.f;
  p[kCompRatio] = 4.f;
  p[kCompKnee] = 0.f;
  p[kCompAttack] = 1.f;
  p[kCompRelease] = 50.f;
  auto s = sine(1000.f, dbToGain(-6.f), 48000.f, 48000);
  auto out = runModule(c, s, 256, p);
  // Пиковый детектор: пик -6 дБ → выше порога на 14 дБ → ослабление ≈ 10.5 дБ
  const float gr = db(rms(out, 24000) / rms(s, 24000));
  CHECK_MSG(gr < -8.f && gr > -12.f, "gr=%.2f", gr);
  CHECK(c.gainReductionDb() < -8.f);
}

TEST_CASE("compressor: mix 0 is transparent")
{
  Compressor c;
  auto p = defaults();
  p[kCompMix] = 0.f;
  p[kCompThreshold] = -40.f;
  auto s = sine(300.f, 0.5f, 48000.f, 24000);
  auto out = runModule(c, s, 100, p);
  CHECK_NEAR(rms(out, 4800), rms(s, 4800), 1e-4);
}

TEST_CASE("drives: silence in → silence out, finite under extreme input")
{
  auto p = defaults();
  p[kOdDrive] = 10.f; p[kDistDrive] = 10.f; p[kFuzzFuzz] = 10.f; p[kBoostLevel] = 20.f;
  std::unique_ptr<Module> mods[] = { std::make_unique<Boost>(), std::make_unique<Overdrive>(),
                                     std::make_unique<Distortion>(), std::make_unique<Fuzz>() };
  for (auto& m : mods)
  {
    auto silence = std::vector<float>(24000, 0.f);
    auto out = runModule(*m, silence, 97, p);
    CHECK_MSG(peak(out) < 1e-6f, "module %d silence peak %g", static_cast<int>(m->id()), peak(out));

    auto hot = noise(24000, 8.f); // +18 дБFS шум
    auto out2 = runModule(*m, hot, 333, p);
    CHECK(allFinite(out2));
    CHECK_MSG(peak(out2) < 16.f, "module %d peak %g", static_cast<int>(m->id()), peak(out2));

    std::vector<float> bad(4096, 0.f);
    bad[100] = 1e30f;
    auto out3 = runModule(*m, bad, 64, p);
    CHECK(allFinite(out3));
  }
}

TEST_CASE("drives: overdrive adds harmonics, level knob works")
{
  auto p = defaults();
  p[kOdDrive] = 8.f;
  Overdrive od;
  auto s = sine(220.f, 0.3f, 48000.f, 48000);
  auto out = runModule(od, s, 128, p);
  const float h1 = toneAmplitude(out, 9600, 220.f, 48000.f);
  const float h3 = toneAmplitude(out, 9600, 660.f, 48000.f);
  CHECK(h1 > 0.05f);
  CHECK_MSG(h3 / h1 > 0.02f, "h3/h1 = %.4f", h3 / h1);

  Overdrive od2;
  p[kOdLevel] = -12.f;
  auto out2 = runModule(od2, s, 128, p);
  CHECK_NEAR(db(rms(out2, 9600) / rms(out, 9600)), -12.0, 0.3);
}

TEST_CASE("drives: fuzz octave mode produces 2nd harmonic")
{
  auto p = defaults();
  p[kFuzzMode] = 2.f;
  p[kFuzzFuzz] = 2.f;
  p[kFuzzTone] = 10.f;
  Fuzz f;
  auto s = sine(200.f, 0.2f, 48000.f, 48000);
  auto out = runModule(f, s, 256, p);
  const float h1 = toneAmplitude(out, 9600, 200.f, 48000.f);
  const float h2 = toneAmplitude(out, 9600, 400.f, 48000.f);
  CHECK_MSG(h2 > h1 * 0.5f, "h1=%.4f h2=%.4f", h1, h2);
}

// ---------------------------------------------------------------------------
// Engine

namespace {

void runEngine(Engine& e, std::vector<float>& l, std::vector<float>& r, int nIn, int block)
{
  TransportInfo t;
  for (size_t pos = 0; pos < l.size(); pos += static_cast<size_t>(block))
  {
    const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(block), l.size() - pos));
    const float* in[2] = { l.data() + pos, r.data() + pos };
    float* out[2] = { l.data() + pos, r.data() + pos };
    e.process(in, nIn, out, 2, n, t);
  }
}

void allModulesOff(Engine& e)
{
  for (int i = 0; i < kNumModules; ++i) e.setParam(moduleEnableParam(static_cast<ModuleId>(i)), 0.f);
}

} // namespace

TEST_CASE("engine: unity passthrough with modules off, mono→stereo")
{
  Engine e;
  e.prepare(48000.0, 256);
  allModulesOff(e);
  auto l = sine(440.f, 0.25f, 48000.f, 24000);
  std::vector<float> r(l.size(), 0.f);
  const auto ref = l;
  runEngine(e, l, r, 1, 256);
  CHECK_NEAR(rms(l, 4800), rms(ref, 4800), 1e-5);
  CHECK_NEAR(rms(r, 4800), rms(ref, 4800), 1e-5);
}

TEST_CASE("engine: input/output trim and bypass")
{
  Engine e;
  e.prepare(48000.0, 128);
  allModulesOff(e);
  e.setParam(kInputTrim, 6.f);
  e.setParam(kOutputTrim, -12.f);
  auto l = sine(440.f, 0.25f, 48000.f, 24000);
  std::vector<float> r(l.size(), 0.f);
  const auto ref = l;
  runEngine(e, l, r, 1, 128);
  CHECK_NEAR(db(rms(l, 9600) / rms(ref, 9600)), -6.0, 0.05);

  Engine e2;
  e2.prepare(48000.0, 128);
  e2.setParam(kDistOn, 1.f);
  e2.setParam(kBypass, 1.f);
  auto l2 = ref;
  std::vector<float> r2(l2.size(), 0.f);
  runEngine(e2, l2, r2, 1, 128);
  CHECK_NEAR(rms(l2, 4800), rms(ref, 4800), 1e-5);
}

TEST_CASE("engine: arbitrary block sizes, NaN input, finite output")
{
  Engine e;
  e.prepare(44100.0, 64);
  e.setParam(kDistOn, 1.f);
  e.setParam(kCompOn, 1.f);
  auto l = noise(44100, 0.5f);
  l[1000] = NAN;
  l[2000] = INFINITY;
  std::vector<float> r(l.size(), 0.f);
  TransportInfo t;
  size_t pos = 0;
  int sizes[] = { 1, 7, 64, 65, 300, 1024, 13 };
  int k = 0;
  while (pos < l.size())
  {
    const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(sizes[k++ % 7]), l.size() - pos));
    const float* in[2] = { l.data() + pos, r.data() + pos };
    float* out[2] = { l.data() + pos, r.data() + pos };
    e.process(in, 1, out, 2, n, t);
    pos += static_cast<size_t>(n);
  }
  CHECK(allFinite(l));
  CHECK(allFinite(r));
  CHECK(peak(l) <= 4.0f);
}

TEST_CASE("engine: chain reorder is applied with fade and stays finite")
{
  Engine e;
  e.prepare(48000.0, 256);
  e.setParam(kOdOn, 1.f);
  e.setParam(kFuzzOn, 1.f);
  auto order = defaultChainOrder();
  std::swap(order[3], order[5]);
  e.setChainOrder(order);
  auto l = sine(110.f, 0.3f, 48000.f, 48000);
  std::vector<float> r(l.size(), 0.f);
  runEngine(e, l, r, 1, 256);
  CHECK(allFinite(l));
  CHECK(e.chainOrder() == order);
  CHECK(rms(l, 24000) > 0.01f);
  ChainOrder bad = defaultChainOrder();
  bad[0] = bad[1];
  e.setChainOrder(bad); // невалидный порядок игнорируется
  CHECK(e.chainOrder() == order);
}

TEST_CASE("engine: module toggle crossfade has no discontinuity spikes")
{
  Engine e;
  e.prepare(48000.0, 64);
  allModulesOff(e);
  auto l = sine(100.f, 0.2f, 48000.f, 9600);
  std::vector<float> r(l.size(), 0.f);
  TransportInfo t;
  float maxStep = 0.f, prev = 0.f;
  for (size_t pos = 0; pos < l.size(); pos += 64)
  {
    if (pos == 4800) e.setParam(kBoostOn, 1.f);
    const float* in[2] = { l.data() + pos, r.data() + pos };
    float* out[2] = { l.data() + pos, r.data() + pos };
    e.process(in, 1, out, 2, 64, t);
    for (int i = 0; i < 64; ++i)
    {
      maxStep = std::max(maxStep, std::fabs(l[pos + i] - prev));
      prev = l[pos + i];
    }
  }
  // Синус 100 Гц 0.2 → макс. шаг ≈ 0.0026; с бустом +6 дБ ≈ 0.0053. Щелчок дал бы скачок ≫ 0.05.
  CHECK_MSG(maxStep < 0.02f, "maxStep=%g", maxStep);
}
