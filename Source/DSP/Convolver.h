// SPDX-License-Identifier: GPL-3.0-or-later
// Свёртка с нулевой добавленной задержкой:
//  * «голова» IR (первые P отсчётов) — прямая свёртка во временной области;
//  * «хвост» — равномерно разбитая FFT-свёртка (overlap-save) с размером раздела P.
// Вклад хвоста в следующий блок из P отсчётов вычисляется по уже полученным входным
// данным, поэтому выход не задерживается. Ядро (ConvolutionKernel) подготавливается
// вне audio thread и неизменно; переключение — с кроссфейдом двух ядер на одной FDL.
#pragma once

#include "FFT.h"

#include <memory>
#include <vector>

namespace tsl {

struct ConvolutionKernel
{
  int partSize = 64;
  int numParts = 0;             // число разделов хвоста
  std::vector<float> head;      // P отсчётов
  std::vector<float> partsRe;   // numParts × P бинов (упакованный формат)
  std::vector<float> partsIm;
  int irLength = 0;

  /** Создать ядро (не для audio thread). fft должен быть подготовлен на 2*partSize. */
  static std::unique_ptr<ConvolutionKernel> create(const float* ir, int length, int partSize, RealFFT& fft);
};

class ZeroLatencyConvolver
{
public:
  /** Не для audio thread. maxParts — максимум разделов хвоста у используемых ядер. */
  void prepare(int partSize, int maxParts, int maxBlock);
  void reset() noexcept;
  int partSize() const noexcept { return mP; }
  /** После завершения кроссфейда второе ядро становится основным: хвост B → A. */
  void promoteSecond() noexcept { std::swap(mTailA, mTailB); }
  int maxParts() const noexcept { return mMaxParts; }

  /**
   * Обработать n отсчётов. Если b != nullptr, выход = (1−g)·conv(a) + g·conv(b), где g линейно
   * меняется от g0 до g1 на протяжении блока. a может быть nullptr (тишина).
   */
  void process(const ConvolutionKernel* a, const ConvolutionKernel* b, const float* in, float* out, int n, float g0 = 0.f,
               float g1 = 0.f) noexcept;

private:
  void headConvolve(const ConvolutionKernel* k, const float* hist, float* dst, int m) noexcept;
  void computeTail(const ConvolutionKernel* k, float* tailOut) noexcept;

  int mP = 64, mMaxParts = 0, mMaxBlock = 0;
  RealFFT mFft;
  std::vector<float> mFdlRe, mFdlIm; // кольцо спектров входа
  int mFdlPos = 0;
  std::vector<float> mPrev, mCur, mTime, mAccRe, mAccIm, mTimeOut;
  std::vector<float> mTailA, mTailB; // вклад хвоста в текущий блок
  std::vector<float> mHist;          // история входа для головы (P−1 + maxBlock)
  std::vector<float> mHeadA, mHeadB;
  int mPos = 0;
};

} // namespace tsl
