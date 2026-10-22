// SPDX-License-Identifier: GPL-3.0-or-later
// Wah, Octave, Pitch Shifter, Harmonizer.
//
// Задержка:
//  * Wah и Octave (режим Mono analog) — без задержки.
//  * Octave Poly, Pitch и голоса Harmonizer используют сдвиг высоты на линии задержки с перекрёстно
//    затухающими головками: голос запаздывает примерно на половину окна (≈ 20 мс в Live, ≈ 40 мс в Smooth).
//    Это не добавляет задержку плагину (прямой сигнал не задерживается), но слышно в самих голосах —
//    фактическая задержка показывается в интерфейсе.
//  * Harmonizer рассчитан на одноголосные линии: высота определяется по чистому DI-сигналу; для аккордов
//    независимое гармонизирование каждой ноты не выполняется (используется последняя уверенно определённая нота).
#pragma once

#include "../Audio/Module.h"
#include "../DSP/Biquad.h"
#include "../DSP/DelayLine.h"
#include "../DSP/GrainPitchShifter.h"
#include "../DSP/PitchDetector.h"
#include "../DSP/Smoother.h"
#include "../DSP/Svf.h"

#include <atomic>

namespace tsl {

/** Сдвиг в полутонах для ступенчатого интервала harmonizer'а от ноты midi в тональности key/scale. */
int harmonyShiftSemitones(int midiNote, int key, int scale, int intervalIndex) noexcept;

class Wah final : public Module
{
public:
  Wah() noexcept : Module(ModuleId::Wah) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;
  /** Текущее положение педали 0…1 (для анимации в UI). */
  float position() const noexcept { return mShownPos.load(std::memory_order_relaxed); }

private:
  Svf mSvf[2];
  int mMode = 0;
  float mQ = 6.f, mSens = 0.5f;
  OnePoleSmoother mPos;
  float mEnv = 0.f, mAtt = 0.f, mRel = 0.f;
  LinearSmoother mMix;
  std::atomic<float> mShownPos{ 0.5f };
};

class Octave final : public Module
{
public:
  Octave() noexcept : Module(ModuleId::Octave) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

private:
  int mMode = 0;
  // Mono (analog): детектор пересечений нуля и делители
  Biquad mTrackLp, mTrackLp2, mToneLp, mUpHp;
  bool mFf1 = false, mFf2 = false, mArmed = true;
  float mEnv = 0.f, mEnvA = 0.f, mEnvR = 0.f;
  // Poly
  GrainPitchShifter mDown1, mDown2, mUp;
  LinearSmoother mSub1, mSub2, mUpL, mDry;
};

class PitchShift final : public Module
{
public:
  PitchShift() noexcept : Module(ModuleId::Pitch) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;
  /** Задержка сдвинутого голоса (мс). */
  float voiceLatencyMs() const noexcept { return mQuality == 0 ? 20.f : 40.f; }

private:
  GrainPitchShifter mShift[2][2]; // [качество][канал]
  int mQuality = 0;
  float mSemis = 12.f, mFine = 0.f;
  LinearSmoother mMix;
};

class Harmonizer final : public Module
{
public:
  static constexpr int kVoices = 4;
  Harmonizer() noexcept : Module(ModuleId::Harmonizer) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

  /** Последняя определённая нота (MIDI) или −1. Для UI. */
  int detectedNote() const noexcept { return mShownNote.load(std::memory_order_relaxed); }
  float voiceLatencyMs() const noexcept { return 20.f; }

private:
  struct Voice
  {
    GrainPitchShifter shift;
    DelayLine delay;
    OnePoleSmoother ratio;
    LinearSmoother level, delaySmp;
    bool on = false;
    int interval = 7;
    float detune = 0.f, pan = 0.f;
  };

  void retarget() noexcept;

  PitchDetector mDetector;
  int mSinceEstimate = 0;
  int mNote = -1;
  int mKey = 9, mScale = 1;
  Voice mVoice[kVoices];
  LinearSmoother mDry, mVoicesLevel;
  std::atomic<int> mShownNote{ -1 };
};

} // namespace tsl
