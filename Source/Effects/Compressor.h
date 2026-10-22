// SPDX-License-Identifier: GPL-3.0-or-later
// Feed-forward компрессор: вычислитель усиления в лог-домене с мягким коленом,
// сглаженный детектор (smooth branching), sidechain HPF, параллельный Mix,
// индикатор gain reduction. Без lookahead → нулевая добавленная задержка.
#pragma once

#include "../Audio/Module.h"
#include "../DSP/Biquad.h"
#include "../DSP/Smoother.h"
#include <atomic>

namespace tsl {

class Compressor final : public Module
{
public:
  Compressor() noexcept : Module(ModuleId::Compressor) {}

  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

  float gainReductionDb() const noexcept { return mGrDb.load(std::memory_order_relaxed); }

  /** Статическая характеристика (для UI-графика и тестов): вход дБ → выход дБ (без makeup). */
  static float staticCurve(float inDb, float thresholdDb, float ratio, float kneeDb) noexcept;

private:
  float mThreshold = -24.f, mRatio = 4.f, mAttackMs = 8.f, mReleaseMs = 160.f, mKnee = 6.f;
  float mScHpfHz = 20.f;
  float mAttCoef = 0.f, mRelCoef = 0.f;
  LinearSmoother mMakeup, mMix;
  Biquad mScHpfL, mScHpfR;
  float mGrState = 0.f; // текущее ослабление, дБ (≥0)
  std::atomic<float> mGrDb{ 0.f };
};

} // namespace tsl
