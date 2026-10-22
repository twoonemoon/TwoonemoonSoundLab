// SPDX-License-Identifier: GPL-3.0-or-later
#include "Reverb.h"

namespace tsl {

namespace {

constexpr float kBaseMs[Reverb::kLines] = { 31.7f, 37.9f, 41.3f, 47.9f, 53.3f, 59.9f, 67.1f, 73.7f };
constexpr float kOutL[Reverb::kLines] = { 1, -1, 1, -1, 1, -1, 1, -1 };
constexpr float kOutR[Reverb::kLines] = { 1, 1, -1, -1, 1, 1, -1, -1 };
constexpr float kErMs[8] = { 3.1f, 5.3f, 7.9f, 11.3f, 13.7f, 17.1f, 19.9f, 23.3f };
constexpr float kErGain[8] = { 0.85f, -0.7f, 0.6f, -0.52f, 0.45f, -0.38f, 0.3f, -0.24f };
constexpr float kDiffMs[4] = { 4.7f, 3.6f, 12.7f, 9.3f };
constexpr float kSpringMs[3] = { 29.3f, 37.1f, 43.9f };

float sizeFactor(int mode, float size)
{
  switch (mode)
  {
    case 0: return 0.30f + 0.55f * size; // Room
    case 2: return 0.45f + 0.45f * size; // Plate
    case 4: return 1.10f + 1.00f * size; // Shimmer
    case 1:
    default: return 0.90f + 1.10f * size; // Hall
  }
}

} // namespace

void Reverb::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const float sr = static_cast<float>(mSampleRate);
  mPre.prepare(static_cast<int>(0.26 * mSampleRate) + 8);
  for (auto& l : mLines) l.prepare(static_cast<int>(0.17 * mSampleRate) + 64);
  for (int i = 0; i < 4; ++i) mDiff[i].line.prepare(static_cast<int>(0.02 * mSampleRate) + 4);
  mEr.prepare(static_cast<int>(0.06 * mSampleRate) + 8);
  for (auto& s : mSpring)
  {
    for (auto& a : s.disp) a.line.prepare(16);
    s.loop.prepare(static_cast<int>(0.05 * mSampleRate) + 8);
  }
  mShift.prepare(mSampleRate, 70.f);
  mShift.setSemitones(12.f);
  mShiftHp.setCoefs(design::make(FilterType::HighPass, mSampleRate, 350.0, 0.707));
  mDry.prepare(sr, 0.03f); mWet.prepare(sr, 0.03f); mInGain.prepare(sr, 0.06f);
  mDry.snap(mDry.target()); mWet.snap(mWet.target()); mInGain.snap(1.f);
  mDirty = true;
  recalc();
  reset();
}

void Reverb::reset() noexcept
{
  mPre.reset();
  for (auto& l : mLines) l.reset();
  for (auto& d : mDampState) d = 0.f;
  for (auto& a : mDiff) a.line.reset();
  mEr.reset();
  for (auto& s : mSpring)
  {
    for (auto& a : s.disp) a.line.reset();
    s.loop.reset();
    s.lp = s.hp = 0.f;
  }
  mShift.reset();
  mShiftHp.reset();
  mShimmerFb = 0.f;
  mInHp.reset();
  mInLp.reset();
  for (int i = 0; i < 4; ++i) mLfo[i].reset(0.25f * i);
}

void Reverb::update(const ParamValues& p) noexcept
{
  mMode = static_cast<int>(p[kRvbMode]);
  mDecay = p[kRvbDecay];
  mSize = p[kRvbSize] * 0.01f;
  mPreDelayMs = p[kRvbPreDelay];
  mEarly = p[kRvbEarly] * 0.01f;
  mDiffusion = p[kRvbDiffusion] * 0.01f;
  mHighDamp = p[kRvbHighDamp];
  mLowCut = p[kRvbLowCut];
  mModulation = p[kRvbModulation] * 0.01f;
  mWidth = p[kRvbWidth] * 0.01f;
  mFreeze = p[kRvbFreeze] >= 0.5f;
  mShimmer = p[kRvbShimmer] * 0.01f;
  const float mix = p[kRvbMix] * 0.01f;
  mDry.setTarget(std::min(1.f, 2.f * (1.f - mix)));
  mWet.setTarget(std::min(1.f, 2.f * mix));
  mInGain.setTarget(mFreeze ? 0.f : 1.f);
  mDirty = true;
}

void Reverb::recalc() noexcept
{
  const float sr = static_cast<float>(mSampleRate);
  const float sf = sizeFactor(mMode, mSize);
  const float maxLen = static_cast<float>(mLines[0].maxDelay() - 64);
  for (int i = 0; i < kLines; ++i)
  {
    mLineLen[i] = std::min(maxLen, kBaseMs[i] * sf * 0.001f * sr);
    mGain[i] = mFreeze ? 1.0f : std::pow(10.f, -3.f * mLineLen[i] / (std::max(0.1f, mDecay) * sr));
  }
  const float damp = mFreeze ? 20000.f : (mMode == 2 ? std::min(20000.f, mHighDamp * 1.4f) : mHighDamp);
  mDampA = std::exp(-kTwoPi * std::min(damp, sr * 0.45f) / sr);
  const float dg = mMode == 2 ? 0.72f : 0.4f + 0.32f * mDiffusion;
  const float dsf = mMode == 0 ? 0.6f : 1.f;
  for (int i = 0; i < 4; ++i)
  {
    mDiff[i].delay = std::max(1, static_cast<int>(kDiffMs[i] * dsf * 0.001f * sr));
    mDiff[i].g = dg;
  }
  const float erScale = mMode == 0 ? (0.6f + 0.8f * mSize) : (1.4f + 1.6f * mSize);
  for (int i = 0; i < 8; ++i) mErTap[i] = std::min(static_cast<float>(mEr.maxDelay() - 2), kErMs[i] * erScale * 0.001f * sr);
  mInHp.setCoefs(design::make(FilterType::HighPass, mSampleRate, mLowCut, 0.707));
  mInLp.setCoefs(design::make(FilterType::LowPass, mSampleRate, mMode == 3 ? 4500.0 : 16000.0, 0.707));
  for (int s = 0; s < 3; ++s)
  {
    Spring& sp = mSpring[s];
    sp.len = std::min(sp.loop.maxDelay() - 2, static_cast<int>(kSpringMs[s] * (0.8f + 0.4f * mSize) * 0.001f * sr));
    sp.fb = mFreeze ? 0.995f : std::pow(10.f, -3.f * sp.len / (std::max(0.1f, mDecay) * sr));
    for (int a = 0; a < 10; ++a)
    {
      sp.disp[a].delay = 1 + (a % 3);
      sp.disp[a].g = 0.62f + 0.02f * s;
    }
  }
  mDirty = false;
}

float Reverb::processSpring(float in, float& outL, float& outR) noexcept
{
  float sum[3];
  for (int s = 0; s < 3; ++s)
  {
    Spring& sp = mSpring[s];
    float x = in + sp.fb * sp.loop.readInt(sp.len);
    for (auto& a : sp.disp) x = a.process(x);
    sp.lp += 0.35f * (x - sp.lp);  // мягкий ФНЧ (характерная тусклость пружины)
    sp.hp += 0.004f * (sp.lp - sp.hp);
    const float y = 1.2f * fastTanh((sp.lp - sp.hp) / 1.2f);
    sp.loop.push(y);
    sum[s] = y;
  }
  outL = 0.6f * sum[0] + 0.4f * sum[1];
  outR = 0.6f * sum[2] + 0.4f * sum[1];
  return 0.f;
}

void Reverb::process(AudioBlock& b) noexcept
{
  const int n = b.numFrames;
  const float sr = static_cast<float>(mSampleRate);
  if (mDirty) recalc();
  const bool inStereo = b.stereo;
  const float pre = std::max(1.f, mPreDelayMs * 0.001f * sr);
  const float modAmt = mModulation * 0.0009f * sr; // до ~0.9 мс

  for (int i = 0; i < n; ++i)
  {
    const float inL = b.l[i];
    const float inR = inStereo ? b.r[i] : inL;
    float x = 0.5f * (inL + inR) * mInGain.next();
    x = mInLp.process(mInHp.process(x));
    mPre.push(x);
    float xp = mPre.readLinear(pre);

    float wl = 0.f, wr = 0.f;
    if (mMode == 3)
    {
      processSpring(xp * 0.5f, wl, wr);
      wl *= 1.3f;
      wr *= 1.3f;
    }
    else
    {
      // Ранние отражения
      float erl = 0.f, err = 0.f;
      if (mMode <= 1 && mEarly > 0.f)
      {
        mEr.push(xp);
        for (int t = 0; t < 8; ++t)
        {
          const float v = kErGain[t] * mEr.readInt(static_cast<int>(mErTap[t]));
          if (t & 1) err += v; else erl += v;
        }
      }
      // Диффузия
      float d = xp + mShimmerFb;
      for (auto& a : mDiff) d = a.process(d);

      // FDN
      float out[kLines];
      for (int k = 0; k < kLines; ++k)
      {
        float len = mLineLen[k];
        if (k % 2 == 0) len += modAmt * mLfo[k / 2].next(0.31f + 0.13f * k, sr);
        out[k] = mLines[k].readLinear(len);
      }
      float sum = 0.f;
      for (float v : out) sum += v;
      const float hh = 2.f / kLines * sum;
      for (int k = 0; k < kLines; ++k)
      {
        float v = (out[k] - hh) * mGain[k];
        mDampState[k] = v + mDampA * (mDampState[k] - v);
        v = mDampState[k];
        mLines[k].push(v + ((k & 1) ? -d : d) * 0.35f);
        wl += kOutL[k] * out[k];
        wr += kOutR[k] * out[k];
      }
      wl *= 0.35f;
      wr *= 0.35f;
      // Shimmer: октава вверх в контуре подпитки
      if (mMode == 4 && mShimmer > 0.f)
      {
        const float s = mShiftHp.process(mShift.process(0.5f * (wl + wr)));
        mShimmerFb = 0.9f * fastTanh(s * mShimmer * 0.55f);
      }
      else
        mShimmerFb = 0.f;
      wl += erl * mEarly * 0.7f;
      wr += err * mEarly * 0.7f;
    }
    // Ширина стереобазы
    const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * mWidth;
    const float dry = mDry.next(), wet = mWet.next();
    b.l[i] = inL * dry + (mid + side) * wet;
    b.r[i] = inR * dry + (mid - side) * wet;
  }
  for (auto& v : mDampState) v = undenormal(v);
  b.stereo = true;
}

} // namespace tsl
