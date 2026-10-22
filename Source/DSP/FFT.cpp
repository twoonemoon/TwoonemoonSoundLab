// SPDX-License-Identifier: GPL-3.0-or-later
#include "FFT.h"

#include <Accelerate/Accelerate.h>
#include <cmath>
#include <cstring>

namespace tsl {

RealFFT::~RealFFT()
{
  if (mSetup) vDSP_destroy_fftsetup(static_cast<FFTSetup>(mSetup));
}

void RealFFT::prepare(int size)
{
  int log2n = 0;
  while ((1 << log2n) < size) ++log2n;
  if (mSetup && log2n == mLog2) return;
  if (mSetup) vDSP_destroy_fftsetup(static_cast<FFTSetup>(mSetup));
  mLog2 = log2n;
  mSize = 1 << log2n;
  mSetup = vDSP_create_fftsetup(static_cast<vDSP_Length>(mLog2), kFFTRadix2);
  mTmpRe.assign(static_cast<size_t>(mSize / 2), 0.f);
  mTmpIm.assign(static_cast<size_t>(mSize / 2), 0.f);
}

void RealFFT::forward(const float* time, float* re, float* im) noexcept
{
  DSPSplitComplex sc{ re, im };
  vDSP_ctoz(reinterpret_cast<const DSPComplex*>(time), 2, &sc, 1, static_cast<vDSP_Length>(mSize / 2));
  vDSP_fft_zrip(static_cast<FFTSetup>(mSetup), &sc, 1, static_cast<vDSP_Length>(mLog2), kFFTDirection_Forward);
  // vDSP возвращает спектр, умноженный на 2 — компенсируем здесь
  const float half = 0.5f;
  vDSP_vsmul(re, 1, &half, re, 1, static_cast<vDSP_Length>(mSize / 2));
  vDSP_vsmul(im, 1, &half, im, 1, static_cast<vDSP_Length>(mSize / 2));
}

void RealFFT::inverse(const float* re, const float* im, float* time) noexcept
{
  std::memcpy(mTmpRe.data(), re, sizeof(float) * static_cast<size_t>(mSize / 2));
  std::memcpy(mTmpIm.data(), im, sizeof(float) * static_cast<size_t>(mSize / 2));
  DSPSplitComplex sc{ mTmpRe.data(), mTmpIm.data() };
  vDSP_fft_zrip(static_cast<FFTSetup>(mSetup), &sc, 1, static_cast<vDSP_Length>(mLog2), kFFTDirection_Inverse);
  vDSP_ztoc(&sc, 1, reinterpret_cast<DSPComplex*>(time), 2, static_cast<vDSP_Length>(mSize / 2));
  const float scale = 1.0f / static_cast<float>(mSize);
  vDSP_vsmul(time, 1, &scale, time, 1, static_cast<vDSP_Length>(mSize));
}

void RealFFT::multiplyAccumulate(const float* aRe, const float* aIm, const float* bRe, const float* bIm, float* accRe,
                                 float* accIm, int half) noexcept
{
  // DC и Nyquist хранятся как вещественные числа в re[0] / im[0]
  const float dc = aRe[0] * bRe[0];
  const float ny = aIm[0] * bIm[0];
  DSPSplitComplex a{ const_cast<float*>(aRe) + 1, const_cast<float*>(aIm) + 1 };
  DSPSplitComplex b{ const_cast<float*>(bRe) + 1, const_cast<float*>(bIm) + 1 };
  DSPSplitComplex c{ accRe + 1, accIm + 1 };
  vDSP_zvma(&a, 1, &b, 1, &c, 1, &c, 1, static_cast<vDSP_Length>(half - 1));
  accRe[0] += dc;
  accIm[0] += ny;
}

} // namespace tsl
