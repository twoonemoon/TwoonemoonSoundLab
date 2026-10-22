// SPDX-License-Identifier: GPL-3.0-or-later
// Noise Gate для high-gain: гистерезис, hold, атака/release, диапазон подавления,
// ключ от локального сигнала или от «чистого» DI-входа (гейт после усилителя,
// управляемый динамикой исходной гитары — мягкие ноты не обрываются дисторшном).
#pragma once

#include "../Audio/Module.h"
#include "../DSP/Biquad.h"
#include <atomic>

namespace tsl {

class NoiseGate final : public Module
{
public:
  NoiseGate() noexcept : Module(ModuleId::Gate) {}

  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

  /** Текущее ослабление (дБ, ≤ 0) — для индикатора в UI. */
  float gainReductionDb() const noexcept { return mGrDb.load(std::memory_order_relaxed); }
  bool isOpen() const noexcept { return mState != State::Closed && mState != State::Release; }

  // Прямой доступ к настройкам для тестов
  void setThresholdDb(float t) noexcept { mThresholdDb = t; recalc(); }
  void setRangeDb(float r) noexcept { mRangeDb = r; recalc(); }

private:
  enum class State { Closed, Attack, Open, Hold, Release };

  void recalc() noexcept;

  float mThresholdDb = -62.f, mHysteresisDb = 6.f, mRangeDb = -90.f;
  float mAttackMs = 0.5f, mHoldMs = 30.f, mReleaseMs = 120.f;
  bool mUseKey = false;

  // Производные
  float mOpenThr = 0.f, mCloseThr = 0.f, mFloor = 0.f;
  float mAttackStep = 1.f, mReleaseMul = 0.f, mEnvRelease = 0.f;
  int mHoldSamples = 0;

  // Состояние
  State mState = State::Closed;
  float mEnv = 0.f, mGain = 0.f;
  int mHoldCounter = 0;
  Biquad mKeyHpf; // убирает инфранизкие помехи из детектора
  std::atomic<float> mGrDb{ 0.f };
};

} // namespace tsl
