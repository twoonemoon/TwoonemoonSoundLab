// SPDX-License-Identifier: GPL-3.0-or-later
// Синтез импульсных откликов кабинетов (собственные ресурсы проекта).
//
// ВАЖНО: это алгоритмическая модель, а не измерения реальных кабинетов. АЧХ строится
// из параметрического описания динамика (резонанс, акустическое оформление, зоны
// излома диффузора, спад ВЧ), модели микрофона и его положения; затем по АЧХ
// восстанавливается минимально-фазовый IR (кепстральный метод). Положение/угол/
// дистанция — художественно рассчитанная имитация известных акустических тенденций
// (центр диффузора ярче, край темнее, внеосевой спад ВЧ, эффект близости).
#pragma once

#include <vector>

namespace tsl {

constexpr int kNumCabSpeakers = 6; // + «User IR» в параметре
constexpr int kNumCabMics = 3;

struct CabMicSetup
{
  int speaker = 2;
  int mic = 0;
  float position = 0.3f; // 0 — центр (пылезащитный колпачок), 1 — край диффузора
  float distanceCm = 3.f;
  float angleDeg = 0.f;
};

const char* cabSpeakerName(int i) noexcept;
const char* cabMicName(int i) noexcept;

/** Синтезировать IR (нормированный по громкости). Не для audio thread. */
std::vector<float> synthesizeCabIR(const CabMicSetup& setup, double sampleRate);

/** АЧХ модели (дБ) на частоте f — для графика и тестов. */
float cabModelResponseDb(const CabMicSetup& setup, float f);

} // namespace tsl
