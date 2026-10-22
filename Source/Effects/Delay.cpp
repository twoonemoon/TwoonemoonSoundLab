// SPDX-License-Identifier: GPL-3.0-or-later
#include "Delay.h"

namespace tsl {

void Delay::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const float sr = static_cast<float>(mSampleRate);
  for (auto& l : mLine) l.prepare(static_cast<int>(kMaxSeconds * mSampleRate) + 64);
  mDelaySmp.prepare(sr, 0.12f);
  mDelaySmp.snap(mTimeMs * 0.001f * sr);
  mDry.prepare(sr, 0.03f); mWet.prepare(sr, 0.03f); mFb.prepare(sr, 0.05f);
  mDry.snap(mDry.target()); mWet.snap(mWet.target()); mFb.snap(mFb.target());
  mFiltersDirty = true;
  reset();
}

void Delay::reset() noexcept
{
  mSnap = true;
  for (auto& l : mLine) l.reset();
  for (auto& f : mLp) f.reset();
  for (auto& f : mHp) f.reset();
  mWow.reset(0.f);
  mFlutter.reset(0.3f);
  mMod.reset(0.6f);
}

void Delay::update(const ParamValues& p) noexcept
{
  mMode = static_cast<int>(p[kDlyMode]);
  mSync = p[kDlySync] >= 0.5f;
  mDivision = static_cast<int>(p[kDlyDivision]);
  mFreeTimeMs = p[kDlyTime];
  mFb.setTarget(std::min(0.98f, p[kDlyFeedback] * 0.01f));
  const float mix = p[kDlyMix] * 0.01f;
  mDry.setTarget(std::min(1.f, 2.f * (1.f - mix)));
  mWet.setTarget(std::min(1.f, 2.f * mix));
  mSat = p[kDlySaturation] * 0.1f;
  mModDepth = p[kDlyModDepth] * 0.01f;
  mModRate = p[kDlyModRate];
  mWidth = p[kDlyWidth] * 0.01f;
  mLowCut = p[kDlyLowCut];
  mHighCut = p[kDlyHighCut];
  mFiltersDirty = true; // срезы и режим влияют на фильтры контура
  mHostTempo = p[kHostTempo] >= 0.5f;
  mManualTempo = p[kTempo];
}

void Delay::designFilters() noexcept
{
  double lp = mHighCut;
  if (mMode == 1) lp = std::min(lp, 5500.0);      // плёнка
  else if (mMode == 2) lp = std::min(lp, 3000.0); // BBD
  const auto lpc = design::make(FilterType::LowPass, mSampleRate, lp, mMode == 2 ? 0.9 : 0.707);
  const auto hpc = design::make(FilterType::HighPass, mSampleRate, mLowCut, 0.707);
  for (int c = 0; c < 2; ++c) { mLp[c].setCoefs(lpc); mHp[c].setCoefs(hpc); }
  mFiltersDirty = false;
}

void Delay::process(AudioBlock& b) noexcept
{
  const int n = b.numFrames;
  const float sr = static_cast<float>(mSampleRate);
  if (mFiltersDirty) designFilters();

  const double tempo = (mHostTempo && b.transport && b.transport->hostTempoValid && b.transport->tempo > 1.0) ? b.transport->tempo : mManualTempo;
  mTimeMs = mSync ? static_cast<float>(divisionBeats(mDivision) * 60000.0 / std::max(20.0, tempo)) : mFreeTimeMs;
  mTimeMs = clampv(mTimeMs, 1.f, 2000.f);
  mDelaySmp.setTarget(mTimeMs * 0.001f * sr);
  if (mSnap)
  {
    // Первый блок после prepare/reset: без «глиссандо» от прежнего времени
    mDelaySmp.snap(mDelaySmp.target());
    mSnap = false;
  }

  const bool inStereo = b.stereo;
  const float maxD = static_cast<float>(mLine[0].maxDelay() - 4);
  auto loop = [&](float y, int c) {
    y = mHp[c].process(mLp[c].process(y));
    if (mSat > 0.f || mMode == 1 || mMode == 2)
    {
      const float drive = 1.f + mSat * 2.5f + (mMode == 1 ? 0.6f : 0.f);
      y = fastTanh(y * drive) / drive;
    }
    return 1.5f * fastTanh(y / 1.5f); // аварийное ограничение контура
  };

  for (int i = 0; i < n; ++i)
  {
    const float d = mDelaySmp.next();
    float mf = 1.f;
    switch (mMode)
    {
      case 1: mf = 1.f + mModDepth * (0.004f * mWow.next(0.45f * (0.5f + mModRate), sr) + 0.0012f * mFlutter.next(6.3f, sr)); break;
      case 2: mf = 1.f + mModDepth * 0.003f * mMod.next(mModRate, sr); break;
      default: mf = 1.f + mModDepth * 0.0015f * mMod.next(mModRate, sr); break;
    }
    const float inL = b.l[i];
    const float inR = inStereo ? b.r[i] : inL;
    const float inM = 0.5f * (inL + inR);
    const float fb = mFb.next();
    float wl = 0.f, wr = 0.f;

    if (mMode == 3) // Ping-Pong
    {
      const float dd = clampv(d * mf, 2.f, maxD);
      const float yl = mLine[0].readCubic(dd), yr = mLine[1].readCubic(dd);
      mLine[0].push(inM + fb * loop(yr, 1));
      mLine[1].push(fb * loop(yl, 0));
      const float mid = 0.5f * (yl + yr), side = 0.5f * (yl - yr) * (0.3f + 0.7f * mWidth) * 1.4f;
      wl = mid + side;
      wr = mid - side;
    }
    else if (mMode == 4) // Multi-Tap
    {
      static constexpr float kGain[4] = { 0.5f, 0.65f, 0.8f, 1.0f };
      static constexpr float kPan[4] = { -0.7f, 0.6f, -0.3f, 0.25f };
      float last = 0.f;
      for (int k = 0; k < 4; ++k)
      {
        const float tap = mLine[0].readCubic(clampv(d * mf * (k + 1) * 0.25f, 2.f, maxD));
        const float pan = kPan[k] * mWidth;
        wl += kGain[k] * tap * std::min(1.f, 1.f - pan);
        wr += kGain[k] * tap * std::min(1.f, 1.f + pan);
        last = tap;
      }
      wl *= 0.6f;
      wr *= 0.6f;
      mLine[0].push(inM + fb * loop(last, 0));
    }
    else // Digital / Tape / Analog
    {
      const float dl = clampv(d * mf, 2.f, maxD);
      const float dr = clampv(d * mf * (1.f + 0.06f * mWidth), 2.f, maxD);
      const float yl = mLine[0].readCubic(dl), yr = mLine[1].readCubic(dr);
      mLine[0].push(inL + fb * loop(yl, 0));
      mLine[1].push(inR + fb * loop(yr, 1));
      wl = yl;
      wr = yr;
    }
    const float dry = mDry.next(), wet = mWet.next();
    b.l[i] = inL * dry + wl * wet;
    b.r[i] = inR * dry + wr * wet;
  }
  b.stereo = true;
}

} // namespace tsl
