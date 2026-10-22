// SPDX-License-Identifier: GPL-3.0-or-later
#include "Eq.h"

namespace tsl {

namespace {

FilterType bandFilter(int type) noexcept
{
  switch (type)
  {
    case 1: return FilterType::LowShelf;
    case 2: return FilterType::HighShelf;
    case 3: return FilterType::Notch;
    default: return FilterType::Peak;
  }
}

constexpr int bandParam(int band, int field) noexcept { return kEqB1On + band * 5 + field; } // On, Type, Freq, Gain, Q

// Баттерворт 4-го порядка = две секции с Q 0.5412 и 1.3066
constexpr double kButter4Q[2] = { 0.54119610, 1.30656296 };

} // namespace

void Eq::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const float blockRate = static_cast<float>(mSampleRate) / 64.f;
  for (auto& b : mBand)
  {
    b.logFreq.prepare(blockRate, 0.04f);
    b.gain.prepare(blockRate, 0.04f);
    b.q.prepare(blockRate, 0.04f);
    b.lastF = -1.f;
  }
  mOut.prepare(static_cast<float>(mSampleRate), 0.03f);
  mPreRing.prepare(16384);
  mPostRing.prepare(16384);
  mMono.assign(static_cast<size_t>(mMaxBlock), 0.f);
  mCutsDirty = true;
  reset();
}

void Eq::reset() noexcept
{
  for (auto& b : mBand)
    for (auto& f : b.bq) f.reset();
  for (auto& s : mLow)
    for (auto& f : s) f.reset();
  for (auto& s : mHigh)
    for (auto& f : s) f.reset();
}

void Eq::update(const ParamValues& p) noexcept
{
  for (int i = 0; i < kBands; ++i)
  {
    BandState& b = mBand[i];
    b.on = p[static_cast<size_t>(bandParam(i, 0))] >= 0.5f;
    b.type = static_cast<int>(p[static_cast<size_t>(bandParam(i, 1))]);
    b.logFreq.setTarget(std::log(p[static_cast<size_t>(bandParam(i, 2))]));
    b.gain.setTarget(p[static_cast<size_t>(bandParam(i, 3))]);
    b.q.setTarget(p[static_cast<size_t>(bandParam(i, 4))]);
    if (b.lastF < 0.f)
    {
      b.logFreq.snap(b.logFreq.target());
      b.gain.snap(b.gain.target());
      b.q.snap(b.q.target());
    }
  }
  const int ls = static_cast<int>(p[kEqLowSlope]), hs = static_cast<int>(p[kEqHighSlope]);
  if (p[kEqLowCut] != mLowHz || p[kEqHighCut] != mHighHz || ls != mLowSlope || hs != mHighSlope)
  {
    mLowHz = p[kEqLowCut];
    mHighHz = p[kEqHighCut];
    mLowSlope = ls;
    mHighSlope = hs;
    mCutsDirty = true;
  }
  mOut.setTarget(dbToGain(p[kEqOutput]));
}

void Eq::designBand(BandState& b, bool force) noexcept
{
  const float f = std::exp(b.logFreq.current()), g = b.gain.current(), q = b.q.current();
  if (!force && std::fabs(f - b.lastF) < 0.01f && std::fabs(g - b.lastG) < 0.005f && std::fabs(q - b.lastQ) < 0.001f && b.type == b.lastType)
    return;
  const auto c = design::make(bandFilter(b.type), mSampleRate, f, q, g);
  b.bq[0].setCoefs(c);
  b.bq[1].setCoefs(c);
  b.lastF = f; b.lastG = g; b.lastQ = q; b.lastType = b.type;
}

void Eq::designCuts() noexcept
{
  for (int st = 0; st < 2; ++st)
  {
    const double qLow = mLowSlope == 1 ? kButter4Q[st] : 0.7071;
    const double qHigh = mHighSlope == 1 ? kButter4Q[st] : 0.7071;
    const auto lc = design::make(FilterType::HighPass, mSampleRate, mLowHz, qLow);
    const auto hc = design::make(FilterType::LowPass, mSampleRate, mHighHz, qHigh);
    for (int c = 0; c < 2; ++c) { mLow[st][c].setCoefs(lc); mHigh[st][c].setCoefs(hc); }
  }
  mCutsDirty = false;
}

void Eq::process(AudioBlock& blk) noexcept
{
  const int n = blk.numFrames;
  // Анализатор: вход
  if (blk.stereo) for (int i = 0; i < n; ++i) mMono[static_cast<size_t>(i)] = 0.5f * (blk.l[i] + blk.r[i]);
  else copyBuffer(mMono.data(), blk.l, n);
  mPreRing.push(mMono.data(), static_cast<size_t>(n));

  if (mCutsDirty) designCuts();
  const int steps = std::max(1, n / 64);
  for (auto& b : mBand)
  {
    for (int s = 0; s < steps; ++s) { b.logFreq.next(); b.gain.next(); b.q.next(); }
    designBand(b, false);
  }

  const int nch = blk.stereo ? 2 : 1;
  float* ch[2] = { blk.l, blk.r };
  for (int c = 0; c < nch; ++c)
  {
    if (mLowHz > 20.5f)
    {
      mLow[0][c].processBlock(ch[c], n);
      if (mLowSlope == 1) mLow[1][c].processBlock(ch[c], n);
    }
    for (auto& b : mBand)
      if (b.on) b.bq[c].processBlock(ch[c], n);
    if (mHighHz < 19900.f)
    {
      mHigh[0][c].processBlock(ch[c], n);
      if (mHighSlope == 1) mHigh[1][c].processBlock(ch[c], n);
    }
  }
  for (int i = 0; i < n; ++i)
  {
    const float g = mOut.next();
    blk.l[i] *= g;
    if (blk.stereo) blk.r[i] *= g;
  }
  if (blk.stereo) for (int i = 0; i < n; ++i) mMono[static_cast<size_t>(i)] = 0.5f * (blk.l[i] + blk.r[i]);
  else copyBuffer(mMono.data(), blk.l, n);
  mPostRing.push(mMono.data(), static_cast<size_t>(n));
}

double Eq::responseDb(const std::function<float(int)>& param, double f, double sr)
{
  double db = 0.0;
  for (int i = 0; i < kBands; ++i)
  {
    if (param(bandParam(i, 0)) < 0.5f) continue;
    const int type = static_cast<int>(param(bandParam(i, 1)));
    const double g = param(bandParam(i, 3));
    if (type != 3 && std::fabs(g) < 0.01) continue;
    const auto c = design::make(bandFilter(type), sr, param(bandParam(i, 2)), param(bandParam(i, 4)), g);
    db += 20.0 * std::log10(std::max(1e-9, c.magnitudeAt(f, sr)));
  }
  const double lowHz = param(kEqLowCut), highHz = param(kEqHighCut);
  const int ls = static_cast<int>(param(kEqLowSlope)), hs = static_cast<int>(param(kEqHighSlope));
  for (int st = 0; st < (ls == 1 ? 2 : 1); ++st)
    if (lowHz > 20.5) db += 20.0 * std::log10(std::max(1e-9, design::make(FilterType::HighPass, sr, lowHz, ls == 1 ? kButter4Q[st] : 0.7071).magnitudeAt(f, sr)));
  for (int st = 0; st < (hs == 1 ? 2 : 1); ++st)
    if (highHz < 19900.0) db += 20.0 * std::log10(std::max(1e-9, design::make(FilterType::LowPass, sr, highHz, hs == 1 ? kButter4Q[st] : 0.7071).magnitudeAt(f, sr)));
  return db + param(kEqOutput);
}

} // namespace tsl
