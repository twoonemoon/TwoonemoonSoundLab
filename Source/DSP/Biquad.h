// SPDX-License-Identifier: GPL-3.0-or-later
// Биквадратные фильтры (Transposed Direct Form II, double precision) и их дизайн
// по формулам RBJ Audio EQ Cookbook (общеизвестные публичные формулы).
#pragma once

#include "DspCommon.h"
#include <complex>

namespace tsl {

struct BiquadCoefs
{
  double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0; // a0 нормирован к 1

  /** Модуль АЧХ на частоте f (для отрисовки кривых EQ, не для audio thread). */
  double magnitudeAt(double f, double sampleRate) const noexcept
  {
    const double w = kTwoPiD * f / sampleRate;
    const std::complex<double> z1 = std::polar(1.0, -w);
    const std::complex<double> z2 = z1 * z1;
    const std::complex<double> num = b0 + b1 * z1 + b2 * z2;
    const std::complex<double> den = 1.0 + a1 * z1 + a2 * z2;
    return std::abs(num / den);
  }

  static BiquadCoefs identity() noexcept { return {}; }
};

enum class FilterType : int
{
  LowPass = 0,
  HighPass,
  BandPass,
  Notch,
  Peak,
  LowShelf,
  HighShelf,
  AllPass,
  LowPass1, // первого порядка
  HighPass1,
};

namespace design {

inline double clampFreq(double f, double sr) noexcept { return clampv(f, 1.0, sr * 0.49); }

inline BiquadCoefs make(FilterType type, double sampleRate, double freq, double q, double gainDb = 0.0) noexcept
{
  BiquadCoefs c;
  freq = clampFreq(freq, sampleRate);
  q = std::max(q, 0.025);
  const double w0 = kTwoPiD * freq / sampleRate;
  const double cw = std::cos(w0), sw = std::sin(w0);
  const double alpha = sw / (2.0 * q);
  const double A = std::pow(10.0, gainDb / 40.0);
  double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

  switch (type)
  {
    case FilterType::LowPass:
      b0 = (1 - cw) * 0.5; b1 = 1 - cw; b2 = (1 - cw) * 0.5;
      a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
      break;
    case FilterType::HighPass:
      b0 = (1 + cw) * 0.5; b1 = -(1 + cw); b2 = (1 + cw) * 0.5;
      a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
      break;
    case FilterType::BandPass: // постоянное усиление 0 дБ в пике
      b0 = alpha; b1 = 0; b2 = -alpha;
      a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
      break;
    case FilterType::Notch:
      b0 = 1; b1 = -2 * cw; b2 = 1;
      a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
      break;
    case FilterType::Peak:
      b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
      a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
      break;
    case FilterType::LowShelf:
    {
      const double sq = 2.0 * std::sqrt(A) * alpha;
      b0 = A * ((A + 1) - (A - 1) * cw + sq);
      b1 = 2 * A * ((A - 1) - (A + 1) * cw);
      b2 = A * ((A + 1) - (A - 1) * cw - sq);
      a0 = (A + 1) + (A - 1) * cw + sq;
      a1 = -2 * ((A - 1) + (A + 1) * cw);
      a2 = (A + 1) + (A - 1) * cw - sq;
      break;
    }
    case FilterType::HighShelf:
    {
      const double sq = 2.0 * std::sqrt(A) * alpha;
      b0 = A * ((A + 1) + (A - 1) * cw + sq);
      b1 = -2 * A * ((A - 1) + (A + 1) * cw);
      b2 = A * ((A + 1) + (A - 1) * cw - sq);
      a0 = (A + 1) - (A - 1) * cw + sq;
      a1 = 2 * ((A - 1) - (A + 1) * cw);
      a2 = (A + 1) - (A - 1) * cw - sq;
      break;
    }
    case FilterType::AllPass:
      b0 = 1 - alpha; b1 = -2 * cw; b2 = 1 + alpha;
      a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
      break;
    case FilterType::LowPass1:
    {
      const double k = std::tan(w0 * 0.5);
      b0 = k; b1 = k; b2 = 0;
      a0 = k + 1; a1 = k - 1; a2 = 0;
      break;
    }
    case FilterType::HighPass1:
    {
      const double k = std::tan(w0 * 0.5);
      b0 = 1; b1 = -1; b2 = 0;
      a0 = k + 1; a1 = k - 1; a2 = 0;
      break;
    }
  }
  c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0;
  c.a1 = a1 / a0; c.a2 = a2 / a0;
  return c;
}

} // namespace design

class Biquad
{
public:
  void setCoefs(const BiquadCoefs& c) noexcept { mC = c; }
  const BiquadCoefs& coefs() const noexcept { return mC; }
  void reset() noexcept { mZ1 = mZ2 = 0.0; }

  inline float process(float xin) noexcept
  {
    const double x = xin;
    const double y = mC.b0 * x + mZ1;
    mZ1 = mC.b1 * x - mC.a1 * y + mZ2;
    mZ2 = mC.b2 * x - mC.a2 * y;
    return static_cast<float>(y);
  }

  void processBlock(float* buf, int n) noexcept
  {
    double z1 = mZ1, z2 = mZ2;
    const double b0 = mC.b0, b1 = mC.b1, b2 = mC.b2, a1 = mC.a1, a2 = mC.a2;
    for (int i = 0; i < n; ++i)
    {
      const double x = buf[i];
      const double y = b0 * x + z1;
      z1 = b1 * x - a1 * y + z2;
      z2 = b2 * x - a2 * y;
      buf[i] = static_cast<float>(y);
    }
    // Защита от денормалов и NaN в состоянии
    mZ1 = std::isfinite(z1) && std::fabs(z1) > 1e-30 ? z1 : 0.0;
    mZ2 = std::isfinite(z2) && std::fabs(z2) > 1e-30 ? z2 : 0.0;
  }

private:
  BiquadCoefs mC;
  double mZ1 = 0.0, mZ2 = 0.0;
};

/** Простой однополюсный фильтр (ФНЧ/ФВЧ), для сопрягающих конденсаторов и сглаживаний в моделях. */
class OnePole
{
public:
  void setLowPass(float freq, float sampleRate) noexcept
  {
    const float f = clampv(freq, 1.0f, sampleRate * 0.49f);
    mA = std::exp(-kTwoPi * f / sampleRate);
  }
  void reset(float v = 0.0f) noexcept { mZ = v; }
  inline float lowPass(float x) noexcept
  {
    mZ = x + mA * (mZ - x);
    return mZ;
  }
  inline float highPass(float x) noexcept { return x - lowPass(x); }
  float state() const noexcept { return mZ; }

private:
  float mA = 0.0f, mZ = 0.0f;
};

} // namespace tsl
