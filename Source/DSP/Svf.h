// SPDX-License-Identifier: GPL-3.0-or-later
// State-variable filter в топологии TPT (trapezoidal integration) — устойчив
// при быстрой модуляции частоты (wah, envelope filter).
#pragma once

#include "DspCommon.h"

namespace tsl {

class Svf
{
public:
  struct Out
  {
    float lp, bp, hp;
  };

  void reset() noexcept { mIc1 = mIc2 = 0.0f; }

  /** Установить частоту и резонанс (Q). Можно вызывать каждый отсчёт. */
  void set(float freq, float q, float sampleRate) noexcept
  {
    const float f = clampv(freq, 5.0f, sampleRate * 0.45f);
    mG = std::tan(kPi * f / sampleRate);
    mK = 1.0f / std::max(q, 0.05f);
    mA1 = 1.0f / (1.0f + mG * (mG + mK));
  }

  inline Out process(float x) noexcept
  {
    const float v3 = x - mIc2;
    const float v1 = mA1 * (mIc1 + mG * v3);
    const float v2 = mIc2 + mG * v1;
    mIc1 = undenormal(2.0f * v1 - mIc1);
    mIc2 = undenormal(2.0f * v2 - mIc2);
    return { v2, v1, x - mK * v1 - v2 };
  }

  float k() const noexcept { return mK; }

private:
  float mG = 0.0f, mK = 1.0f, mA1 = 1.0f;
  float mIc1 = 0.0f, mIc2 = 0.0f;
};

} // namespace tsl
