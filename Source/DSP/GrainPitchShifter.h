// SPDX-License-Identifier: GPL-3.0-or-later
// Сдвиг высоты тона во временной области: две головки чтения линии задержки, перекрёстно
// затухающие окнами sin². Когда головка перезапускается (её вес равен нулю), новая начальная
// задержка выбирается по максимуму корреляции с текущей активной головкой (принцип WSOLA) —
// склейки фазово согласованы, средняя частота точная, «фланжирование» склеек уменьшено.
// Задержка голоса ≈ половина окна + поисковый диапазон. Без аллокаций в process().
#pragma once

#include "DelayLine.h"

namespace tsl {

class GrainPitchShifter
{
public:
  void prepare(double sampleRate, float windowMs = 40.f)
  {
    mWindow = static_cast<float>(sampleRate * windowMs * 0.001);
    mSearch = static_cast<int>(sampleRate * 0.016);
    mCorrLen = static_cast<int>(sampleRate * 0.006);
    // Худший случай: сдвиг вниз на 2 октавы или вверх на октаву с запасом
    mLine.prepare(static_cast<int>(mWindow * 3.f) + mSearch + mCorrLen + 16);
    reset();
  }

  void reset() noexcept
  {
    mLine.reset();
    mHead[0] = { startDelay(0.f), 0.f };
    mHead[1] = { startDelay(0.f), 0.5f };
  }

  void setSemitones(float st) noexcept { mRatio = std::pow(2.f, st / 12.f); }
  void setRatio(float r) noexcept { mRatio = clampv(r, 0.2f, 4.f); }

  inline float process(float x) noexcept
  {
    mLine.push(x);
    const float drift = mRatio - 1.f; // задержка уменьшается на drift за отсчёт
    float out = 0.f;
    for (int h = 0; h < 2; ++h)
    {
      Head& hd = mHead[h];
      hd.fade += 1.f / mWindow;
      if (hd.fade >= 1.f)
      {
        hd.fade -= 1.f;
        hd.delay = bestStart(mHead[1 - h].delay);
      }
      hd.delay -= drift;
      const float maxD = static_cast<float>(mLine.maxDelay() - 2);
      hd.delay = clampv(hd.delay, 1.f, maxD);
      const float w = std::sin(kPi * hd.fade);
      out += w * w * mLine.readLinear(hd.delay);
    }
    return out;
  }

private:
  struct Head
  {
    float delay;
    float fade;
  };

  /** Минимальная стартовая задержка, чтобы головка не догнала запись за окно. */
  float startDelay(float) const noexcept
  {
    const float travel = std::max(0.f, (mRatio - 1.f) * mWindow);
    return 2.f + travel;
  }

  /** Выбор стартовой задержки с максимальной корреляцией с другой головкой. */
  float bestStart(float otherDelay) const noexcept
  {
    const float base = startDelay(0.f);
    const int n = mCorrLen;
    float best = base, bestScore = -1e30f;
    for (int k = 0; k <= mSearch; k += 2)
    {
      const float cand = base + static_cast<float>(k);
      float acc = 0.f, e = 1e-9f;
      // Сравниваем прошлые участки сигнала, откуда читают головки (j — назад от текущей позиции)
      const int dOther = static_cast<int>(otherDelay), dCand = static_cast<int>(cand);
      for (int j = 0; j < n; j += 2)
      {
        const float a = mLine.readInt(dOther + j), b = mLine.readInt(dCand + j);
        acc += a * b;
        e += b * b;
      }
      const float score = acc / std::sqrt(e);
      if (score > bestScore)
      {
        bestScore = score;
        best = cand;
      }
    }
    return best;
  }

  DelayLine mLine;
  Head mHead[2]{ { 2.f, 0.f }, { 2.f, 0.5f } };
  float mWindow = 1920.f, mRatio = 2.f;
  int mSearch = 768, mCorrLen = 288;
};

} // namespace tsl
