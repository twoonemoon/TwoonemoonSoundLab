// SPDX-License-Identifier: GPL-3.0-or-later
#include "../TestHarness.h"
#include "../TestSignals.h"

#include "../../Source/Effects/Delay.h"
#include "../../Source/Effects/Eq.h"
#include "../../Source/Effects/Modulation.h"
#include "../../Source/Effects/Reverb.h"

#include <memory>

using namespace tsl;
using namespace tsltest;

namespace {

ParamValues defaults()
{
  ParamValues p{};
  for (int i = 0; i < kNumParams; ++i) p[static_cast<size_t>(i)] = paramSpec(i).defaultValue;
  return p;
}

struct Stereo
{
  std::vector<float> l, r;
  bool stereo = false;
};

Stereo run(Module& m, const std::vector<float>& in, const ParamValues& p, int block = 128, double sr = 48000.0,
           TransportInfo t = {})
{
  m.prepare({ sr, 512 });
  m.update(p);
  Stereo s{ in, std::vector<float>(in.size(), 0.f) };
  for (size_t pos = 0; pos < in.size(); pos += static_cast<size_t>(block))
  {
    const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(block), in.size() - pos));
    AudioBlock b;
    b.l = s.l.data() + pos;
    b.r = s.r.data() + pos;
    b.numFrames = n;
    b.transport = &t;
    m.process(b);
    s.stereo = b.stereo;
  }
  return s;
}

int firstPeakAfter(const std::vector<float>& x, int from, float thr)
{
  for (size_t i = static_cast<size_t>(from); i < x.size(); ++i)
    if (std::fabs(x[i]) > thr) return static_cast<int>(i);
  return -1;
}

} // namespace

TEST_CASE("fx: every new module is silent on silence and finite on hot noise")
{
  std::unique_ptr<Module> mods[] = { std::make_unique<Eq>(),      std::make_unique<Delay>(),   std::make_unique<Reverb>(),
                                     std::make_unique<Chorus>(),  std::make_unique<Phaser>(),  std::make_unique<Flanger>(),
                                     std::make_unique<Tremolo>(), std::make_unique<Vibrato>(), std::make_unique<Rotary>(),
                                     std::make_unique<EnvFilter>() };
  auto p = defaults();
  p[kDlyFeedback] = 100.f; p[kFlaFeedback] = 95.f; p[kPhaFeedback] = 95.f; p[kRvbDecay] = 20.f; p[kDlyMix] = 100.f;
  for (auto& m : mods)
  {
    auto s = run(*m, std::vector<float>(24000, 0.f), p);
    CHECK_MSG(peak(s.l) < 1e-6f && peak(s.r) < 1e-6f, "module %d silence", static_cast<int>(m->id()));
    auto h = run(*m, noise(48000, 2.f), p, 97);
    CHECK(allFinite(h.l) && allFinite(h.r));
    CHECK_MSG(peak(h.l) < 12.f && peak(h.r) < 12.f, "module %d peak %g/%g", static_cast<int>(m->id()), peak(h.l), peak(h.r));
  }
}

TEST_CASE("delay: echo arrives at the set time, sync follows tempo, max feedback stays bounded")
{
  auto p = defaults();
  p[kDlyMix] = 50.f;
  p[kDlyFeedback] = 0.f;
  p[kDlyTime] = 250.f;
  p[kDlyModDepth] = 0.f;
  p[kDlyLowCut] = 20.f;
  p[kDlyHighCut] = 20000.f;
  p[kDlySaturation] = 0.f;
  Delay d;
  auto s = run(d, impulse(24000, 0.5f), p);
  const int echo = firstPeakAfter(s.l, 100, 0.05f);
  CHECK_MSG(std::abs(echo - 12000) <= 3, "echo at %d", echo);

  // Синхронизация: 1/4 при 100 BPM = 600 мс
  p[kDlySync] = 1.f;
  p[kDlyDivision] = 4.f;
  TransportInfo t;
  t.tempo = 100.0;
  t.hostTempoValid = true;
  Delay d2;
  auto s2 = run(d2, impulse(48000, 0.5f), p, 128, 48000.0, t);
  const int e2 = firstPeakAfter(s2.l, 100, 0.05f);
  CHECK_MSG(std::abs(e2 - 28800) <= 3, "sync echo at %d", e2);
  CHECK(d2.currentTimeMs() > 599.f && d2.currentTimeMs() < 601.f);

  for (int mode = 0; mode < 5; ++mode)
  {
    auto q = defaults();
    q[kDlyMode] = static_cast<float>(mode);
    q[kDlyFeedback] = 100.f;
    q[kDlyMix] = 100.f;
    q[kDlyTime] = 50.f;
    Delay dd;
    auto in = noise(96000, 0.8f);
    auto out = run(dd, in, q, 256);
    CHECK(allFinite(out.l));
    CHECK_MSG(peak(out.l, 48000) < 4.f, "mode %d peak %g", mode, peak(out.l, 48000));
    CHECK(out.stereo);
  }
}

TEST_CASE("reverb: tail length follows decay, freeze sustains, all modes stable")
{
  auto energyAfter = [](float decay, int mode, int fromMs) {
    auto p = defaults();
    p[kRvbMix] = 100.f;
    p[kRvbDecay] = decay;
    p[kRvbMode] = static_cast<float>(mode);
    p[kRvbPreDelay] = 0.f;
    Reverb r;
    auto s = run(r, impulse(96000, 1.f), p);
    return rms(s.l, fromMs * 48);
  };
  for (int mode = 0; mode < 5; ++mode)
  {
    const float shortT = energyAfter(0.5f, mode, 1000), longT = energyAfter(5.f, mode, 1000);
    CHECK_MSG(longT > shortT * 10.f, "mode %d short %g long %g", mode, shortT, longT);
  }
  // Freeze: хвост не затухает
  auto p = defaults();
  p[kRvbMix] = 100.f;
  p[kRvbDecay] = 2.f;
  Reverb r;
  r.prepare({ 48000.0, 256 });
  r.update(p);
  auto in = noise(144000, 0.3f);
  std::fill(in.begin() + 24000, in.end(), 0.f);
  std::vector<float> rr(in.size());
  for (size_t pos = 0; pos < in.size(); pos += 256)
  {
    if (pos == 23040) { p[kRvbFreeze] = 1.f; r.update(p); }
    AudioBlock b;
    b.l = in.data() + pos;
    b.r = rr.data() + pos;
    b.numFrames = 256;
    r.process(b);
  }
  const float e1 = rms(in.data() + 48000, 24000), e2 = rms(in.data() + 120000, 24000);
  CHECK_MSG(e2 > e1 * 0.5f, "freeze e1 %g e2 %g", e1, e2);
}

TEST_CASE("eq: processed response matches UI curve function")
{
  auto p = defaults();
  p[kEqOn] = 1.f;
  p[kEqB3Gain] = 9.f;
  p[kEqB3Freq] = 1000.f;
  p[kEqB3Q] = 1.5f;
  p[kEqB1Gain] = -6.f;
  p[kEqLowCut] = 80.f;
  p[kEqLowSlope] = 1.f;
  auto getter = [&](int id) { return p[static_cast<size_t>(id)]; };
  for (float f : { 50.f, 120.f, 1000.f, 4000.f })
  {
    Eq e;
    auto s = run(e, sine(f, 0.1f, 48000.f, 48000), p);
    const float measured = db(toneAmplitude(s.l, 24000, f, 48000.f) / 0.1f);
    const float expected = static_cast<float>(Eq::responseDb(getter, f, 48000.0));
    CHECK_MSG(std::fabs(measured - expected) < 0.3f, "f %.0f measured %.2f expected %.2f", f, measured, expected);
  }
}

TEST_CASE("modulation: tremolo depth, stereo outputs, phaser creates notches")
{
  auto p = defaults();
  p[kTrmDepth] = 100.f;
  p[kTrmRate] = 4.f;
  Tremolo t;
  auto s = run(t, std::vector<float>(48000, 0.5f), p);
  float mn = 1.f, mx = 0.f;
  for (size_t i = 12000; i < s.l.size(); ++i) { mn = std::min(mn, s.l[i]); mx = std::max(mx, s.l[i]); }
  CHECK(mn < 0.02f && mx > 0.48f);

  p[kTrmStereo] = 1.f;
  Tremolo t2;
  auto s2 = run(t2, std::vector<float>(48000, 0.5f), p);
  CHECK(s2.stereo);
  float maxSum = 0.f;
  for (size_t i = 12000; i < s2.l.size(); ++i) maxSum = std::max(maxSum, std::fabs(s2.l[i] + s2.r[i] - 0.5f));
  CHECK(maxSum < 0.05f); // панорама: сумма каналов постоянна

  std::unique_ptr<Module> stereoMods[] = { std::make_unique<Chorus>(), std::make_unique<Flanger>(), std::make_unique<Rotary>() };
  for (auto& m : stereoMods)
  {
    auto out = run(*m, sine(440.f, 0.3f, 48000.f, 48000), defaults());
    CHECK(out.stereo);
    double diff = 0;
    for (size_t i = 24000; i < out.l.size(); ++i) diff += std::fabs(out.l[i] - out.r[i]);
    CHECK_MSG(diff > 10.0, "module %d stereo diff %g", static_cast<int>(m->id()), diff);
  }

  // Фазер на 100% Mix (50/50) с остановленным LFO даёт провал хотя бы в одной из полос
  auto q = defaults();
  q[kPhaMix] = 100.f;
  q[kPhaRate] = 0.02f;
  q[kPhaFeedback] = 0.f;
  float minA = 1.f;
  for (float f = 150.f; f < 4000.f; f *= 1.12f)
  {
    Phaser ph;
    auto out = run(ph, sine(f, 0.3f, 48000.f, 9600), q);
    minA = std::min(minA, toneAmplitude(out.l, 4800, f, 48000.f) / 0.3f);
  }
  CHECK_MSG(minA < 0.3f, "deepest notch %.3f", minA);
}

TEST_CASE("env filter: louder playing opens the filter (up mode)")
{
  auto p = defaults();
  p[kEnvMode] = 0.f; // LP
  p[kEnvSensitivity] = 6.f;
  auto band = [&](float amp) {
    EnvFilter e;
    auto s = run(e, sine(2000.f, amp, 48000.f, 24000), p);
    return toneAmplitude(s.l, 12000, 2000.f, 48000.f) / amp;
  };
  CHECK(band(0.5f) > band(0.01f) * 2.f);
}
