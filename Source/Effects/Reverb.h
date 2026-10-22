// SPDX-License-Identifier: GPL-3.0-or-later
// Алгоритмическая реверберация (собственная реализация):
//  * Room / Hall / Plate / Shimmer — FDN из 8 линий с матрицей Хаусхолдера, затухание по RT60,
//    демпфирование ВЧ в каждой линии, модуляция длин, входная диффузия, ранние отражения;
//  * Spring — дисперсионные цепочки allpass внутри коротких контуров ОС (характерный «чирп»);
//  * Shimmer — сдвиг высоты на октаву вверх в контуре подпитки;
//  * Freeze — бесконечное сустейн-состояние (без потерь, вход отключён).
// Это алгоритмическая модель, а не измеренные помещения.
#pragma once

#include "../Audio/Module.h"
#include "../DSP/Biquad.h"
#include "../DSP/DelayLine.h"
#include "../DSP/GrainPitchShifter.h"
#include "../DSP/Lfo.h"
#include "../DSP/Smoother.h"

namespace tsl {

class Reverb final : public Module
{
public:
  static constexpr int kLines = 8;

  Reverb() noexcept : Module(ModuleId::Reverb) {}

  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;
  bool hasTail() const noexcept override { return true; }

private:
  struct Allpass
  {
    DelayLine line;
    int delay = 100;
    float g = 0.6f;
    inline float process(float x) noexcept
    {
      const float d = line.readInt(delay);
      const float v = x + g * d;
      line.push(v);
      return d - g * v;
    }
  };

  void recalc() noexcept;
  float processSpring(float in, float& outL, float& outR) noexcept;

  int mMode = 1;
  float mDecay = 2.5f, mSize = 0.6f, mPreDelayMs = 20.f, mEarly = 0.4f, mDiffusion = 0.75f;
  float mHighDamp = 6000.f, mLowCut = 120.f, mModulation = 0.3f, mWidth = 1.f, mShimmer = 0.4f;
  bool mFreeze = false;
  bool mDirty = true;

  DelayLine mPre;
  DelayLine mLines[kLines];
  float mLineLen[kLines]{};
  float mGain[kLines]{};
  float mDampState[kLines]{};
  float mDampA = 0.f;
  Lfo mLfo[4];
  Allpass mDiff[4];
  Biquad mInHp, mInLp;
  DelayLine mEr;
  float mErTap[8]{};

  // Spring
  struct Spring
  {
    Allpass disp[10];
    DelayLine loop;
    int len = 1500;
    float fb = 0.6f, lp = 0.f, hp = 0.f;
  } mSpring[3];

  GrainPitchShifter mShift;
  Biquad mShiftHp;
  float mShimmerFb = 0.f;

  LinearSmoother mDry, mWet, mInGain;
};

} // namespace tsl
