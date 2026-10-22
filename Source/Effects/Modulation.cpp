// SPDX-License-Identifier: GPL-3.0-or-later
#include "Modulation.h"

namespace tsl {

namespace {
inline float wetMix(float dry, float wet, float m) noexcept { return dry + m * (wet - dry); }
} // namespace

// ============================================================================ Chorus
void Chorus::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  for (auto& l : mLine) l.prepare(static_cast<int>(0.03 * mSampleRate) + 8);
  mMix.prepare(static_cast<float>(mSampleRate), 0.03f);
  mMix.snap(mMix.target());
  reset();
}
void Chorus::reset() noexcept
{
  for (auto& l : mLine) l.reset();
  for (auto& t : mTone) t.reset();
  mLfo.reset();
}
void Chorus::update(const ParamValues& p) noexcept
{
  mRate = p[kChoRate];
  mDepth = p[kChoDepth] * 0.01f;
  mWidth = p[kChoWidth] * 0.01f;
  mSync = p[kChoSync] >= 0.5f;
  mDiv = static_cast<int>(p[kChoDivision]);
  mTempo.read(p);
  mMix.setTarget(p[kChoMix] * 0.01f);
  if (p[kChoTone] != mToneHz || mTone[0].coefs().b0 == 1.0)
  {
    mToneHz = p[kChoTone];
    const auto c = design::make(FilterType::LowPass, mSampleRate, mToneHz, 0.707);
    mTone[0].setCoefs(c);
    mTone[1].setCoefs(c);
  }
}
void Chorus::process(AudioBlock& b) noexcept
{
  const float sr = static_cast<float>(mSampleRate);
  const float rate = syncedRate(mSync, mDiv, mRate, mTempo.tempo(b.transport));
  const float base = 0.007f * sr, range = 0.006f * sr * mDepth;
  for (int i = 0; i < b.numFrames; ++i)
  {
    const float inL = b.l[i], inR = b.stereo ? b.r[i] : inL;
    const float lfoL = mLfo.next(rate, sr);
    const float lfoR = std::sin(kTwoPi * (mLfo.phase() + 0.25f * mWidth));
    mLine[0].push(inL);
    mLine[1].push(inR);
    const float wl = mTone[0].process(mLine[0].readCubic(base + range * (0.5f + 0.5f * lfoL)));
    const float wr = mTone[1].process(mLine[1].readCubic(base + range * (0.5f + 0.5f * lfoR)));
    const float m = mMix.next();
    b.l[i] = wetMix(inL, wl, m);
    b.r[i] = wetMix(inR, wr, m);
  }
  b.stereo = true;
}

// ============================================================================ Flanger
void Flanger::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const float sr = static_cast<float>(mSampleRate);
  for (auto& l : mLine) l.prepare(static_cast<int>(0.025 * mSampleRate) + 8);
  mManual.prepare(sr, 0.05f);
  mManual.snap(mManual.target() > 0.f ? mManual.target() : 2.f);
  mMix.prepare(sr, 0.03f);
  mMix.snap(mMix.target());
  reset();
}
void Flanger::reset() noexcept
{
  for (auto& l : mLine) l.reset();
  mFb[0] = mFb[1] = 0.f;
  mLfo.reset();
}
void Flanger::update(const ParamValues& p) noexcept
{
  mRate = p[kFlaRate];
  mDepth = p[kFlaDepth] * 0.01f;
  mFeedback = p[kFlaFeedback] * 0.01f;
  mManual.setTarget(p[kFlaManual]);
  mSync = p[kFlaSync] >= 0.5f;
  mDiv = static_cast<int>(p[kFlaDivision]);
  mTempo.read(p);
  mMix.setTarget(p[kFlaMix] * 0.01f);
}
void Flanger::process(AudioBlock& b) noexcept
{
  const float sr = static_cast<float>(mSampleRate);
  const float rate = syncedRate(mSync, mDiv, mRate, mTempo.tempo(b.transport));
  for (int i = 0; i < b.numFrames; ++i)
  {
    const float inL = b.l[i], inR = b.stereo ? b.r[i] : inL;
    const float man = mManual.next() * 0.001f * sr;
    const float l1 = mLfo.next(rate, sr, LfoShape::Triangle);
    const float l2 = 4.f * std::fabs(mLfo.phase() + 0.25f - std::floor(mLfo.phase() + 0.25f) - 0.5f) - 1.f;
    const float dL = std::max(1.5f, man * (1.f + 0.9f * mDepth * l1));
    const float dR = std::max(1.5f, man * (1.f + 0.9f * mDepth * l2));
    const float yl = mLine[0].readCubic(dL), yr = mLine[1].readCubic(dR);
    mLine[0].push(inL + mFeedback * 0.95f * fastTanh(yl));
    mLine[1].push(inR + mFeedback * 0.95f * fastTanh(yr));
    const float m = mMix.next();
    const float norm = 1.f / (1.f + 0.5f * m);
    b.l[i] = (inL + m * yl) * norm;
    b.r[i] = (inR + m * yr) * norm;
  }
  b.stereo = true;
}

// ============================================================================ Phaser
void Phaser::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  mMix.prepare(static_cast<float>(mSampleRate), 0.03f);
  mMix.snap(mMix.target());
  reset();
}
void Phaser::reset() noexcept
{
  for (auto& ch : mState)
    for (auto& s : ch) s = 0.f;
  mFbState[0] = mFbState[1] = 0.f;
  mLfo.reset();
}
void Phaser::update(const ParamValues& p) noexcept
{
  mRate = p[kPhaRate];
  mDepth = p[kPhaDepth] * 0.01f;
  mFeedback = p[kPhaFeedback] * 0.01f;
  static constexpr int kStages[4] = { 4, 6, 8, 12 };
  mStages = kStages[clampv(static_cast<int>(p[kPhaStages]), 0, 3)];
  mStereo = p[kPhaStereo] / 360.f;
  mSync = p[kPhaSync] >= 0.5f;
  mDiv = static_cast<int>(p[kPhaDivision]);
  mTempo.read(p);
  mMix.setTarget(p[kPhaMix] * 0.01f);
}
void Phaser::process(AudioBlock& b) noexcept
{
  const float sr = static_cast<float>(mSampleRate);
  const float rate = syncedRate(mSync, mDiv, mRate, mTempo.tempo(b.transport));
  const float octaves = 0.5f + 3.5f * mDepth;
  for (int i = 0; i < b.numFrames; ++i)
  {
    const float in[2] = { b.l[i], b.stereo ? b.r[i] : b.l[i] };
    const float lfo0 = mLfo.next(rate, sr);
    const float m = mMix.next();
    float out[2];
    for (int c = 0; c < 2; ++c)
    {
      const float lfo = c == 0 ? lfo0 : std::sin(kTwoPi * (mLfo.phase() + mStereo));
      const float fc = 160.f * std::pow(2.f, octaves * (0.5f + 0.5f * lfo));
      const float g = std::tan(kPi * std::min(fc, sr * 0.45f) / sr);
      const float G = g / (1.f + g);
      float x = in[c] + mFeedback * mFbState[c];
      for (int s = 0; s < mStages; ++s)
      {
        float& st = mState[c][s];
        const float v = (x - st) * G;
        const float lp = v + st;
        st = lp + v;
        x = 2.f * lp - x;
      }
      mFbState[c] = undenormal(fastTanh(x));
      // Mix 100% — классическая смесь 50/50 (глубокие провалы), 0% — сухой сигнал
      out[c] = wetMix(in[c], x, m * 0.5f);
    }
    b.l[i] = out[0];
    b.r[i] = out[1];
  }
  for (auto& ch : mState)
    for (auto& s : ch) s = undenormal(s);
  b.stereo = true;
}

// ============================================================================ Tremolo
void Tremolo::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  mDepth.prepare(static_cast<float>(mSampleRate), 0.03f);
  mDepth.snap(mDepth.target());
  reset();
}
void Tremolo::reset() noexcept { mLfo.reset(); }
void Tremolo::update(const ParamValues& p) noexcept
{
  mRate = p[kTrmRate];
  mDepth.setTarget(p[kTrmDepth] * 0.01f);
  mShape = static_cast<int>(p[kTrmShape]);
  mStereo = p[kTrmStereo] >= 0.5f;
  mSync = p[kTrmSync] >= 0.5f;
  mDiv = static_cast<int>(p[kTrmDivision]);
  mTempo.read(p);
}
void Tremolo::process(AudioBlock& b) noexcept
{
  const float sr = static_cast<float>(mSampleRate);
  const float rate = syncedRate(mSync, mDiv, mRate, mTempo.tempo(b.transport));
  const LfoShape shape = mShape == 1 ? LfoShape::Triangle : (mShape == 2 ? LfoShape::Square : LfoShape::Sine);
  if (mStereo && !b.stereo)
  {
    copyBuffer(b.r, b.l, b.numFrames);
    b.stereo = true;
  }
  for (int i = 0; i < b.numFrames; ++i)
  {
    const float d = mDepth.next();
    const float lfo = 0.5f + 0.5f * mLfo.next(rate, sr, shape);
    b.l[i] *= 1.f - d * lfo;
    if (b.stereo) b.r[i] *= 1.f - d * (mStereo ? (1.f - lfo) : lfo);
  }
}

// ============================================================================ Vibrato
void Vibrato::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  for (auto& l : mLine) l.prepare(static_cast<int>(0.02 * mSampleRate) + 8);
  mDepth.prepare(static_cast<float>(mSampleRate), 0.03f);
  mDepth.snap(mDepth.target());
  reset();
}
void Vibrato::reset() noexcept
{
  for (auto& l : mLine) l.reset();
  mLfo.reset();
}
void Vibrato::update(const ParamValues& p) noexcept
{
  mRate = p[kVibRate];
  mDepth.setTarget(p[kVibDepth] * 0.01f);
  mSync = p[kVibSync] >= 0.5f;
  mDiv = static_cast<int>(p[kVibDivision]);
  mTempo.read(p);
}
void Vibrato::process(AudioBlock& b) noexcept
{
  const float sr = static_cast<float>(mSampleRate);
  const float rate = syncedRate(mSync, mDiv, mRate, mTempo.tempo(b.transport));
  const float base = 0.005f * sr;
  for (int i = 0; i < b.numFrames; ++i)
  {
    const float d = base + mDepth.next() * 0.003f * sr * mLfo.next(rate, sr);
    mLine[0].push(b.l[i]);
    b.l[i] = mLine[0].readCubic(d);
    if (b.stereo)
    {
      mLine[1].push(b.r[i]);
      b.r[i] = mLine[1].readCubic(d);
    }
  }
}

// ============================================================================ Rotary
void Rotary::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const auto lp = design::make(FilterType::LowPass, mSampleRate, 800.0, 0.7071);
  const auto hp = design::make(FilterType::HighPass, mSampleRate, 800.0, 0.7071);
  mXoverLp.setCoefs(lp); mXoverLp2.setCoefs(lp);
  mXoverHp.setCoefs(hp); mXoverHp2.setCoefs(hp);
  mHornLine.prepare(static_cast<int>(0.01 * mSampleRate) + 8);
  mDrumLine.prepare(static_cast<int>(0.01 * mSampleRate) + 8);
  mMix.prepare(static_cast<float>(mSampleRate), 0.03f);
  mMix.snap(mMix.target());
  mHornHz = mHornTarget;
  mDrumHz = mDrumTarget;
  reset();
}
void Rotary::reset() noexcept
{
  mXoverLp.reset(); mXoverLp2.reset(); mXoverHp.reset(); mXoverHp2.reset();
  mHornLine.reset();
  mDrumLine.reset();
}
void Rotary::update(const ParamValues& p) noexcept
{
  const int speed = static_cast<int>(p[kRotSpeed]);
  mHornTarget = speed == 0 ? 0.83f : (speed == 1 ? 6.7f : 0.f);
  mDrumTarget = speed == 0 ? 0.67f : (speed == 1 ? 5.8f : 0.f);
  mDrive = p[kRotDrive] * 0.1f;
  mBalance = p[kRotBalance] * 0.01f;
  mWidth = p[kRotWidth] * 0.01f;
  mMix.setTarget(p[kRotMix] * 0.01f);
}
void Rotary::process(AudioBlock& b) noexcept
{
  const float sr = static_cast<float>(mSampleRate);
  const int n = b.numFrames;
  // Инерция: рупор разгоняется быстрее барабана
  const float blockSec = static_cast<float>(n) / sr;
  mHornHz += (mHornTarget - mHornHz) * std::min(1.f, blockSec / 0.7f);
  mDrumHz += (mDrumTarget - mDrumHz) * std::min(1.f, blockSec / 2.8f);
  const float hornGain = std::min(1.f, 1.f + mBalance), drumGain = std::min(1.f, 1.f - mBalance);
  const float drive = 1.f + mDrive * 6.f;
  const float wOff = 0.25f * mWidth;
  for (int i = 0; i < n; ++i)
  {
    const float inL = b.l[i], inR = b.stereo ? b.r[i] : inL;
    float x = 0.5f * (inL + inR);
    x = fastTanh(x * drive) / drive * (1.f + mDrive * 0.5f);
    const float lo = mXoverLp2.process(mXoverLp.process(x));
    const float hi = mXoverHp2.process(mXoverHp.process(x));
    mHornPhase += mHornHz / sr;
    mHornPhase -= std::floor(mHornPhase);
    mDrumPhase += mDrumHz / sr;
    mDrumPhase -= std::floor(mDrumPhase);
    mHornLine.push(hi);
    mDrumLine.push(lo);
    const float hA = kTwoPi * mHornPhase, hB = kTwoPi * (mHornPhase + wOff);
    const float dA = kTwoPi * mDrumPhase, dB = kTwoPi * (mDrumPhase + wOff);
    const float base = 0.0025f * sr, dop = 0.0004f * sr;
    const float hornL = mHornLine.readLinear(base + dop * std::sin(hA)) * (0.72f + 0.28f * std::cos(hA));
    const float hornR = mHornLine.readLinear(base + dop * std::sin(hB)) * (0.72f + 0.28f * std::cos(hB));
    const float drumL = mDrumLine.readLinear(base + dop * 0.3f * std::sin(dA)) * (0.86f + 0.14f * std::cos(dA));
    const float drumR = mDrumLine.readLinear(base + dop * 0.3f * std::sin(dB)) * (0.86f + 0.14f * std::cos(dB));
    const float m = mMix.next();
    b.l[i] = wetMix(inL, hornGain * hornL + drumGain * drumL, m);
    b.r[i] = wetMix(inR, hornGain * hornR + drumGain * drumR, m);
  }
  b.stereo = true;
}

// ============================================================================ Envelope filter
void EnvFilter::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const float sr = static_cast<float>(mSampleRate);
  mFreq.prepare(sr, 0.004f);
  mFreq.snap(mLow);
  mMix.prepare(sr, 0.03f);
  mMix.snap(mMix.target());
  mAtt = onePoleCoef(0.008f, sr);
  mRel = onePoleCoef(0.15f, sr);
  reset();
}
void EnvFilter::reset() noexcept
{
  for (auto& s : mSvf) s.reset();
  mEnv = 0.f;
}
void EnvFilter::update(const ParamValues& p) noexcept
{
  const float sr = static_cast<float>(mSampleRate);
  mMode = static_cast<int>(p[kEnvMode]);
  mDown = p[kEnvDirection] >= 0.5f;
  mSens = p[kEnvSensitivity] * 0.1f;
  mLow = p[kEnvRangeLow];
  mHigh = p[kEnvRangeHigh];
  mQ = p[kEnvQ];
  mAtt = onePoleCoef(p[kEnvAttack] * 0.001f, sr);
  mRel = onePoleCoef(p[kEnvRelease] * 0.001f, sr);
  mMix.setTarget(p[kEnvMix] * 0.01f);
}
void EnvFilter::process(AudioBlock& b) noexcept
{
  const float sr = static_cast<float>(mSampleRate);
  const float gain = 2.f + 60.f * mSens * mSens;
  const float ratio = mHigh / std::max(mLow, 1.f);
  for (int i = 0; i < b.numFrames; ++i)
  {
    const float inL = b.l[i], inR = b.stereo ? b.r[i] : inL;
    const float a = std::fabs(0.5f * (inL + inR));
    mEnv = a > mEnv ? a + mAtt * (mEnv - a) : a + mRel * (mEnv - a);
    float e = clampv(mEnv * gain, 0.f, 1.f);
    if (mDown) e = 1.f - e;
    mFreq.setTarget(mLow * std::pow(ratio, e));
    const float f = mFreq.next();
    const float m = mMix.next();
    for (int c = 0; c < (b.stereo ? 2 : 1); ++c)
    {
      mSvf[c].set(f, mQ, sr);
      const float x = c == 0 ? inL : inR;
      const auto o = mSvf[c].process(x);
      const float y = mMode == 0 ? o.lp : (mMode == 1 ? o.bp * 1.4f : o.hp);
      (c == 0 ? b.l : b.r)[i] = wetMix(x, y, m);
    }
  }
  mEnv = undenormal(mEnv);
}

} // namespace tsl
