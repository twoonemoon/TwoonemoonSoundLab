// SPDX-License-Identifier: GPL-3.0-or-later
// Определение высоты тона для тюнера: нормированная функция квадратов разностей
// (NSDF, метод McLeod) через FFT-автокорреляцию, выбор «ключевого» максимума и
// параболическая интерполяция. prepare() — вне audio thread; push()/estimate() не аллоцируют
// и допустимы в audio thread (FFT-план создан заранее).
#pragma once

#include "FFT.h"

#include <vector>

namespace tsl {

class PitchDetector
{
public:
  struct Result
  {
    float frequency = 0.f; // Гц, 0 — не определено
    float clarity = 0.f;   // 0…1, «чистота» периодичности
    float level = 0.f;     // RMS окна
  };

  /** Не для audio thread. sampleRate — частота входного потока. */
  void prepare(double sampleRate, float minFreq = 30.f, float maxFreq = 1500.f);
  /** Добавить входные отсчёты (децимация ×2 внутри). */
  void push(const float* x, int n);
  /** Оценить высоту по последнему окну. */
  Result estimate();

  struct Note
  {
    int midi = -1;
    float cents = 0.f;
    const char* name = "";
    int octave = 0;
  };
  static Note noteFor(float freq, float a4 = 440.f);

private:
  double mRate = 24000.0; // после децимации
  int mWindow = 2048;
  int mMinLag = 16, mMaxLag = 800;
  std::vector<float> mRing;
  int mWrite = 0;
  float mDecPrev = 0.f;
  bool mDecPhase = false;
  float mLpState = 0.f;
  RealFFT mFft;
  std::vector<float> mTime, mRe, mIm, mAcf, mNsdf, mFrame;
};

} // namespace tsl
