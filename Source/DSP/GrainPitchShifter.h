// SPDX-License-Identifier: GPL-3.0-or-later
// Простой двухголовочный pitch shifter на линии задержки (перекрёстно затухающие головки).
// Задержка ≈ половина окна; используется в контуре Shimmer-реверберации, где задержка не критична.
#pragma once

#include "DelayLine.h"

namespace tsl {

class GrainPitchShifter
{
public:
  void prepare(double sampleRate, float windowMs = 60.f)
  {
    mWindow = static_cast<float>(sampleRate * windowMs * 0.001);
    mLine.prepare(static_cast<int>(mWindow) + 8);
    reset();
  }
  void reset() noexcept
  {
    mLine.reset();
    mPhase = 0.f;
  }
  void setSemitones(float st) noexcept { mRatio = std::pow(2.f, st / 12.f); }

  inline float process(float x) noexcept
  {
    mLine.push(x);
    // Фаза движется со скоростью (1 − ratio): для повышения задержка уменьшается
    mPhase += (1.f - mRatio) / mWindow;
    mPhase -= std::floor(mPhase);
    const float p2 = mPhase + 0.5f - std::floor(mPhase + 0.5f);
    const float d1 = 1.f + mPhase * (mWindow - 2.f);
    const float d2 = 1.f + p2 * (mWindow - 2.f);
    const float w1 = std::sin(kPi * mPhase);
    const float w2 = std::sin(kPi * p2);
    return w1 * w1 * mLine.readLinear(d1) + w2 * w2 * mLine.readLinear(d2);
  }

private:
  DelayLine mLine;
  float mWindow = 2880.f, mPhase = 0.f, mRatio = 2.f;
};

} // namespace tsl
