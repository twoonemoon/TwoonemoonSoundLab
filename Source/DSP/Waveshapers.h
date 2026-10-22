// SPDX-License-Identifier: GPL-3.0-or-later
// Нелинейные характеристики, мотивированные реальными элементами схем.
// Это алгоритмические аппроксимации статических ВАХ, не SPICE-модели.
#pragma once

#include "DspCommon.h"

namespace tsl::shape {

/** Мягкое ограничение, гладкое, нечётное. */
inline float softClip(float x) noexcept { return fastTanh(x); }

/** Кубическое ограничение (C1-гладкое) с жёстким потолком ±1. */
inline float cubicClip(float x) noexcept
{
  if (x >= 1.0f) return 2.0f / 3.0f;
  if (x <= -1.0f) return -2.0f / 3.0f;
  return x - x * x * x / 3.0f;
}

/** Диоды в обратной связи ОУ: логарифмический рост (asinh) — мягкое, «прозрачное» сжатие. */
inline float feedbackDiodes(float x, float vd) noexcept
{
  return vd * std::asinh(x / vd);
}

/** Диоды на землю: мягкое колено, потолок ≈ vd, параметр жёсткости knee (1..8). */
inline float shuntDiodes(float x, float vd, float knee) noexcept
{
  const float u = x / vd;
  return vd * u / std::pow(1.0f + std::pow(std::fabs(u), knee), 1.0f / knee);
}

/** Асимметричный клиппер: разные пороги для положительной и отрицательной полуволн. */
inline float asymClip(float x, float posLimit, float negLimit) noexcept
{
  return x >= 0.0f ? posLimit * fastTanh(x / posLimit) : negLimit * fastTanh(x / negLimit);
}

/** Жёсткое ограничение. */
inline float hardClip(float x, float lim) noexcept { return clampv(x, -lim, lim); }

/**
 * Триод (общий катод), упрощённая модель передаточной характеристики с учётом
 * сеточного тока: положительная полуволна (сетка уходит в проводимость) сжимается
 * резче, отрицательная (к отсечке) — мягче и дальше. bias ∈ [-1, 1] сдвигает рабочую точку.
 * Выход инвертирован относительно входа как у реального каскада не делаем — фаза не важна.
 */
inline float triode(float x, float bias) noexcept
{
  const float v = x + bias * 0.35f;
  float y;
  if (v > 0.0f)
  {
    // сеточная проводимость: мягкое, но относительно раннее ограничение
    y = v / std::sqrt(1.0f + 1.8f * v * v);
  }
  else
  {
    // к отсечке: ограничение позже и плавнее
    const float u = -v;
    y = -(1.35f * fastTanh(u / 1.35f));
  }
  // вычитаем смещение рабочей точки, чтобы тишина давала ноль на выходе
  const float b = bias * 0.35f;
  const float y0 = b > 0.0f ? b / std::sqrt(1.0f + 1.8f * b * b) : -(1.35f * fastTanh(-b / 1.35f));
  return y - y0;
}

} // namespace tsl::shape
