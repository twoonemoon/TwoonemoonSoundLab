// SPDX-License-Identifier: GPL-3.0-or-later
#include "Compressor.h"

namespace tsl {

float Compressor::staticCurve(float x, float t, float r, float w) noexcept
{
  const float slope = 1.0f / std::max(1.0f, r);
  if (w > 0.0f)
  {
    if (2.0f * (x - t) < -w) return x;
    if (2.0f * std::fabs(x - t) <= w)
    {
      const float d = x - t + w * 0.5f;
      return x + (slope - 1.0f) * d * d / (2.0f * w);
    }
    return t + (x - t) * slope;
  }
  return x <= t ? x : t + (x - t) * slope;
}

void Compressor::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const float sr = static_cast<float>(mSampleRate);
  mMakeup.prepare(sr, 0.03f);
  mMix.prepare(sr, 0.03f);
  mMakeup.snap(1.0f);
  mMix.snap(1.0f);
  mAttCoef = onePoleCoef(mAttackMs * 0.001f, sr);
  mRelCoef = onePoleCoef(mReleaseMs * 0.001f, sr);
  const auto hp = design::make(FilterType::HighPass, mSampleRate, mScHpfHz, 0.707);
  mScHpfL.setCoefs(hp);
  mScHpfR.setCoefs(hp);
  reset();
}

void Compressor::reset() noexcept
{
  mGrState = 0.f;
  mScHpfL.reset();
  mScHpfR.reset();
  mGrDb.store(0.f, std::memory_order_relaxed);
}

void Compressor::update(const ParamValues& p) noexcept
{
  const float sr = static_cast<float>(mSampleRate);
  mThreshold = p[kCompThreshold];
  mRatio = p[kCompRatio];
  mKnee = p[kCompKnee];
  if (p[kCompAttack] != mAttackMs) { mAttackMs = p[kCompAttack]; mAttCoef = onePoleCoef(mAttackMs * 0.001f, sr); }
  if (p[kCompRelease] != mReleaseMs) { mReleaseMs = p[kCompRelease]; mRelCoef = onePoleCoef(mReleaseMs * 0.001f, sr); }
  if (p[kCompScHpf] != mScHpfHz)
  {
    mScHpfHz = p[kCompScHpf];
    const auto hp = design::make(FilterType::HighPass, mSampleRate, mScHpfHz, 0.707);
    mScHpfL.setCoefs(hp);
    mScHpfR.setCoefs(hp);
  }
  mMakeup.setTarget(dbToGain(p[kCompMakeup]));
  mMix.setTarget(p[kCompMix] * 0.01f);
}

void Compressor::process(AudioBlock& b) noexcept
{
  const int n = b.numFrames;
  float gr = mGrState, maxGr = 0.f;
  for (int i = 0; i < n; ++i)
  {
    const float dl = b.l[i];
    const float dr = b.stereo ? b.r[i] : dl;
    float sc = std::fabs(mScHpfL.process(dl));
    if (b.stereo) sc = std::max(sc, std::fabs(mScHpfR.process(dr)));
    const float inDb = gainToDb(sc, -120.f);
    const float target = inDb - staticCurve(inDb, mThreshold, mRatio, mKnee); // ≥ 0
    // Smooth branching (Giannoulis/Massberg/Reiss): атака при росте ослабления, release при спаде
    gr = target > gr ? target + mAttCoef * (gr - target) : target + mRelCoef * (gr - target);
    maxGr = std::max(maxGr, gr);
    const float g = dbToGain(-gr) * mMakeup.next();
    const float mix = mMix.next();
    b.l[i] = dl + mix * (dl * g - dl);
    if (b.stereo) b.r[i] = dr + mix * (dr * g - dr);
  }
  mGrState = undenormal(gr);
  mGrDb.store(-maxGr, std::memory_order_relaxed);
}

} // namespace tsl
