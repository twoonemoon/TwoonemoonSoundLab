// SPDX-License-Identifier: GPL-3.0-or-later
// Модуль усилителя: предусилитель (каскады триодов с динамическим смещением от
// сеточного тока), пассивный tone stack, катодный повторитель, двухтактный
// оконечный каскад (класс AB, Bias управляет переходными искажениями), проседание
// питания (Sag), петля ООС с Presence/Depth, выходной трансформатор.
// Акустические модели — отдельный чистый тракт (пьезо-коррекция, резонансы корпуса, Room).
#pragma once

#include "../Audio/Module.h"
#include "../DSP/Biquad.h"
#include "../DSP/DelayLine.h"
#include "../DSP/Oversampler.h"
#include "../DSP/Smoother.h"
#include "AmpVoices.h"
#include "ToneStack.h"

#include <atomic>

namespace tsl {

class Amp final : public Module
{
public:
  Amp() noexcept : Module(ModuleId::Amp) {}

  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

  int activeModel() const noexcept { return mModel; }
  /** Групповая задержка оверсэмплинга на НЧ (отсчёты, для документации/тестов; не для audio thread). */
  float oversamplingGroupDelay() const { return mCh[0].os.groupDelaySamples(); }

  /** Уровень сигнала на входе оконечного каскада (для индикатора «насыщения» в UI). */
  float powerDrive() const noexcept { return mPowerDrive.load(std::memory_order_relaxed); }

private:
  struct StageState
  {
    float coupling = 0.f, cathode = 0.f, miller = 0.f, gridEnv = 0.f;
  };

  struct Channel
  {
    Oversampler os;
    Biquad inHp, bright, lowCut, highCut;
    StageState st[4];
    ToneStack ts;
    // Оконечный каскад
    float fbPrev = 0.f, presLp = 0.f, depthLp1 = 0.f, sagEnv = 0.f, xfHp = 0.f, xfLp = 0.f, xfLp2 = 0.f;
    // Акустика
    Biquad body[3], piezoNotch, piezoShelf, warmth, eqBass, eqMid, eqTreble, air;
    float transientEnv = 0.f;
    DelayLine room;
    void resetState() noexcept;
  };

  void applyModel(int model) noexcept;
  void recalcCoefs() noexcept;
  void updateTargets() noexcept;
  void processElectric(Channel& c, float* x, int n) noexcept;
  void processAcoustic(Channel& c, float* x, float* roomOutL, float* roomOutR, int n, bool firstChannel) noexcept;

  Channel mCh[2];
  int mModel = 4, mPendingModel = 4;
  int mOsFactor = 4, mPendingOsFactor = 4;
  bool mSwitching = false;
  LinearSmoother mSwitchFade;

  // Положения ручек (сырые) и сглаживание
  float mKnobGain = 5.f, mKnobMaster = 5.f, mLevelDb = 0.f;
  float mBass = 0.5f, mMid = 0.5f, mTreble = 0.5f;
  OnePoleSmoother mBassS, mMidS, mTrebleS;
  float mTsBass = -1.f, mTsMid = -1.f, mTsTreble = -1.f;
  float mPresence = 0.5f, mDepth = 0.5f, mSagKnob = 5.f, mBiasKnob = 5.f, mFbKnob = 5.f;
  bool mBright = false;
  float mLowCut = 20.f, mHighCut = 20000.f;
  float mAcBody = 5.f, mAcTame = 3.f, mAcAir = 3.f, mAcRoom = 2.f;
  bool mCoefsDirty = true;

  LinearSmoother mGain, mMaster, mOutGain, mRoomMix;
  float mBlockGain = 1.f, mBlockMaster = 1.f, mBlockOut = 1.f;

  // Производные коэффициенты (общие для каналов)
  float mStageA_coupling[4]{}, mStageA_cathode[4]{}, mStageA_miller[4]{};
  float mGridAttack = 0.f, mGridRelease[4]{};
  float mPaB = 0.25f, mPaA = 8.f, mPaNorm = 1.f;
  float mNfbK = 0.3f, mPresAmt = 0.5f, mDepthAmt = 0.5f;
  float mPresA = 0.f, mDepthA = 0.f, mSagAmt = 0.3f, mSagAtt = 0.f, mSagRel = 0.f;
  float mXfHpA = 0.f, mXfLpA = 0.f;
  float mTsMakeup = 1.f;
  float mTransientAtt = 0.f, mTransientRel = 0.f;
  std::atomic<float> mPowerDrive{ 0.f };

  std::vector<float> mRoomL, mRoomR;
};

} // namespace tsl
