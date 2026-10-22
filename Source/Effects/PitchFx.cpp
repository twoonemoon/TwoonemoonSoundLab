// SPDX-License-Identifier: GPL-3.0-or-later
#include "PitchFx.h"

namespace tsl {

// ============================================================================ Лады
namespace {

struct ScaleDef
{
  int size;
  int steps[12];
};

const ScaleDef kScales[13] = {
  { 7, { 0, 2, 4, 5, 7, 9, 11 } },  // Major
  { 7, { 0, 2, 3, 5, 7, 8, 10 } },  // Natural Minor
  { 7, { 0, 2, 3, 5, 7, 8, 11 } },  // Harmonic Minor
  { 7, { 0, 2, 3, 5, 7, 9, 11 } },  // Melodic Minor
  { 7, { 0, 2, 3, 5, 7, 9, 10 } },  // Dorian
  { 7, { 0, 1, 3, 5, 7, 8, 10 } },  // Phrygian
  { 7, { 0, 2, 4, 6, 7, 9, 11 } },  // Lydian
  { 7, { 0, 2, 4, 5, 7, 9, 10 } },  // Mixolydian
  { 7, { 0, 1, 3, 5, 6, 8, 10 } },  // Locrian
  { 5, { 0, 2, 4, 7, 9 } },         // Major Pentatonic
  { 5, { 0, 3, 5, 7, 10 } },        // Minor Pentatonic
  { 6, { 0, 3, 5, 6, 7, 10 } },     // Blues
  { 12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } }, // Chromatic
};

// Интервалы: диатонические ступени (для 7-ступенных ладов) и полутоны (для Chromatic)
constexpr int kIntervalDegrees[16] = { -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9 };
constexpr int kIntervalSemis[16] = { -12, -10, -9, -7, -5, -4, -2, 0, 2, 4, 5, 7, 9, 11, 12, 16 };

int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

} // namespace

int harmonyShiftSemitones(int midi, int key, int scale, int intervalIndex) noexcept
{
  scale = clampv(scale, 0, 12);
  intervalIndex = clampv(intervalIndex, 0, 15);
  const ScaleDef& sc = kScales[scale];
  if (sc.size == 12 || midi < 0) return kIntervalSemis[intervalIndex];
  // Ступень лада, ближайшая к ноте (ноты вне лада притягиваются к ближайшей ступени снизу/сверху)
  const int rel = ((midi - key) % 12 + 12) % 12;
  int degree = 0, bestDist = 99;
  for (int d = 0; d < sc.size; ++d)
  {
    const int dist = std::abs(sc.steps[d] - rel);
    if (dist < bestDist) { bestDist = dist; degree = d; }
  }
  const int octaveBase = midi - rel; // MIDI-нота тоники ниже текущей
  int steps = kIntervalDegrees[intervalIndex];
  if (sc.size != 7) steps = static_cast<int>(std::lround(steps * sc.size / 7.0));
  const int target = degree + steps;
  const int oct = floorDiv(target, sc.size);
  const int idx = target - oct * sc.size;
  const int targetMidi = octaveBase + oct * 12 + sc.steps[idx];
  const int sourceMidi = octaveBase + sc.steps[degree];
  return targetMidi - sourceMidi;
}

// ============================================================================ Wah
void Wah::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const float sr = static_cast<float>(mSampleRate);
  mPos.prepare(sr, 0.015f);
  mPos.snap(0.5f);
  mMix.prepare(sr, 0.03f);
  mMix.snap(mMix.target());
  mAtt = onePoleCoef(0.006f, sr);
  mRel = onePoleCoef(0.12f, sr);
  reset();
}
void Wah::reset() noexcept
{
  for (auto& s : mSvf) s.reset();
  mEnv = 0.f;
}
void Wah::update(const ParamValues& p) noexcept
{
  mMode = static_cast<int>(p[kWahMode]);
  mQ = p[kWahQ];
  mSens = p[kWahSensitivity] * 0.1f;
  if (mMode == 0) mPos.setTarget(p[kWahPosition] * 0.01f);
  mMix.setTarget(p[kWahMix] * 0.01f);
}
void Wah::process(AudioBlock& b) noexcept
{
  const float sr = static_cast<float>(mSampleRate);
  const float gain = 2.f + 50.f * mSens * mSens;
  float pos = 0.f;
  for (int i = 0; i < b.numFrames; ++i)
  {
    const float inL = b.l[i], inR = b.stereo ? b.r[i] : inL;
    if (mMode == 1)
    {
      const float a = std::fabs(0.5f * (inL + inR));
      mEnv = a > mEnv ? a + mAtt * (mEnv - a) : a + mRel * (mEnv - a);
      mPos.setTarget(clampv(mEnv * gain, 0.f, 1.f));
    }
    pos = mPos.next();
    // Ход педали: 350 Гц … 2.2 кГц, кривая близка к реальной (больше движения в середине)
    const float f = 350.f * std::pow(6.3f, pos);
    const float m = mMix.next();
    for (int c = 0; c < (b.stereo ? 2 : 1); ++c)
    {
      mSvf[c].set(f, mQ, sr);
      const float x = c == 0 ? inL : inR;
      const auto o = mSvf[c].process(x);
      // Резонансный полосовой + немного НЧ: характерный «голос» педали
      const float y = (o.bp * 1.8f + o.lp * 0.25f);
      (c == 0 ? b.l : b.r)[i] = x + m * (y - x);
    }
  }
  mEnv = undenormal(mEnv);
  mShownPos.store(pos, std::memory_order_relaxed);
}

// ============================================================================ Octave
void Octave::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const float sr = static_cast<float>(mSampleRate);
  // Трекинг основного тона: ФНЧ подавляет гармоники перед детектором пересечений нуля
  const auto lp = design::make(FilterType::LowPass, mSampleRate, 380.0, 0.6);
  mTrackLp.setCoefs(lp);
  mTrackLp2.setCoefs(lp);
  mUpHp.setCoefs(design::make(FilterType::HighPass, mSampleRate, 80.0, 0.707));
  mEnvA = onePoleCoef(0.004f, sr);
  mEnvR = onePoleCoef(0.06f, sr);
  for (auto* s : { &mDown1, &mDown2, &mUp }) s->prepare(mSampleRate, 50.f);
  mDown1.setSemitones(-12.f);
  mDown2.setSemitones(-24.f);
  mUp.setSemitones(12.f);
  for (auto* s : { &mSub1, &mSub2, &mUpL, &mDry }) { s->prepare(sr, 0.03f); s->snap(s->target()); }
  reset();
}
void Octave::reset() noexcept
{
  mTrackLp.reset(); mTrackLp2.reset(); mToneLp.reset(); mUpHp.reset();
  mFf1 = mFf2 = false;
  mArmed = true;
  mEnv = 0.f;
  mDown1.reset(); mDown2.reset(); mUp.reset();
}
void Octave::update(const ParamValues& p) noexcept
{
  mMode = static_cast<int>(p[kOctMode]);
  mSub1.setTarget(p[kOctSub1] * 0.01f);
  mSub2.setTarget(p[kOctSub2] * 0.01f);
  mUpL.setTarget(p[kOctUp] * 0.01f);
  mDry.setTarget(p[kOctDry] * 0.01f);
  mToneLp.setCoefs(design::make(FilterType::LowPass, mSampleRate, p[kOctTone], 0.707));
}
void Octave::process(AudioBlock& b) noexcept
{
  for (int i = 0; i < b.numFrames; ++i)
  {
    const float inL = b.l[i], inR = b.stereo ? b.r[i] : inL;
    const float x = 0.5f * (inL + inR);
    float sub1, sub2, up;
    if (mMode == 0)
    {
      // Аналоговый делитель: триггер по пересечению нуля с гистерезисом → меандр f/2 и f/4
      const float t = mTrackLp2.process(mTrackLp.process(x));
      const float a = std::fabs(x);
      mEnv = a > mEnv ? a + mEnvA * (mEnv - a) : a + mEnvR * (mEnv - a);
      const float hyst = 0.08f * mEnv;
      if (mArmed && t > hyst)
      {
        mArmed = false;
        mFf1 = !mFf1;
        if (mFf1) mFf2 = !mFf2;
      }
      else if (!mArmed && t < -hyst)
        mArmed = true;
      const float gate = mEnv > 0.002f ? 1.f : 0.f;
      sub1 = mToneLp.process((mFf1 ? 1.f : -1.f) * mEnv * 0.9f * gate);
      sub2 = (mFf2 ? 1.f : -1.f) * mEnv * 0.8f * gate;
      up = mUpHp.process(std::fabs(x) * 1.6f);
    }
    else
    {
      sub1 = mToneLp.process(mDown1.process(x));
      sub2 = mDown2.process(x);
      up = mUp.process(x);
    }
    const float wet = sub1 * mSub1.next() + sub2 * mSub2.next() + up * mUpL.next();
    const float dry = mDry.next();
    b.l[i] = inL * dry + wet;
    if (b.stereo) b.r[i] = inR * dry + wet;
  }
  mEnv = undenormal(mEnv);
}

// ============================================================================ Pitch
void PitchShift::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  for (int c = 0; c < 2; ++c)
  {
    mShift[0][c].prepare(mSampleRate, 40.f);
    mShift[1][c].prepare(mSampleRate, 80.f);
  }
  mMix.prepare(static_cast<float>(mSampleRate), 0.03f);
  mMix.snap(mMix.target());
  reset();
}
void PitchShift::reset() noexcept
{
  for (auto& q : mShift)
    for (auto& s : q) s.reset();
}
void PitchShift::update(const ParamValues& p) noexcept
{
  mSemis = p[kPitchSemitones];
  mFine = p[kPitchFine];
  mQuality = static_cast<int>(p[kPitchQuality]);
  for (auto& q : mShift)
    for (auto& s : q) s.setSemitones(mSemis + mFine * 0.01f);
  mMix.setTarget(p[kPitchMix] * 0.01f);
}
void PitchShift::process(AudioBlock& b) noexcept
{
  auto& sh = mShift[clampv(mQuality, 0, 1)];
  for (int i = 0; i < b.numFrames; ++i)
  {
    const float m = mMix.next();
    const float l = b.l[i];
    b.l[i] = l + m * (sh[0].process(l) - l);
    if (b.stereo)
    {
      const float r = b.r[i];
      b.r[i] = r + m * (sh[1].process(r) - r);
    }
  }
}

// ============================================================================ Harmonizer
void Harmonizer::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const float sr = static_cast<float>(mSampleRate);
  mDetector.prepare(mSampleRate, 70.f, 1400.f);
  for (auto& v : mVoice)
  {
    v.shift.prepare(mSampleRate, 40.f);
    v.delay.prepare(static_cast<int>(0.065 * mSampleRate) + 8);
    v.ratio.prepare(sr, 0.012f);
    v.ratio.snap(1.f);
    v.level.prepare(sr, 0.03f);
    v.level.snap(v.level.target());
    v.delaySmp.prepare(sr, 0.05f);
    v.delaySmp.snap(v.delaySmp.target());
  }
  mDry.prepare(sr, 0.03f);
  mVoicesLevel.prepare(sr, 0.03f);
  mDry.snap(mDry.target());
  mVoicesLevel.snap(mVoicesLevel.target());
  reset();
}

void Harmonizer::reset() noexcept
{
  for (auto& v : mVoice)
  {
    v.shift.reset();
    v.delay.reset();
  }
  mNote = -1;
  mSinceEstimate = 0;
  mShownNote.store(-1, std::memory_order_relaxed);
}

void Harmonizer::update(const ParamValues& p) noexcept
{
  mKey = static_cast<int>(p[kHarmKey]);
  mScale = static_cast<int>(p[kHarmScale]);
  mDry.setTarget(p[kHarmDry] * 0.01f);
  mVoicesLevel.setTarget(p[kHarmVoices] * 0.01f);
  for (int i = 0; i < kVoices; ++i)
  {
    const int b = kHarmV1On + i * 6; // On, Interval, Level, Pan, Delay, Detune
    Voice& v = mVoice[i];
    v.on = p[static_cast<size_t>(b)] >= 0.5f;
    v.interval = static_cast<int>(p[static_cast<size_t>(b + 1)]);
    v.level.setTarget(v.on ? p[static_cast<size_t>(b + 2)] * 0.01f : 0.f);
    v.pan = p[static_cast<size_t>(b + 3)] * 0.01f;
    v.delaySmp.setTarget(p[static_cast<size_t>(b + 4)] * 0.001f * static_cast<float>(mSampleRate));
    v.detune = p[static_cast<size_t>(b + 5)];
  }
  retarget();
}

void Harmonizer::retarget() noexcept
{
  for (auto& v : mVoice)
  {
    const int st = harmonyShiftSemitones(mNote, mKey, mScale, v.interval);
    v.ratio.setTarget(std::pow(2.f, (static_cast<float>(st) + v.detune * 0.01f) / 12.f));
  }
}

void Harmonizer::process(AudioBlock& b) noexcept
{
  const int n = b.numFrames;
  // Определение ноты по чистому DI-сигналу (если доступен), иначе по входу модуля
  const float* key = b.keyInput ? b.keyInput : b.l;
  mDetector.push(key, n);
  mSinceEstimate += n;
  if (mSinceEstimate >= static_cast<int>(mSampleRate * 0.008))
  {
    mSinceEstimate = 0;
    const auto r = mDetector.estimate();
    if (r.frequency > 0.f && r.clarity > 0.85f)
    {
      const int note = static_cast<int>(std::lround(69.f + 12.f * std::log2(r.frequency / 440.f)));
      if (note != mNote)
      {
        mNote = note;
        retarget();
      }
      mShownNote.store(mNote, std::memory_order_relaxed);
    }
  }

  const bool inStereo = b.stereo;
  for (int i = 0; i < n; ++i)
  {
    const float inL = b.l[i], inR = inStereo ? b.r[i] : inL;
    const float x = 0.5f * (inL + inR);
    float wl = 0.f, wr = 0.f;
    for (auto& v : mVoice)
    {
      const float lvl = v.level.next();
      const float ratio = v.ratio.next();
      const float d = v.delaySmp.next();
      if (lvl <= 0.f && !v.level.isSmoothing()) continue;
      v.shift.setRatio(ratio);
      const float y = v.shift.process(x);
      v.delay.push(y);
      const float yd = d > 0.5f ? v.delay.readLinear(1.f + d) : y;
      wl += yd * lvl * std::min(1.f, 1.f - v.pan);
      wr += yd * lvl * std::min(1.f, 1.f + v.pan);
    }
    const float dry = mDry.next(), vl = mVoicesLevel.next();
    b.l[i] = inL * dry + wl * vl;
    b.r[i] = inR * dry + wr * vl;
  }
  b.stereo = true;
}

} // namespace tsl
