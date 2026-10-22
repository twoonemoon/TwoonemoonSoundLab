// SPDX-License-Identifier: GPL-3.0-or-later
#include "PitchDetector.h"

#include "DspCommon.h"

#include <cmath>

namespace tsl {

void PitchDetector::prepare(double sampleRate, float minFreq, float maxFreq)
{
  mRate = sampleRate * 0.5;
  mMaxLag = static_cast<int>(mRate / std::max(minFreq, 20.f));
  mMinLag = std::max(2, static_cast<int>(mRate / std::max(maxFreq, 100.f)));
  mWindow = 1;
  while (mWindow < mMaxLag * 2 + 64) mWindow <<= 1;
  int fftSize = 1;
  while (fftSize < mWindow * 2) fftSize <<= 1;
  mFft.prepare(fftSize);
  mRing.assign(static_cast<size_t>(mWindow), 0.f);
  mTime.assign(static_cast<size_t>(fftSize), 0.f);
  mRe.assign(static_cast<size_t>(fftSize / 2), 0.f);
  mIm.assign(static_cast<size_t>(fftSize / 2), 0.f);
  mAcf.assign(static_cast<size_t>(fftSize), 0.f);
  mNsdf.assign(static_cast<size_t>(mMaxLag + 2), 0.f);
  mFrame.assign(static_cast<size_t>(mWindow), 0.f);
  mWrite = 0;
}

void PitchDetector::push(const float* x, int n)
{
  if (mRing.empty()) return;
  // Децимация ×2 с простым ФНЧ (полоса тюнера ≤ 1.5 кГц, запас огромный)
  for (int i = 0; i < n; ++i)
  {
    mLpState += 0.45f * (x[i] - mLpState);
    const float y = 0.5f * (mLpState + mDecPrev);
    mDecPrev = mLpState;
    mDecPhase = !mDecPhase;
    if (!mDecPhase) continue;
    mRing[static_cast<size_t>(mWrite)] = y;
    mWrite = (mWrite + 1) % mWindow;
  }
}

PitchDetector::Result PitchDetector::estimate()
{
  Result res;
  if (mRing.empty()) return res;
  const int W = mWindow;
  double energy = 0.0;
  for (int i = 0; i < W; ++i)
  {
    const float v = mRing[static_cast<size_t>((mWrite + i) % W)];
    mFrame[static_cast<size_t>(i)] = v;
    energy += static_cast<double>(v) * v;
  }
  res.level = static_cast<float>(std::sqrt(energy / W));
  if (res.level < 1e-4f) return res; // тишина (≈ −80 дБFS)

  // Автокорреляция через FFT (с нулевым дополнением → линейная)
  std::fill(mTime.begin(), mTime.end(), 0.f);
  std::copy(mFrame.begin(), mFrame.end(), mTime.begin());
  mFft.forward(mTime.data(), mRe.data(), mIm.data());
  const int half = static_cast<int>(mRe.size());
  for (int k = 1; k < half; ++k)
  {
    mRe[static_cast<size_t>(k)] = mRe[static_cast<size_t>(k)] * mRe[static_cast<size_t>(k)] + mIm[static_cast<size_t>(k)] * mIm[static_cast<size_t>(k)];
    mIm[static_cast<size_t>(k)] = 0.f;
  }
  mRe[0] = mRe[0] * mRe[0];
  mIm[0] = mIm[0] * mIm[0];
  mFft.inverse(mRe.data(), mIm.data(), mAcf.data());

  // NSDF: n(τ) = 2 r(τ) / m(τ), m(τ) = Σ x[j]² + x[j+τ]²
  double m = 2.0 * energy;
  for (int tau = 0; tau <= mMaxLag; ++tau)
  {
    if (tau > 0)
    {
      const double a = mFrame[static_cast<size_t>(tau - 1)], b = mFrame[static_cast<size_t>(W - tau)];
      m -= a * a + b * b;
    }
    mNsdf[static_cast<size_t>(tau)] = m > 1e-12 ? static_cast<float>(2.0 * mAcf[static_cast<size_t>(tau)] / m) : 0.f;
  }

  // Ключевые максимумы между пересечениями нуля
  float globalMax = 0.f;
  std::vector<int> peaks;
  int tau = 1;
  while (tau < mMaxLag && mNsdf[static_cast<size_t>(tau)] > 0.f) ++tau; // пропустить начальный положительный участок
  while (tau < mMaxLag)
  {
    while (tau < mMaxLag && mNsdf[static_cast<size_t>(tau)] <= 0.f) ++tau;
    int best = -1;
    float bestV = 0.f;
    while (tau < mMaxLag && mNsdf[static_cast<size_t>(tau)] > 0.f)
    {
      if (mNsdf[static_cast<size_t>(tau)] > bestV) { bestV = mNsdf[static_cast<size_t>(tau)]; best = tau; }
      ++tau;
    }
    if (best >= mMinLag)
    {
      peaks.push_back(best);
      globalMax = std::max(globalMax, bestV);
    }
  }
  if (peaks.empty() || globalMax < 0.3f) return res;
  const float thr = 0.88f * globalMax;
  int chosen = peaks.front();
  for (int p : peaks)
    if (mNsdf[static_cast<size_t>(p)] >= thr) { chosen = p; break; }

  // Параболическая интерполяция
  const float y0 = mNsdf[static_cast<size_t>(chosen - 1)], y1 = mNsdf[static_cast<size_t>(chosen)], y2 = mNsdf[static_cast<size_t>(chosen + 1)];
  const float den = y0 - 2.f * y1 + y2;
  const float shift = std::fabs(den) > 1e-9f ? 0.5f * (y0 - y2) / den : 0.f;
  const float period = static_cast<float>(chosen) + clampv(shift, -0.5f, 0.5f);
  res.frequency = static_cast<float>(mRate) / period;
  res.clarity = y1;
  return res;
}

PitchDetector::Note PitchDetector::noteFor(float freq, float a4)
{
  static const char* kNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
  Note n;
  if (freq <= 0.f) return n;
  const float midi = 69.f + 12.f * std::log2(freq / a4);
  n.midi = static_cast<int>(std::lround(midi));
  n.cents = (midi - static_cast<float>(n.midi)) * 100.f;
  n.name = kNames[((n.midi % 12) + 12) % 12];
  n.octave = n.midi / 12 - 1;
  return n;
}

} // namespace tsl
