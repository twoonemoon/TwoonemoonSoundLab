// SPDX-License-Identifier: GPL-3.0-or-later
// Утилиты: Stereo Widener (с контролем моносовместимости), Output Limiter (без lookahead),
// Looper (запись/воспроизведение/наложение), метроном (генератор щелчков для движка).
#pragma once

#include "../Audio/Module.h"
#include "../DSP/Biquad.h"
#include "../DSP/Smoother.h"

#include <atomic>
#include <vector>

namespace tsl {

class Widener final : public Module
{
public:
  Widener() noexcept : Module(ModuleId::Widener) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;
  /** Корреляция каналов на выходе (−1…+1): < 0 — риск потерь при сложении в моно. */
  float correlation() const noexcept { return mCorr.load(std::memory_order_relaxed); }

private:
  LinearSmoother mWidth;
  Biquad mSideHp[2]; // 4-й порядок (Баттерворт), ступени последовательно
  float mMonoHz = 120.f;
  double mLR = 0.0, mLL = 0.0, mRR = 0.0;
  std::atomic<float> mCorr{ 1.f };
};

class Limiter final : public Module
{
public:
  Limiter() noexcept : Module(ModuleId::Limiter) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;
  float gainReductionDb() const noexcept { return mGr.load(std::memory_order_relaxed); }

private:
  float mCeiling = 0.89f, mRelease = 0.f, mAttack = 0.f, mEnv = 0.f;
  std::atomic<float> mGr{ 0.f };
};

class Looper final : public Module
{
public:
  static constexpr double kMaxSeconds = 30.0;
  enum class State : int { Empty, Recording, Playing, Overdubbing, Stopped };
  enum class Command : int { None, RecordToggle, PlayStop, Overdub, Clear };

  Looper() noexcept : Module(ModuleId::Looper) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

  /** UI: команда (выполняется в начале следующего блока). */
  void command(Command c) noexcept { mCommand.store(static_cast<int>(c), std::memory_order_release); }
  State state() const noexcept { return static_cast<State>(mStateShown.load(std::memory_order_relaxed)); }
  /** Позиция воспроизведения 0…1 и длина петли в секундах (для UI). */
  float position() const noexcept { return mPosShown.load(std::memory_order_relaxed); }
  float lengthSeconds() const noexcept { return mLenShown.load(std::memory_order_relaxed); }

private:
  void apply(Command c) noexcept;
  std::vector<float> mBuf[2];
  int mCapacity = 0, mLength = 0, mPos = 0;
  State mState = State::Empty;
  LinearSmoother mLevel;
  std::atomic<int> mCommand{ 0 };
  std::atomic<int> mStateShown{ 0 };
  std::atomic<float> mPosShown{ 0.f }, mLenShown{ 0.f };
};

/** Метроном: щелчки по сильным/слабым долям размера 4/4. Генерирует сигнал в выход движка. */
class Metronome
{
public:
  void prepare(double sampleRate) noexcept;
  void reset() noexcept { mPhaseSamples = 0.0; mClick = 0; mBeat = 0; mStart = true; }
  /** Добавить щелчки в out (моно → оба канала). tempo — BPM; ppq — позиция хоста (если играет). */
  void render(float* l, float* r, int n, double tempo, bool hostPlaying, double ppq, float gain, bool accent) noexcept;

private:
  double mSr = 48000.0, mPhaseSamples = 0.0;
  int mClick = 0, mClickLen = 960, mBeat = 0;
  bool mAccentNow = false;
  bool mStart = true; // первый щелчок — сразу при включении
};

} // namespace tsl
