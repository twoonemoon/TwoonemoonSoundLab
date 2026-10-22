// SPDX-License-Identifier: GPL-3.0-or-later
#include "../TestHarness.h"
#include "../TestSignals.h"

#include "../../Source/Effects/PitchFx.h"

using namespace tsl;
using namespace tsltest;

namespace {

ParamValues defaults()
{
  ParamValues p{};
  for (int i = 0; i < kNumParams; ++i) p[static_cast<size_t>(i)] = paramSpec(i).defaultValue;
  return p;
}

std::vector<float> runL(Module& m, const std::vector<float>& in, const ParamValues& p, int block = 128, std::vector<float>* r = nullptr)
{
  m.prepare({ 48000.0, 512 });
  m.update(p);
  std::vector<float> l = in, rr(in.size(), 0.f);
  for (size_t pos = 0; pos < l.size(); pos += static_cast<size_t>(block))
  {
    const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(block), l.size() - pos));
    AudioBlock b;
    b.l = l.data() + pos;
    b.r = rr.data() + pos;
    b.numFrames = n;
    b.keyInput = in.data() + pos;
    m.process(b);
  }
  if (r) *r = rr;
  return l;
}

/** Частота с наибольшей амплитудой среди кандидатов. */
float dominant(const std::vector<float>& x, int from, std::initializer_list<float> cands)
{
  float best = 0.f, bestA = -1.f;
  for (float f : cands)
  {
    const float a = toneAmplitude(x, from, f, 48000.f);
    if (a > bestA) { bestA = a; best = f; }
  }
  return best;
}

} // namespace

TEST_CASE("harmony: diatonic intervals in A minor and chromatic fallback")
{
  const int A = 9, minor = 1, major = 0, chromatic = 12;
  // Индексы интервалов: 9 = +3rd, 11 = +5th, 14 = +Octave, 5 = -3rd, 7 = Unison
  CHECK(harmonyShiftSemitones(57, A, minor, 9) == 3);   // A → C
  CHECK(harmonyShiftSemitones(60, A, minor, 9) == 4);   // C → E
  CHECK(harmonyShiftSemitones(64, A, minor, 11) == 7);  // E → B
  CHECK(harmonyShiftSemitones(59, A, minor, 11) == 6);  // B → F (уменьшённая квинта лада)
  CHECK(harmonyShiftSemitones(57, A, minor, 14) == 12);
  CHECK(harmonyShiftSemitones(57, A, minor, 7) == 0);
  CHECK(harmonyShiftSemitones(60, A, minor, 5) == -3);  // C → A
  CHECK(harmonyShiftSemitones(60, 0, major, 9) == 4);   // C мажор: C → E
  CHECK(harmonyShiftSemitones(62, 0, major, 9) == 3);   // D → F
  CHECK(harmonyShiftSemitones(61, 0, chromatic, 9) == 4);
  CHECK(harmonyShiftSemitones(-1, 0, major, 11) == 7);  // нота неизвестна → фиксированный интервал
  for (int key = 0; key < 12; ++key)
    for (int sc = 0; sc < 13; ++sc)
      for (int iv = 0; iv < 16; ++iv)
      {
        const int s = harmonyShiftSemitones(48 + key, key, sc, iv);
        CHECK(s >= -14 && s <= 17);
      }
}

TEST_CASE("pitch shifter: +12 st doubles frequency, -12 halves it")
{
  auto p = defaults();
  p[kPitchMix] = 100.f;
  p[kPitchSemitones] = 12.f;
  PitchShift ps;
  auto out = runL(ps, sine(220.f, 0.3f, 48000.f, 48000), p);
  CHECK(dominant(out, 12000, { 110.f, 220.f, 440.f, 660.f }) == 440.f);
  p[kPitchSemitones] = -12.f;
  PitchShift ps2;
  auto out2 = runL(ps2, sine(440.f, 0.3f, 48000.f, 48000), p);
  CHECK(dominant(out2, 12000, { 220.f, 440.f, 880.f }) == 220.f);
  CHECK(allFinite(out2));
}

TEST_CASE("octave: analog divider produces sub-octave with zero latency, poly mode too")
{
  auto p = defaults();
  p[kOctDry] = 0.f;
  p[kOctSub1] = 100.f;
  p[kOctTone] = 5000.f;
  Octave o;
  auto out = runL(o, sine(196.f, 0.3f, 48000.f, 48000), p);
  CHECK(toneAmplitude(out, 12000, 98.f, 48000.f) > toneAmplitude(out, 12000, 196.f, 48000.f) * 2.f);
  p[kOctMode] = 1.f;
  Octave o2;
  auto out2 = runL(o2, sine(196.f, 0.3f, 48000.f, 48000), p);
  CHECK(dominant(out2, 12000, { 98.f, 196.f, 392.f }) == 98.f);
}

TEST_CASE("wah: pedal position moves the resonant peak")
{
  auto peakAt = [](float pos, float f) {
    auto p = defaults();
    p[kWahPosition] = pos;
    Wah w;
    auto out = runL(w, sine(f, 0.1f, 48000.f, 24000), p);
    return toneAmplitude(out, 12000, f, 48000.f);
  };
  CHECK(peakAt(0.f, 400.f) > peakAt(100.f, 400.f) * 2.f);
  CHECK(peakAt(100.f, 2000.f) > peakAt(0.f, 2000.f) * 2.f);
}

TEST_CASE("harmonizer: detects the note and adds a diatonic third")
{
  auto p = defaults();
  p[kHarmKey] = 9.f;      // A
  p[kHarmScale] = 1.f;    // natural minor
  p[kHarmDry] = 0.f;
  p[kHarmV1Interval] = 9.f; // +3rd
  p[kHarmV1Pan] = 0.f;
  p[kHarmV1Detune] = 0.f;
  p[kHarmV2On] = 0.f;
  Harmonizer h;
  std::vector<float> r;
  auto out = runL(h, sine(220.f, 0.3f, 48000.f, 48000), p, 128, &r); // A3 → C4 (261.63)
  CHECK(h.detectedNote() == 57);
  CHECK(dominant(out, 24000, { 220.f, 246.94f, 261.63f, 277.18f }) == 261.63f);
}
