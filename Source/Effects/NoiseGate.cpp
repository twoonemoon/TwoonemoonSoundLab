// SPDX-License-Identifier: GPL-3.0-or-later
#include "NoiseGate.h"

namespace tsl {

void NoiseGate::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  mKeyHpf.setCoefs(design::make(FilterType::HighPass, mSampleRate, 40.0, 0.707));
  recalc();
  reset();
}

void NoiseGate::reset() noexcept
{
  mState = State::Closed;
  mEnv = 0.f;
  mGain = mFloor;
  mHoldCounter = 0;
  mKeyHpf.reset();
  mGrDb.store(0.f, std::memory_order_relaxed);
}

void NoiseGate::update(const ParamValues& p) noexcept
{
  const float thr = p[kGateThreshold], hyst = p[kGateHysteresis], range = p[kGateRange];
  const float att = p[kGateAttack], hold = p[kGateHold], rel = p[kGateRelease];
  const bool key = p[kGateKey] >= 0.5f;
  if (thr != mThresholdDb || hyst != mHysteresisDb || range != mRangeDb || att != mAttackMs || hold != mHoldMs ||
      rel != mReleaseMs || key != mUseKey)
  {
    mThresholdDb = thr; mHysteresisDb = hyst; mRangeDb = range;
    mAttackMs = att; mHoldMs = hold; mReleaseMs = rel; mUseKey = key;
    recalc();
  }
}

void NoiseGate::recalc() noexcept
{
  const float sr = static_cast<float>(mSampleRate);
  mOpenThr = dbToGain(mThresholdDb);
  mCloseThr = dbToGain(mThresholdDb - mHysteresisDb);
  mFloor = mRangeDb <= -95.f ? 0.f : dbToGain(mRangeDb);
  mAttackStep = 1.0f / std::max(1.0f, mAttackMs * 0.001f * sr);
  mHoldSamples = static_cast<int>(mHoldMs * 0.001f * sr);
  // Release: спад на 60 дБ за время release (экспоненциально, естественное затухание)
  mReleaseMul = std::pow(10.0f, -3.0f / std::max(1.0f, mReleaseMs * 0.001f * sr));
  // Детектор: мгновенная атака, спад ~ 8 мс (сглаживает период низких нот, не «дребезжит»)
  mEnvRelease = onePoleCoef(0.008f, sr);
}

void NoiseGate::process(AudioBlock& b) noexcept
{
  const int n = b.numFrames;
  const float* det = (mUseKey && b.keyInput) ? b.keyInput : b.l;
  float env = mEnv, gain = mGain;
  float minGain = 1.0f;

  for (int i = 0; i < n; ++i)
  {
    float x = det[i];
    if (!mUseKey && b.stereo) x = std::fabs(x) > std::fabs(b.r[i]) ? x : b.r[i];
    const float a = std::fabs(mKeyHpf.process(x));
    env = a > env ? a : env * mEnvRelease;

    switch (mState)
    {
      case State::Closed:
      case State::Release:
        if (env > mOpenThr) mState = State::Attack;
        break;
      case State::Attack:
      case State::Open:
        if (env < mCloseThr) { mState = State::Hold; mHoldCounter = mHoldSamples; }
        break;
      case State::Hold:
        if (env > mCloseThr) mState = gain >= 1.0f ? State::Open : State::Attack;
        else if (--mHoldCounter <= 0) mState = State::Release;
        break;
    }

    switch (mState)
    {
      case State::Attack:
        gain += mAttackStep;
        if (gain >= 1.0f) { gain = 1.0f; mState = State::Open; }
        break;
      case State::Release:
        gain *= mReleaseMul;
        if (gain <= mFloor + 1e-6f) { gain = mFloor; mState = State::Closed; }
        break;
      case State::Hold:
        // в hold сохраняем текущее усиление (если атака не завершена — продолжаем её)
        if (gain < 1.0f) gain = std::min(1.0f, gain + mAttackStep);
        break;
      default: break;
    }
    gain = std::max(gain, mFloor);

    b.l[i] *= gain;
    if (b.stereo) b.r[i] *= gain;
    minGain = std::min(minGain, gain);
  }
  mEnv = undenormal(env);
  mGain = gain;
  mGrDb.store(gainToDb(minGain, -96.f), std::memory_order_relaxed);
}

} // namespace tsl
