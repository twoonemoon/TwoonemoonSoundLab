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
