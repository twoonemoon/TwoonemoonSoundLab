// SPDX-License-Identifier: GPL-3.0-or-later
// Низколатентный оверсэмплинг на полифазных IIR halfband-фильтрах (каскады
// allpass-секций). Коэффициенты рассчитываются методом Valenzuela–Constantinides
// по эллиптическим функциям (опубликованный метод проектирования). Минимальная
// групповая задержка: в полосе гитарного сигнала — единицы отсчётов, без lookahead.
#pragma once

#include "DspCommon.h"
#include <array>
#include <vector>

namespace tsl {

namespace halfband {

/** Рассчитать nCoefs коэффициентов для halfband-фильтра с заданной шириной переходной полосы.
 *  transition — нормированная ширина (0..0.5) относительно повышенной частоты. Не для audio thread. */
std::vector<double> designCoefs(int nCoefs, double transition);

/** Оценка подавления в полосе задерживания (дБ, положительное число). */
double estimateAttenuationDb(int nCoefs, double transition);

} // namespace halfband

/** Одна 2x-ступень: апсемплинг и даунсемплинг с собственными состояниями. */
class HalfbandStage
{
public:
  static constexpr int kMaxCoefs = 16;

  void init(const std::vector<double>& coefs) noexcept;
  void reset() noexcept;

  /** Один входной отсчёт → два выходных. */
  inline void upsample(float in, float& out0, float& out1) noexcept
  {
    float even = in, odd = in;
    for (int i = 0; i < mN; i += 2)
    {
      const float t0 = even;
      even = (even - mUpY[i]) * mC[i] + mUpX[i];
      mUpX[i] = t0;
      mUpY[i] = even;
      if (i + 1 < mN)
      {
        const float t1 = odd;
        odd = (odd - mUpY[i + 1]) * mC[i + 1] + mUpX[i + 1];
        mUpX[i + 1] = t1;
        mUpY[i + 1] = odd;
      }
    }
    out0 = even;
    out1 = odd;
  }

  /** Два входных отсчёта → один выходной. */
  inline float downsample(float in0, float in1) noexcept
  {
    float s0 = in1, s1 = in0;
    for (int i = 0; i < mN; i += 2)
    {
      const float t0 = s0;
      s0 = (s0 - mDnY[i]) * mC[i] + mDnX[i];
      mDnX[i] = t0;
      mDnY[i] = s0;
      if (i + 1 < mN)
      {
        const float t1 = s1;
        s1 = (s1 - mDnY[i + 1]) * mC[i + 1] + mDnX[i + 1];
        mDnX[i + 1] = t1;
        mDnY[i + 1] = s1;
      }
    }
    return 0.5f * (s0 + s1);
  }

  void flushDenormals() noexcept;

private:
  int mN = 0;
  std::array<float, kMaxCoefs> mC{};
  std::array<float, kMaxCoefs> mUpX{}, mUpY{}, mDnX{}, mDnY{};
};

/** Оверсэмплер с коэффициентом 1, 2, 4 или 8. Буферы выделяются в prepare(). */
class Oversampler
{
public:
  static constexpr int kMaxStages = 3;

  /** Не для audio thread. */
  void prepare(int maxBlockSize, int maxFactor = 8);
  /** Сменить коэффициент (1/2/4/8). Вызывать вне audio thread или между блоками с последующим reset(). */
  void setFactor(int factor) noexcept;
  int factor() const noexcept { return mFactor; }
  void reset() noexcept;

  /** Апсемплинг блока: возвращает указатель на внутренний буфер длиной n*factor. */
  float* upsample(const float* in, int n) noexcept;
  /** Даунсемплинг из внутреннего буфера (после обработки) в out[n]. */
  void downsample(float* out, int n) noexcept;
  float* buffer() noexcept { return mBufA.data(); }

  /** Приблизительная групповая задержка на низких частотах (в отсчётах базовой частоты). */
  float groupDelaySamples() const noexcept;

private:
  int mFactor = 1, mStages = 0, mMaxBlock = 0;
  std::array<HalfbandStage, kMaxStages> mStage;
  std::vector<float> mBufA, mBufB;
};

} // namespace tsl
