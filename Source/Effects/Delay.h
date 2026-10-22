// SPDX-License-Identifier: GPL-3.0-or-later
// Delay: пять режимов — Digital, Tape (wow/flutter, насыщение, потеря ВЧ), Analog
// (тёмный BBD-подобный контур), Ping-Pong (перекрёстная ОС), Multi-Tap (4 отвода).
// Синхронизация с темпом хоста или ручным, плавная смена времени (без щелчков),
// ограничение в контуре ОС — устойчивость при любых допустимых настройках.
#pragma once

#include "../Audio/Module.h"
#include "../DSP/Biquad.h"
#include "../DSP/DelayLine.h"
#include "../DSP/Lfo.h"
#include "../DSP/Smoother.h"

namespace tsl {

class Delay final : public Module
{
public:
  static constexpr float kMaxSeconds = 2.2f;

  Delay() noexcept : Module(ModuleId::Delay) {}

  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;
  bool hasTail() const noexcept override { return true; }

  /** Текущее время задержки (мс) с учётом синхронизации — для UI. */
  float currentTimeMs() const noexcept { return mTimeMs; }

private:
  void designFilters() noexcept;

  int mMode = 0;
  bool mSync = false;
  int mDivision = 5;
  float mFreeTimeMs = 380.f, mTimeMs = 380.f;
  float mSat = 0.2f, mModDepth = 0.15f, mModRate = 0.6f, mWidth = 0.6f;
  float mLowCut = 80.f, mHighCut = 7000.f;
  bool mFiltersDirty = true;
  bool mSnap = true;
  bool mHostTempo = true;
  double mManualTempo = 120.0;

  DelayLine mLine[2];
  OnePoleSmoother mDelaySmp;
  LinearSmoother mDry, mWet, mFb;
  Biquad mLp[2], mHp[2];
  Lfo mWow, mFlutter, mMod;
};

} // namespace tsl
