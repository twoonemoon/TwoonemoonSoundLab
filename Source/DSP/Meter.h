// SPDX-License-Identifier: GPL-3.0-or-later
// Peak/RMS-измеритель уровня. Audio thread пишет атомики, UI читает без блокировок.
#pragma once

#include "DspCommon.h"
#include <atomic>

namespace tsl {

class LevelMeter
{
public:
  void prepare(float sampleRate) noexcept
  {
    mRmsCoef = onePoleCoef(0.300f, sampleRate);  // интеграция RMS ~300 мс
    mPeakRelease = onePoleCoef(0.600f, sampleRate);
    reset();
  }
  void reset() noexcept
  {
    mRmsState = 0.0f;
    mPeakState = 0.0f;
    mPeak.store(0.0f, std::memory_order_relaxed);
    mRms.store(0.0f, std::memory_order_relaxed);
  }

  /** Обработать блок (один канал или максимум по каналам — вызывающий решает). */
  void process(const float* buf, int n) noexcept
  {
    float peak = mPeakState, rms = mRmsState;
    float blockMax = 0.0f;
    for (int i = 0; i < n; ++i)
    {
      const float a = std::fabs(buf[i]);
      blockMax = std::max(blockMax, a);
      peak = a > peak ? a : peak * mPeakRelease;
      rms = a * a + mRmsCoef * (rms - a * a);
    }
    mPeakState = undenormal(peak);
    mRmsState = undenormal(rms);
    mPeak.store(mPeakState, std::memory_order_relaxed);
    mRms.store(std::sqrt(mRmsState), std::memory_order_relaxed);
    if (blockMax >= 1.0f) mClip.store(true, std::memory_order_relaxed);
  }

  void processStereo(const float* l, const float* r, int n) noexcept
  {
    float peak = mPeakState, rms = mRmsState;
    float blockMax = 0.0f;
    for (int i = 0; i < n; ++i)
    {
      const float a = std::max(std::fabs(l[i]), std::fabs(r[i]));
      blockMax = std::max(blockMax, a);
      peak = a > peak ? a : peak * mPeakRelease;
      rms = a * a + mRmsCoef * (rms - a * a);
    }
    mPeakState = undenormal(peak);
    mRmsState = undenormal(rms);
    mPeak.store(mPeakState, std::memory_order_relaxed);
    mRms.store(std::sqrt(mRmsState), std::memory_order_relaxed);
    if (blockMax >= 1.0f) mClip.store(true, std::memory_order_relaxed);
  }

  float peak() const noexcept { return mPeak.load(std::memory_order_relaxed); }
  float rms() const noexcept { return mRms.load(std::memory_order_relaxed); }
  bool clipped() const noexcept { return mClip.load(std::memory_order_relaxed); }
  void clearClip() noexcept { mClip.store(false, std::memory_order_relaxed); }

private:
  float mRmsCoef = 0.0f, mPeakRelease = 0.0f;
  float mRmsState = 0.0f, mPeakState = 0.0f;
  std::atomic<float> mPeak{ 0.0f }, mRms{ 0.0f };
  std::atomic<bool> mClip{ false };
};

} // namespace tsl
