// SPDX-License-Identifier: GPL-3.0-or-later
#include "Utility.h"

namespace tsl {

// ============================================================================ Widener
void Widener::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  mWidth.prepare(static_cast<float>(mSampleRate), 0.03f);
  mWidth.snap(mWidth.target());
  mSideHp[0].setCoefs(design::make(FilterType::HighPass, mSampleRate, mMonoHz, 0.5412));
  mSideHp[1].setCoefs(design::make(FilterType::HighPass, mSampleRate, mMonoHz, 1.3066));
  reset();
}
void Widener::reset() noexcept
{
  for (auto& b : mSideHp) b.reset();
  mLR = mLL = mRR = 0.0;
}
void Widener::update(const ParamValues& p) noexcept
{
  mWidth.setTarget(p[kWidWidth] * 0.01f);
  if (p[kWidMonoBelow] != mMonoHz)
  {
    mMonoHz = p[kWidMonoBelow];
    mSideHp[0].setCoefs(design::make(FilterType::HighPass, mSampleRate, mMonoHz, 0.5412));
    mSideHp[1].setCoefs(design::make(FilterType::HighPass, mSampleRate, mMonoHz, 1.3066));
  }
}
void Widener::process(AudioBlock& b) noexcept
{
  if (!b.stereo) return; // моно-сигнал не расширяется (нет боковой составляющей) — честно ничего не делаем
  double lr = 0, ll = 0, rr = 0;
  for (int i = 0; i < b.numFrames; ++i)
  {
    const float w = mWidth.next();
    const float m = 0.5f * (b.l[i] + b.r[i]);
    // Низ боковой составляющей убирается — бас остаётся в моно и не теряется при сложении каналов
    const float s = mSideHp[1].process(mSideHp[0].process(0.5f * (b.l[i] - b.r[i]))) * w;
    b.l[i] = m + s;
    b.r[i] = m - s;
    lr += static_cast<double>(b.l[i]) * b.r[i];
    ll += static_cast<double>(b.l[i]) * b.l[i];
    rr += static_cast<double>(b.r[i]) * b.r[i];
  }
  const double k = 0.9;
  mLR = mLR * k + lr * (1 - k);
  mLL = mLL * k + ll * (1 - k);
  mRR = mRR * k + rr * (1 - k);
  const double den = std::sqrt(mLL * mRR);
  mCorr.store(den > 1e-12 ? static_cast<float>(mLR / den) : 1.f, std::memory_order_relaxed);
}

// ============================================================================ Limiter
void Limiter::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  mAttack = onePoleCoef(0.0002f, static_cast<float>(mSampleRate));
  mRelease = onePoleCoef(0.08f, static_cast<float>(mSampleRate));
  reset();
}
void Limiter::reset() noexcept
{
  mEnv = 0.f;
  mGr.store(0.f, std::memory_order_relaxed);
}
void Limiter::update(const ParamValues& p) noexcept
{
  mCeiling = dbToGain(p[kLimCeiling]);
  mRelease = onePoleCoef(p[kLimRelease] * 0.001f, static_cast<float>(mSampleRate));
}
void Limiter::process(AudioBlock& b) noexcept
{
  float minG = 1.f;
  for (int i = 0; i < b.numFrames; ++i)
  {
    const float a = b.stereo ? std::max(std::fabs(b.l[i]), std::fabs(b.r[i])) : std::fabs(b.l[i]);
    mEnv = a > mEnv ? a + mAttack * (mEnv - a) : a + mRelease * (mEnv - a);
    const float g = mEnv > mCeiling ? mCeiling / mEnv : 1.f;
    minG = std::min(minG, g);
    // Последняя ступень — жёсткое ограничение потолка (атака не нулевая, редкие пики срезаются)
    b.l[i] = clampv(b.l[i] * g, -mCeiling, mCeiling);
    if (b.stereo) b.r[i] = clampv(b.r[i] * g, -mCeiling, mCeiling);
  }
  mEnv = undenormal(mEnv);
  mGr.store(gainToDb(minG, -60.f), std::memory_order_relaxed);
}

// ============================================================================ Looper
void Looper::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  mCapacity = static_cast<int>(kMaxSeconds * mSampleRate);
  for (auto& b : mBuf) b.assign(static_cast<size_t>(mCapacity), 0.f);
  mLevel.prepare(static_cast<float>(mSampleRate), 0.03f);
  mLevel.snap(mLevel.target());
  mState = State::Empty;
  mLength = mPos = 0;
}
void Looper::reset() noexcept
{
  // Петля переживает переключения цепочки: сброс модулей не стирает запись
}
void Looper::update(const ParamValues& p) noexcept { mLevel.setTarget(dbToGain(p[kLoopLevel])); }

void Looper::apply(Command c) noexcept
{
  switch (c)
  {
    case Command::RecordToggle:
      if (mState == State::Empty) { mState = State::Recording; mLength = 0; mPos = 0; }
      else if (mState == State::Recording) { mState = mLength > 0 ? State::Playing : State::Empty; mPos = 0; }
      else if (mState == State::Overdubbing) mState = State::Playing;
      else if (mState == State::Playing || mState == State::Stopped) mState = State::Overdubbing;
      break;
    case Command::PlayStop:
      if (mState == State::Playing || mState == State::Overdubbing) mState = State::Stopped;
      else if (mState == State::Stopped) { mState = State::Playing; mPos = 0; }
      else if (mState == State::Recording) { mState = mLength > 0 ? State::Playing : State::Empty; mPos = 0; }
      break;
    case Command::Overdub:
      if (mState == State::Playing) mState = State::Overdubbing;
      else if (mState == State::Overdubbing) mState = State::Playing;
      break;
    case Command::Clear:
      mState = State::Empty;
      mLength = mPos = 0;
      break;
    default: break;
  }
}

void Looper::process(AudioBlock& b) noexcept
{
  const int cmd = mCommand.exchange(0, std::memory_order_acq_rel);
  if (cmd) apply(static_cast<Command>(cmd));
  const int n = b.numFrames;
  const int fade = std::max(1, static_cast<int>(0.005 * mSampleRate));
  bool makeStereo = mLength > 0 && (mState == State::Playing || mState == State::Overdubbing) && !b.stereo;
  if (makeStereo)
  {
    copyBuffer(b.r, b.l, n);
    b.stereo = true;
  }
  for (int i = 0; i < n; ++i)
  {
    const float inL = b.l[i], inR = b.stereo ? b.r[i] : inL;
    const float lvl = mLevel.next();
    switch (mState)
    {
      case State::Recording:
        if (mLength < mCapacity)
        {
          mBuf[0][static_cast<size_t>(mLength)] = inL;
          mBuf[1][static_cast<size_t>(mLength)] = inR;
          ++mLength;
        }
        else
        {
          mState = State::Playing; // буфер заполнен — переходим к воспроизведению
          mPos = 0;
        }
        break;
      case State::Playing:
      case State::Overdubbing:
      {
        if (mLength <= 0) break;
        // Плавная склейка на стыке петли
        float env = 1.f;
        if (mPos < fade) env = static_cast<float>(mPos) / fade;
        else if (mLength - mPos < fade) env = static_cast<float>(mLength - mPos) / fade;
        const float pl = mBuf[0][static_cast<size_t>(mPos)], pr = mBuf[1][static_cast<size_t>(mPos)];
        if (mState == State::Overdubbing)
        {
          mBuf[0][static_cast<size_t>(mPos)] = sanitize(pl + inL);
          mBuf[1][static_cast<size_t>(mPos)] = sanitize(pr + inR);
        }
        b.l[i] = inL + pl * lvl * env;
        if (b.stereo) b.r[i] = inR + pr * lvl * env;
        if (++mPos >= mLength) mPos = 0;
        break;
      }
      default: break;
    }
  }
  mStateShown.store(static_cast<int>(mState), std::memory_order_relaxed);
  mPosShown.store(mLength > 0 ? static_cast<float>(mPos) / mLength : 0.f, std::memory_order_relaxed);
  mLenShown.store(static_cast<float>(mLength / mSampleRate), std::memory_order_relaxed);
}

// ============================================================================ Metronome
void Metronome::prepare(double sampleRate) noexcept
{
  mSr = sampleRate;
  mClickLen = static_cast<int>(0.025 * sampleRate);
  reset();
}

void Metronome::render(float* l, float* r, int n, double tempo, bool hostPlaying, double ppq, float gain, bool accent) noexcept
{
  const double beatLen = 60.0 / std::max(20.0, tempo) * mSr;
  if (mStart)
  {
    mPhaseSamples = beatLen;
    mStart = false;
  }
  if (hostPlaying && ppq >= 0.0)
  {
    // Синхронно с транспортом: сколько сэмплов до следующей доли и её номер в такте 4/4
    const double next = std::ceil(ppq - 1e-9);
    const double toNext = (next - ppq) * beatLen;
    mPhaseSamples = beatLen - toNext;
    mBeat = static_cast<int>(next) % 4;
  }
  for (int i = 0; i < n; ++i)
  {
    if (mPhaseSamples >= beatLen)
    {
      mPhaseSamples -= beatLen;
      mClick = mClickLen;
      mAccentNow = accent && (mBeat % 4 == 0);
      mBeat = (mBeat + 1) % 4;
    }
    mPhaseSamples += 1.0;
    if (mClick > 0)
    {
      const int t = mClickLen - mClick;
      const float env = std::exp(-static_cast<float>(t) / (0.004f * static_cast<float>(mSr)));
      const float f = mAccentNow ? 1600.f : 1100.f;
      const float v = gain * env * std::sin(kTwoPi * f * static_cast<float>(t) / static_cast<float>(mSr)) * (mAccentNow ? 1.f : 0.7f);
      l[i] += v;
      r[i] += v;
      --mClick;
    }
  }
}

} // namespace tsl
