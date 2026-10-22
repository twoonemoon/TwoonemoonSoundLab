// SPDX-License-Identifier: GPL-3.0-or-later
// Сглаживание параметров без zipper noise.
#pragma once

#include "DspCommon.h"

namespace tsl {

/** Линейная рампа к цели за фиксированное время. Точно приходит в цель — удобно для gain/bypass. */
class LinearSmoother
{
public:
  void prepare(float sampleRate, float rampSeconds) noexcept
  {
    mRampSamples = std::max(1, static_cast<int>(sampleRate * rampSeconds));
    snap(mTarget);
  }
  void snap(float v) noexcept
  {
    mCurrent = mTarget = v;
    mRemaining = 0;
    mStep = 0.0f;
  }
  void setTarget(float v) noexcept
  {
    if (v == mTarget) return;
    mTarget = v;
    mRemaining = mRampSamples;
    mStep = (mTarget - mCurrent) / static_cast<float>(mRemaining);
  }
  float next() noexcept
  {
    if (mRemaining > 0)
    {
      --mRemaining;
      mCurrent = mRemaining == 0 ? mTarget : mCurrent + mStep;
    }
    return mCurrent;
  }
  /** Пропустить n отсчётов (когда значение используется раз за блок). */
  float skip(int n) noexcept
  {
    if (mRemaining > 0)
    {
      if (n >= mRemaining) { mCurrent = mTarget; mRemaining = 0; }
      else { mCurrent += mStep * static_cast<float>(n); mRemaining -= n; }
    }
    return mCurrent;
  }
  bool isSmoothing() const noexcept { return mRemaining > 0; }
  float current() const noexcept { return mCurrent; }
  float target() const noexcept { return mTarget; }

private:
  float mCurrent = 0.0f, mTarget = 0.0f, mStep = 0.0f;
  int mRampSamples = 64, mRemaining = 0;
};

/** Однополюсное (экспоненциальное) сглаживание — для частот, Q, drive. */
class OnePoleSmoother
{
public:
  void prepare(float sampleRate, float timeSeconds) noexcept { mCoef = onePoleCoef(timeSeconds, sampleRate); }
  void snap(float v) noexcept { mCurrent = mTarget = v; }
  void setTarget(float v) noexcept { mTarget = v; }
  float next() noexcept
  {
    mCurrent = mTarget + mCoef * (mCurrent - mTarget);
    if (std::fabs(mCurrent - mTarget) < 1.0e-7f) mCurrent = mTarget;
    return mCurrent;
  }
  float current() const noexcept { return mCurrent; }
  float target() const noexcept { return mTarget; }
  bool isSmoothing() const noexcept { return mCurrent != mTarget; }

private:
  float mCurrent = 0.0f, mTarget = 0.0f, mCoef = 0.0f;
};

} // namespace tsl
