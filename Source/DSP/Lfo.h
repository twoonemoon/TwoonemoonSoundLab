// SPDX-License-Identifier: GPL-3.0-or-later
// LFO, темп и музыкальные доли для эффектов модуляции и задержки.
#pragma once

#include "../Audio/Module.h"
#include "DspCommon.h"

namespace tsl {

/** Длительность доли в четвертях (индекс соответствует labels::kDivision). */
inline double divisionBeats(int idx) noexcept
{
  static constexpr double kBeats[14] = { 4.0, 2.0, 3.0, 4.0 / 3.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0, 0.125 };
  return kBeats[clampv(idx, 0, 13)];
}

/** Текущий темп: хоста (если включено и доступно) либо ручной. */
inline double currentTempo(const TransportInfo* t, const ParamValues& p) noexcept
{
  if (p[kHostTempo] >= 0.5f && t && t->hostTempoValid && t->tempo > 1.0) return t->tempo;
  return p[kTempo];
}

/** Частота LFO с учётом синхронизации: freeRate (Гц) или доля такта. */
inline float syncedRate(bool sync, int division, float freeRate, double tempo) noexcept
{
  if (!sync) return freeRate;
  const double beatSec = 60.0 / std::max(20.0, tempo);
  return static_cast<float>(1.0 / (divisionBeats(division) * beatSec));
}

enum class LfoShape { Sine, Triangle, Square };

class Lfo
{
public:
  void reset(float phase = 0.f) noexcept { mPhase = phase - std::floor(phase); }
  /** Сдвиг фазы на rate/sr; возвращает значение в [-1, 1]. */
  inline float next(float rate, float sampleRate, LfoShape shape = LfoShape::Sine, float phaseOffset = 0.f) noexcept
  {
    float ph = mPhase + phaseOffset;
    ph -= std::floor(ph);
    mPhase += rate / sampleRate;
    if (mPhase >= 1.f) mPhase -= 1.f;
    switch (shape)
    {
      case LfoShape::Triangle: return 4.f * std::fabs(ph - 0.5f) - 1.f;
      case LfoShape::Square:
      {
        // Сглаженный меандр (без щелчков)
        const float s = std::sin(kTwoPi * ph);
        return fastTanh(s * 6.f);
      }
      case LfoShape::Sine:
      default: return std::sin(kTwoPi * ph);
    }
  }
  float phase() const noexcept { return mPhase; }

private:
  float mPhase = 0.f;
};

} // namespace tsl
