// SPDX-License-Identifier: GPL-3.0-or-later
// Пассивный трёхполосный tone stack (топология «treble-bass-mid» с общим узлом),
// дискретизированный билинейным преобразованием. Передаточная функция третьего
// порядка выводится из узловых уравнений схемы (см. D. Yeh, J. Smith, «Discretization
// of the '59 Fender Bassman Tone Stack», DAFx-06). Номиналы компонентов у каждой
// модели TwoonemoonSoundLab свои — это не копии конкретных усилителей.
#pragma once

#include "../DSP/DspCommon.h"

namespace tsl {

struct ToneStackComponents
{
  double R1, R2, R3, R4; // R1 — treble pot, R2 — bass pot, R3 — mid pot, R4 — slope
  double C1, C2, C3;
};

class ToneStack
{
public:
  /** Пересчитать коэффициенты. bass/mid/treble ∈ [0,1] (положения ручек). Не аллоцирует. */
  void design(const ToneStackComponents& c, double bass, double mid, double treble, double sampleRate) noexcept
  {
    const double t = clampv(treble, 0.0, 1.0);
    const double m = clampv(mid, 0.0, 1.0);
    // Bass — логарифмический потенциометр
    const double l = std::exp((clampv(bass, 0.0, 1.0) - 1.0) * 3.4);
    const double R1 = c.R1, R2 = c.R2, R3 = c.R3, R4 = c.R4, C1 = c.C1, C2 = c.C2, C3 = c.C3;

    const double b1 = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
    const double b2 = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) +
                      m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) +
                      l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4) +
                      l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) +
                      (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
    const double b3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4) -
                      m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) +
                      m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) + t * C1 * C2 * C3 * R1 * R3 * R4 -
                      t * m * C1 * C2 * C3 * R1 * R3 * R4 + t * l * C1 * C2 * C3 * R1 * R2 * R4;
    const double a0 = 1.0;
    const double a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
    const double a2 = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) +
                      l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) +
                      l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4) +
                      (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 +
                       C2 * C3 * R3 * R4);
    const double a3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4) -
                      m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) +
                      m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4) +
                      l * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;

    // Билинейное преобразование s = K (1 - z^-1) / (1 + z^-1) с предыскажением на 1 кГц
    const double w = kTwoPiD * 1000.0;
    const double K = w / std::tan(w / (2.0 * sampleRate));
    const double K2 = K * K, K3 = K2 * K;
    // Шаблоны (1-z)^k (1+z)^(3-k)
    static constexpr double P0[4] = { 1, 3, 3, 1 }, P1[4] = { 1, 1, -1, -1 }, P2[4] = { 1, -1, -1, 1 }, P3[4] = { 1, -3, 3, -1 };
    double B[4], A[4];
    for (int i = 0; i < 4; ++i)
    {
      B[i] = 0.0 * P0[i] + b1 * K * P1[i] + b2 * K2 * P2[i] + b3 * K3 * P3[i];
      A[i] = a0 * P0[i] + a1 * K * P1[i] + a2 * K2 * P2[i] + a3 * K3 * P3[i];
    }
    const double inv = 1.0 / A[0];
    for (int i = 0; i < 4; ++i)
    {
      mB[i] = B[i] * inv;
      mA[i] = A[i] * inv;
    }
  }

  void reset() noexcept { mZ[0] = mZ[1] = mZ[2] = 0.0; }

  inline float process(float xin) noexcept
  {
    const double x = xin;
    const double y = mB[0] * x + mZ[0];
    mZ[0] = mB[1] * x - mA[1] * y + mZ[1];
    mZ[1] = mB[2] * x - mA[2] * y + mZ[2];
    mZ[2] = mB[3] * x - mA[3] * y;
    return static_cast<float>(y);
  }

  void flush() noexcept
  {
    for (double& z : mZ)
      if (!std::isfinite(z) || std::fabs(z) < 1e-30) z = 0.0;
  }

  /** Модуль АЧХ (для тестов и графика). */
  double magnitudeAt(double f, double sampleRate) const noexcept
  {
    const double wv = kTwoPiD * f / sampleRate;
    double nr = 0, ni = 0, dr = 0, di = 0;
    for (int k = 0; k < 4; ++k)
    {
      nr += mB[k] * std::cos(-wv * k); ni += mB[k] * std::sin(-wv * k);
      dr += mA[k] * std::cos(-wv * k); di += mA[k] * std::sin(-wv * k);
    }
    return std::sqrt((nr * nr + ni * ni) / (dr * dr + di * di));
  }

private:
  double mB[4] = { 1, 0, 0, 0 }, mA[4] = { 1, 0, 0, 0 };
  double mZ[3] = { 0, 0, 0 };
};

} // namespace tsl
