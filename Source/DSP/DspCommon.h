// SPDX-License-Identifier: GPL-3.0-or-later
// TwoonemoonSoundLab — общие DSP-утилиты (без аллокаций, безопасны для audio thread).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#if defined(__aarch64__) || defined(_M_ARM64)
#define TSL_ARM64 1
#endif

namespace tsl {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr double kPiD = 3.14159265358979323846;
constexpr double kTwoPiD = 6.28318530717958647692;

inline float dbToGain(float db) noexcept { return std::pow(10.0f, db * 0.05f); }
inline float gainToDb(float g, float floorDb = -120.0f) noexcept
{
  return g > 1.0e-6f ? 20.0f * std::log10(g) : floorDb;
}

template <typename T>
inline T clampv(T x, T lo, T hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }

inline float lerp(float a, float b, float t) noexcept { return a + (b - a) * t; }

/** Защита от NaN/Inf: заменяет нечисловые значения нулём. */
inline float sanitize(float x) noexcept { return std::isfinite(x) ? x : 0.0f; }

/** Обнуляет денормализованные значения в состоянии фильтров. */
inline float undenormal(float x) noexcept { return (std::fabs(x) < 1.0e-20f) ? 0.0f : x; }

/** Быстрый рациональный tanh (Padé 7/6), ошибка < 2e-4 в |x|<4.97, далее насыщение. */
inline float fastTanh(float x) noexcept
{
  if (x > 4.97f) return 1.0f;
  if (x < -4.97f) return -1.0f;
  const float x2 = x * x;
  const float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
  const float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
  return a / b;
}

/** Время → коэффициент однополюсного сглаживания (достижение ~63% за tau). */
inline float onePoleCoef(float timeSeconds, float sampleRate) noexcept
{
  if (timeSeconds <= 0.0f) return 0.0f;
  return std::exp(-1.0f / (timeSeconds * sampleRate));
}

/** RAII: включает flush-to-zero/denormals-are-zero для текущего потока на время блока. */
class ScopedNoDenormals
{
public:
  ScopedNoDenormals() noexcept
  {
#if TSL_ARM64
    uint64_t fpcr;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
    mPrev = fpcr;
    fpcr |= (1ull << 24); // FZ
    __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#elif defined(__SSE__) || defined(__x86_64__)
    uint32_t csr;
    __asm__ __volatile__("stmxcsr %0" : "=m"(csr));
    mPrev = csr;
    csr |= 0x8040; // FTZ | DAZ
    __asm__ __volatile__("ldmxcsr %0" : : "m"(csr));
#endif
  }
  ~ScopedNoDenormals() noexcept
  {
#if TSL_ARM64
    uint64_t fpcr = mPrev;
    __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#elif defined(__SSE__) || defined(__x86_64__)
    uint32_t csr = static_cast<uint32_t>(mPrev);
    __asm__ __volatile__("ldmxcsr %0" : : "m"(csr));
#endif
  }
  ScopedNoDenormals(const ScopedNoDenormals&) = delete;
  ScopedNoDenormals& operator=(const ScopedNoDenormals&) = delete;

private:
  uint64_t mPrev = 0;
};

/** Быстрый детерминированный генератор шума (xorshift32), для тестов и dither. */
class FastRandom
{
public:
  explicit FastRandom(uint32_t seed = 0x9E3779B9u) noexcept : mState(seed ? seed : 1u) {}
  uint32_t nextU32() noexcept
  {
    mState ^= mState << 13;
    mState ^= mState >> 17;
    mState ^= mState << 5;
    return mState;
  }
  /** Равномерно в [-1, 1). */
  float nextBipolar() noexcept { return static_cast<float>(nextU32()) * (2.0f / 4294967296.0f) - 1.0f; }
  float nextUnipolar() noexcept { return static_cast<float>(nextU32()) * (1.0f / 4294967296.0f); }

private:
  uint32_t mState;
};

inline void copyBuffer(float* dst, const float* src, int n) noexcept { std::memcpy(dst, src, sizeof(float) * static_cast<size_t>(n)); }
inline void clearBuffer(float* dst, int n) noexcept { std::memset(dst, 0, sizeof(float) * static_cast<size_t>(n)); }

} // namespace tsl
