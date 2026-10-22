// SPDX-License-Identifier: GPL-3.0-or-later
// Линия задержки с дробным чтением (линейная и кубическая Эрмита интерполяция).
// Буфер выделяется в prepare(), размер — степень двойки.
#pragma once

#include "DspCommon.h"
#include <vector>

namespace tsl {

class DelayLine
{
public:
  /** Не для audio thread. */
  void prepare(int maxDelaySamples)
  {
    int size = 4;
    while (size < maxDelaySamples + 4) size <<= 1;
    mBuf.assign(static_cast<size_t>(size), 0.0f);
    mMask = size - 1;
    mWrite = 0;
  }
  void reset() noexcept { std::fill(mBuf.begin(), mBuf.end(), 0.0f); mWrite = 0; }
  int maxDelay() const noexcept { return mMask - 3; }

  inline void push(float x) noexcept
  {
    mBuf[static_cast<size_t>(mWrite)] = x;
    mWrite = (mWrite + 1) & mMask;
  }

  /** Целая задержка d ≥ 1 (1 = последний записанный отсчёт). */
  inline float readInt(int d) const noexcept
  {
    return mBuf[static_cast<size_t>((mWrite - d) & mMask)];
  }

  /** Линейная интерполяция, d ≥ 1. */
  inline float readLinear(float d) const noexcept
  {
    d = clampv(d, 1.0f, static_cast<float>(maxDelay()));
    const int i = static_cast<int>(d);
    const float f = d - static_cast<float>(i);
    const float a = readInt(i), b = readInt(i + 1);
    return a + (b - a) * f;
  }

  /** Кубическая интерполяция Эрмита (4 точки), d ≥ 1. */
  inline float readCubic(float d) const noexcept
  {
    d = clampv(d, 2.0f, static_cast<float>(maxDelay() - 1));
    const int i = static_cast<int>(d);
    const float f = d - static_cast<float>(i);
    const float xm1 = readInt(i - 1), x0 = readInt(i), x1 = readInt(i + 1), x2 = readInt(i + 2);
    const float c0 = x0;
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + c0;
  }

private:
  std::vector<float> mBuf;
  int mMask = 0;
  int mWrite = 0;
};

} // namespace tsl
