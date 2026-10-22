// SPDX-License-Identifier: GPL-3.0-or-later
// Генераторы тестовых сигналов и простые измерения.
#pragma once

#include "../Source/DSP/DspCommon.h"
#include <cmath>
#include <complex>
#include <vector>

namespace tsltest {

inline std::vector<float> sine(float freq, float amp, float sr, int n, float phase = 0.f)
{
  std::vector<float> v(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) v[static_cast<size_t>(i)] = amp * std::sin(tsl::kTwoPi * freq * static_cast<float>(i) / sr + phase);
  return v;
}

inline std::vector<float> impulse(int n, float amp = 1.f)
{
  std::vector<float> v(static_cast<size_t>(n), 0.f);
  if (n > 0) v[0] = amp;
  return v;
}

inline std::vector<float> noise(int n, float amp, uint32_t seed = 1234)
{
  tsl::FastRandom r(seed);
  std::vector<float> v(static_cast<size_t>(n));
  for (auto& x : v) x = amp * r.nextBipolar();
  return v;
}

/**
 * Синтетический «гитарный DI»: щипки струн по алгоритму Karplus–Strong (аккорд и одиночные ноты).
 * Пиковый уровень ≈ peakDb дБFS. Используется для рендер-тестов и выравнивания громкости.
 */
inline std::vector<float> pluckedGuitar(float sr, float seconds, float peakDb = -12.f, uint32_t seed = 77)
{
  const int n = static_cast<int>(sr * seconds);
  std::vector<float> out(static_cast<size_t>(n), 0.f);
  tsl::FastRandom rnd(seed);
  // Аккорд E5 (E2 B2 E3) каждые 1 с + одиночные ноты между
  const float chord[] = { 82.41f, 123.47f, 164.81f };
  const float notes[] = { 196.0f, 246.94f, 329.63f, 392.0f };
  auto pluck = [&](float freq, int start, float amp) {
    const int period = std::max(2, static_cast<int>(sr / freq));
    std::vector<float> buf(static_cast<size_t>(period));
    for (auto& v : buf) v = rnd.nextBipolar() * amp;
    int idx = 0;
    for (int i = start; i < n; ++i)
    {
      const int nxt = (idx + 1) % period;
      const float y = buf[static_cast<size_t>(idx)];
      buf[static_cast<size_t>(idx)] = 0.4985f * (y + buf[static_cast<size_t>(nxt)]);
      out[static_cast<size_t>(i)] += y;
      idx = nxt;
      if (i - start > static_cast<int>(sr * 2.5f)) break;
    }
  };
  for (int t = 0; t < static_cast<int>(seconds); ++t)
  {
    const int s0 = static_cast<int>(t * sr);
    for (int c = 0; c < 3; ++c) pluck(chord[c], s0 + c * static_cast<int>(sr * 0.008f), 0.5f);
    pluck(notes[t % 4], s0 + static_cast<int>(sr * 0.5f), 0.6f);
  }
  float pk = 0.f;
  for (float v : out) pk = std::max(pk, std::fabs(v));
  const float g = pk > 0.f ? std::pow(10.f, peakDb / 20.f) / pk : 1.f;
  for (auto& v : out) v *= g;
  return out;
}

inline float rms(const float* x, int n)
{
  double s = 0;
  for (int i = 0; i < n; ++i) s += static_cast<double>(x[i]) * x[i];
  return n > 0 ? static_cast<float>(std::sqrt(s / n)) : 0.f;
}

inline float rms(const std::vector<float>& v, int from = 0)
{
  return rms(v.data() + from, static_cast<int>(v.size()) - from);
}

inline float peak(const std::vector<float>& v, int from = 0)
{
  float p = 0;
  for (size_t i = static_cast<size_t>(from); i < v.size(); ++i) p = std::max(p, std::fabs(v[i]));
  return p;
}

inline bool allFinite(const std::vector<float>& v)
{
  for (float x : v)
    if (!std::isfinite(x)) return false;
  return true;
}

/** Амплитуда компоненты частоты f (корреляция с синусом/косинусом), окно Ханна. */
inline float toneAmplitude(const float* x, int n, float f, float sr)
{
  double re = 0, im = 0, wsum = 0;
  for (int i = 0; i < n; ++i)
  {
    const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (n - 1));
    const double ph = 2.0 * M_PI * f * i / sr;
    re += w * x[i] * std::cos(ph);
    im += w * x[i] * std::sin(ph);
    wsum += w;
  }
  return static_cast<float>(2.0 * std::sqrt(re * re + im * im) / wsum);
}

inline float toneAmplitude(const std::vector<float>& v, int from, float f, float sr)
{
  return toneAmplitude(v.data() + from, static_cast<int>(v.size()) - from, f, sr);
}

inline float db(float g) { return 20.f * std::log10(std::max(g, 1e-12f)); }

} // namespace tsltest
