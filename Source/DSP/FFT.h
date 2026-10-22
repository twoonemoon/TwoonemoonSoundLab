// SPDX-License-Identifier: GPL-3.0-or-later
// Обёртка над вещественным FFT Apple Accelerate (vDSP). План создаётся вне audio thread.
// Формат спектра — «упакованный» vDSP: re[0] = DC, im[0] = Nyquist, далее бины 1…N/2−1.
#pragma once

#include <vector>

namespace tsl {

class RealFFT
{
public:
  RealFFT() = default;
  ~RealFFT();
  RealFFT(const RealFFT&) = delete;
  RealFFT& operator=(const RealFFT&) = delete;

  /** Не для audio thread. size — степень двойки ≥ 16. */
  void prepare(int size);
  int size() const noexcept { return mSize; }

  /** Прямое преобразование: time[size] → re[size/2], im[size/2] (без масштабирования). */
  void forward(const float* time, float* re, float* im) noexcept;
  /** Обратное: re/im → time[size], масштаб 1/size (готовая обратная операция). */
  void inverse(const float* re, const float* im, float* time) noexcept;

  /** acc += a * b (упакованный формат). */
  static void multiplyAccumulate(const float* aRe, const float* aIm, const float* bRe, const float* bIm, float* accRe,
                                 float* accIm, int half) noexcept;

private:
  void* mSetup = nullptr;
  int mSize = 0, mLog2 = 0;
  std::vector<float> mTmpRe, mTmpIm;
};

} // namespace tsl
