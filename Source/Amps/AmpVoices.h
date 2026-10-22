// SPDX-License-Identifier: GPL-3.0-or-later
// Описания 12 оригинальных моделей усилителей TwoonemoonSoundLab.
// Это собственные «схемы»: число каскадов, усиление и смещение каскадов, частоты
// сопрягающих конденсаторов и ёмкостей Миллера, шунтирование катодов, номиналы
// tone stack, характер оконечного каскада, ООС и проседание питания.
// Имена — оригинальные; модели не являются копиями существующих усилителей.
#pragma once

#include "ToneStack.h"

namespace tsl {

enum class AmpCategory : int { Clean, Crunch, HighGain, Acoustic };

struct TriodeStageSpec
{
  float gain;           // усиление на входе нелинейности (нормированное)
  float bias;           // смещение рабочей точки −1…1 (отрицательное — к отсечке)
  float gridK;          // сила сдвига смещения сеточным током (блокинг, «дыхание» динамики)
  float gridReleaseMs;  // восстановление смещения
  float couplingHz;     // ФВЧ сопрягающего конденсатора перед каскадом
  float millerHz;       // ФНЧ ёмкости Миллера после каскада
  float cathodeLowGain; // усиление НЧ при частично шунтированном катоде (1 = полностью шунтирован)
  float cathodeHz;      // граница катодной полки
};

/** Параметры акустического тракта (модели категории Acoustic). */
struct AcousticSpec
{
  float bodyHz[3];    // резонансы корпуса: воздух (Гельмгольц), верхняя дека, нижняя дека
  float bodyQ[3];
  float bodyMaxDb[3]; // при Body = 10
  float piezoNotchHz; // область «кряка» пьезо
  float warmthHz;     // ФНЧ «теплоты» тракта
  float roomScale;    // насколько выражено пространство при Room
};

struct AmpVoice
{
  const char* name;
  const char* description; // краткое описание для UI (рус.)
  AmpCategory category;

  // Вход
  float inputHpHz;
  float brightDb, brightHz; // встроенный bright-канал (сильнее при малом Gain), активируется Bright
  float gainMinDb, gainMaxDb;

  // Предусилитель
  int numStages;
  TriodeStageSpec stages[4];
  int toneStackAfter; // после какого каскада стоит tone stack (0-based)
  ToneStackComponents ts;
  float toneStackMakeupDb;
  bool cathodeFollower;

  // Оконечный каскад
  float masterMinDb, masterMaxDb;
  float paHardness;  // резкость характеристики ламп (softplus)
  float paBias;      // номинальное смещение (Bias = 5)
  float nfb;         // номинальная глубина ООС (Feedback = 5), 0…1
  float presenceHz, depthHz;
  float sag;         // номинальное проседание питания (Sag = 5)
  float xfmrHpHz, xfmrLpHz;

  float outputDb; // выравнивание громкости между моделями

  AcousticSpec acoustic; // используется только для AmpCategory::Acoustic
};

constexpr int kNumAmpModels = 12;

const AmpVoice& ampVoice(int index) noexcept;

} // namespace tsl
