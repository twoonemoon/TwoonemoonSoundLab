// SPDX-License-Identifier: GPL-3.0-or-later
#include "Convolver.h"

#include <Accelerate/Accelerate.h>
#include <algorithm>
#include <cstring>

namespace tsl {

std::unique_ptr<ConvolutionKernel> ConvolutionKernel::create(const float* ir, int length, int partSize, RealFFT& fft)
{
  auto k = std::make_unique<ConvolutionKernel>();
  const int P = partSize;
  k->partSize = P;
  k->irLength = std::max(0, length);
  k->head.assign(static_cast<size_t>(P), 0.f);
  for (int i = 0; i < std::min(length, P); ++i) k->head[static_cast<size_t>(i)] = ir[i];
  const int tail = std::max(0, length - P);
  k->numParts = (tail + P - 1) / P;
  k->partsRe.assign(static_cast<size_t>(k->numParts * P), 0.f);
  k->partsIm.assign(static_cast<size_t>(k->numParts * P), 0.f);
  std::vector<float> time(static_cast<size_t>(2 * P));
  for (int p = 0; p < k->numParts; ++p)
  {
    std::fill(time.begin(), time.end(), 0.f);
    const int start = P + p * P;
    const int count = std::min(P, length - start);
    for (int i = 0; i < count; ++i) time[static_cast<size_t>(i)] = ir[start + i];
    fft.forward(time.data(), k->partsRe.data() + p * P, k->partsIm.data() + p * P);
  }
  return k;
}

void ZeroLatencyConvolver::prepare(int partSize, int maxParts, int maxBlock)
{
  mP = partSize;
  mMaxParts = std::max(1, maxParts);
  mMaxBlock = std::max(1, maxBlock);
  mFft.prepare(2 * mP);
  mFdlRe.assign(static_cast<size_t>(mMaxParts * mP), 0.f);
  mFdlIm.assign(static_cast<size_t>(mMaxParts * mP), 0.f);
  for (auto* v : { &mPrev, &mCur, &mTailA, &mTailB, &mAccRe, &mAccIm }) v->assign(static_cast<size_t>(mP), 0.f);
  mTime.assign(static_cast<size_t>(2 * mP), 0.f);
  mTimeOut.assign(static_cast<size_t>(2 * mP), 0.f);
  mHist.assign(static_cast<size_t>(mP - 1 + mP), 0.f);
  mHeadA.assign(static_cast<size_t>(mP), 0.f);
  mHeadB.assign(static_cast<size_t>(mP), 0.f);
  reset();
}

void ZeroLatencyConvolver::reset() noexcept
{
  std::fill(mFdlRe.begin(), mFdlRe.end(), 0.f);
  std::fill(mFdlIm.begin(), mFdlIm.end(), 0.f);
  for (auto* v : { &mPrev, &mCur, &mTailA, &mTailB, &mHist }) std::fill(v->begin(), v->end(), 0.f);
  mFdlPos = 0;
  mPos = 0;
}

void ZeroLatencyConvolver::headConvolve(const ConvolutionKernel* k, const float* hist, float* dst, int m) noexcept
{
  // Свёртка: dst[n] = Σ_q x[n−q]·h[q]; hist начинается на P−1 отсчётов раньше текущего
  vDSP_conv(hist, 1, k->head.data() + mP - 1, -1, dst, 1, static_cast<vDSP_Length>(m), static_cast<vDSP_Length>(mP));
}

void ZeroLatencyConvolver::computeTail(const ConvolutionKernel* k, float* tailOut) noexcept
{
  if (!k || k->numParts == 0)
  {
    std::fill(tailOut, tailOut + mP, 0.f);
    return;
  }
  std::fill(mAccRe.begin(), mAccRe.end(), 0.f);
  std::fill(mAccIm.begin(), mAccIm.end(), 0.f);
  const int parts = std::min(k->numParts, mMaxParts);
  for (int p = 0; p < parts; ++p)
  {
    const int idx = (mFdlPos - p + mMaxParts) % mMaxParts;
    RealFFT::multiplyAccumulate(mFdlRe.data() + idx * mP, mFdlIm.data() + idx * mP, k->partsRe.data() + p * mP,
                                k->partsIm.data() + p * mP, mAccRe.data(), mAccIm.data(), mP);
  }
  mFft.inverse(mAccRe.data(), mAccIm.data(), mTimeOut.data());
  std::memcpy(tailOut, mTimeOut.data() + mP, sizeof(float) * static_cast<size_t>(mP));
}

void ZeroLatencyConvolver::process(const ConvolutionKernel* a, const ConvolutionKernel* b, const float* in, float* out,
                                   int n, float g0, float g1) noexcept
{
  int done = 0;
  while (done < n)
  {
    const int m = std::min(n - done, mP - mPos);
    float* histNew = mHist.data() + (mP - 1);
    std::memcpy(histNew, in + done, sizeof(float) * static_cast<size_t>(m));
    if (a) headConvolve(a, mHist.data(), mHeadA.data(), m);
    else std::fill(mHeadA.begin(), mHeadA.begin() + m, 0.f);
    if (b) headConvolve(b, mHist.data(), mHeadB.data(), m);

    for (int i = 0; i < m; ++i)
    {
      const float ya = mHeadA[static_cast<size_t>(i)] + mTailA[static_cast<size_t>(mPos + i)];
      if (b)
      {
        const float yb = mHeadB[static_cast<size_t>(i)] + mTailB[static_cast<size_t>(mPos + i)];
        const float g = g0 + (g1 - g0) * (static_cast<float>(done + i + 1) / static_cast<float>(n));
        out[done + i] = ya + g * (yb - ya);
      }
      else
        out[done + i] = ya;
    }
    // Сдвиг истории для головы
    std::memmove(mHist.data(), mHist.data() + m, sizeof(float) * static_cast<size_t>(mP - 1));
    std::memcpy(mCur.data() + mPos, in + done, sizeof(float) * static_cast<size_t>(m));
    mPos += m;
    done += m;

    if (mPos == mP)
    {
      // Граница раздела: спектр [prev, cur] → FDL, затем хвост для следующего блока
      std::memcpy(mTime.data(), mPrev.data(), sizeof(float) * static_cast<size_t>(mP));
      std::memcpy(mTime.data() + mP, mCur.data(), sizeof(float) * static_cast<size_t>(mP));
      mFft.forward(mTime.data(), mFdlRe.data() + mFdlPos * mP, mFdlIm.data() + mFdlPos * mP);
      computeTail(a, mTailA.data());
      if (b) computeTail(b, mTailB.data());
      mFdlPos = (mFdlPos + 1) % mMaxParts;
      std::swap(mPrev, mCur);
      mPos = 0;
    }
  }
}

} // namespace tsl
