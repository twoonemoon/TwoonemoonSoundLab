// SPDX-License-Identifier: GPL-3.0-or-later
#include "AmpVoices.h"

namespace tsl {

namespace {

constexpr double k = 1e3, M = 1e6, p = 1e-12, n = 1e-9;

// Номиналы tone stack (собственные наборы)
constexpr ToneStackComponents TS_Scoop{ 250 * k, 250 * k, 10 * k, 100 * k, 250 * p, 100 * n, 47 * n };
constexpr ToneStackComponents TS_Round{ 250 * k, 1 * M, 25 * k, 56 * k, 250 * p, 22 * n, 22 * n };
constexpr ToneStackComponents TS_Glass{ 220 * k, 500 * k, 20 * k, 82 * k, 330 * p, 47 * n, 22 * n };
constexpr ToneStackComponents TS_Edge{ 220 * k, 1 * M, 25 * k, 47 * k, 470 * p, 22 * n, 22 * n };
constexpr ToneStackComponents TS_Mid{ 220 * k, 1 * M, 25 * k, 33 * k, 500 * p, 22 * n, 22 * n };
constexpr ToneStackComponents TS_Tight{ 250 * k, 1 * M, 25 * k, 39 * k, 680 * p, 47 * n, 22 * n };
constexpr ToneStackComponents TS_Heavy{ 250 * k, 250 * k, 25 * k, 47 * k, 470 * p, 22 * n, 33 * n };
constexpr ToneStackComponents TS_Lead{ 220 * k, 1 * M, 50 * k, 33 * k, 560 * p, 22 * n, 22 * n };

constexpr AcousticSpec kNoAcoustic{ { 100, 200, 400 }, { 1, 1, 1 }, { 0, 0, 0 }, 1500, 20000, 0 };

const AmpVoice kVoices[kNumAmpModels] = {
  // ------------------------------------------------------------------ CLEAN
  { "Lumen Twin", "Кристальный clean с большим запасом, выразительная атака и верх", AmpCategory::Clean,
    25.f, 6.f, 2200.f, -8.f, 14.f,
    2,
    { { 1.0f, -0.05f, 0.25f, 60.f, 20.f, 14000.f, 1.0f, 200.f },
      { 0.8f, -0.10f, 0.20f, 80.f, 25.f, 12000.f, 1.0f, 200.f } },
    0, TS_Scoop, 16.f, false,
    -10.f, 10.f, 7.f, 0.30f, 0.55f, 3800.f, 95.f, 0.25f, 45.f, 11000.f,
    9.3f, kNoAcoustic },
  { "Opal Bloom", "Округлый тёплый clean, гладкая верхняя середина, плотное тело", AmpCategory::Clean,
    30.f, 0.f, 2000.f, -6.f, 16.f,
    2,
    { { 1.1f, 0.00f, 0.35f, 90.f, 25.f, 9000.f, 0.85f, 300.f },
      { 0.9f, -0.05f, 0.30f, 120.f, 30.f, 8000.f, 1.0f, 250.f } },
    0, TS_Round, 15.f, false,
    -8.f, 12.f, 6.f, 0.35f, 0.35f, 3200.f, 85.f, 0.45f, 55.f, 9000.f,
    2.8f, kNoAcoustic },
  { "Glass Garden", "Современный стеклянный clean, широкий запас и чувствительность к динамике", AmpCategory::Clean,
    35.f, 4.f, 3000.f, -10.f, 12.f,
    2,
    { { 0.9f, -0.08f, 0.15f, 50.f, 30.f, 16000.f, 1.0f, 200.f },
      { 0.7f, -0.10f, 0.15f, 60.f, 35.f, 15000.f, 1.0f, 200.f } },
    1, TS_Glass, 15.f, false,
    -12.f, 8.f, 9.f, 0.28f, 0.65f, 4500.f, 110.f, 0.15f, 40.f, 13000.f,
    13.8f, kNoAcoustic },
  // ------------------------------------------------------------------ CRUNCH
  { "Velvet Edge", "Лёгкий breakup: откликается на силу атаки, мягко уплотняется", AmpCategory::Crunch,
    30.f, 3.f, 2000.f, -4.f, 22.f,
    3,
    { { 1.4f, -0.05f, 0.50f, 120.f, 25.f, 10000.f, 0.8f, 350.f },
      { 1.6f, -0.10f, 0.45f, 140.f, 40.f, 9000.f, 1.0f, 300.f },
      { 1.0f, -0.15f, 0.30f, 160.f, 30.f, 8000.f, 1.0f, 300.f } },
    1, TS_Edge, 15.f, true,
    -6.f, 16.f, 6.f, 0.30f, 0.25f, 3000.f, 90.f, 0.60f, 60.f, 8500.f,
    -1.8f, kNoAcoustic },
  { "Brass Circuit", "Плотный упругий crunch для роковых ритм-партий", AmpCategory::Crunch,
    40.f, 4.f, 2500.f, -8.f, 28.f,
    3,
    { { 1.8f, -0.05f, 0.40f, 90.f, 35.f, 11000.f, 0.7f, 500.f },
      { 2.2f, -0.10f, 0.35f, 100.f, 60.f, 9500.f, 1.0f, 400.f },
      { 1.4f, -0.15f, 0.30f, 120.f, 40.f, 8000.f, 1.0f, 300.f } },
    2, TS_Mid, 17.f, true,
    -6.f, 14.f, 8.f, 0.25f, 0.40f, 3500.f, 100.f, 0.40f, 70.f, 8000.f,
    -4.1f, kNoAcoustic },
  { "Rust Monarch", "Грубый насыщенный crunch с богатой серединой", AmpCategory::Crunch,
    45.f, 2.f, 1800.f, -6.f, 32.f,
    3,
    { { 2.2f, 0.00f, 0.55f, 110.f, 40.f, 8500.f, 0.65f, 600.f },
      { 2.6f, -0.12f, 0.50f, 130.f, 70.f, 8000.f, 1.0f, 400.f },
      { 1.8f, -0.20f, 0.40f, 150.f, 60.f, 7000.f, 1.0f, 300.f } },
    2, TS_Edge, 16.f, true,
    -4.f, 16.f, 6.5f, 0.22f, 0.30f, 3000.f, 90.f, 0.55f, 65.f, 7200.f,
    -4.9f, kNoAcoustic },
  // ------------------------------------------------------------------ HIGH GAIN
  { "Thorn Drive", "Плотный modern rock / high-gain с хорошей артикуляцией аккордов", AmpCategory::HighGain,
    50.f, 3.f, 2500.f, -6.f, 36.f,
    4,
    { { 2.0f, -0.05f, 0.35f, 80.f, 45.f, 12000.f, 0.6f, 700.f },
      { 2.6f, -0.12f, 0.35f, 90.f, 90.f, 10000.f, 0.9f, 500.f },
      { 2.4f, -0.15f, 0.30f, 90.f, 70.f, 9000.f, 1.0f, 400.f },
      { 1.4f, -0.20f, 0.25f, 100.f, 50.f, 8000.f, 1.0f, 300.f } },
    3, TS_Tight, 17.f, true,
    -6.f, 12.f, 9.f, 0.25f, 0.45f, 4000.f, 105.f, 0.35f, 75.f, 8500.f,
    -5.0f, kNoAcoustic },
  { "Iron Veil", "Tight modern metal: быстрый низ, чёткий отклик на palm-mute", AmpCategory::HighGain,
    70.f, 2.f, 3000.f, -6.f, 40.f,
    4,
    { { 2.4f, -0.05f, 0.25f, 60.f, 90.f, 13000.f, 0.5f, 900.f },
      { 2.8f, -0.12f, 0.30f, 70.f, 160.f, 11000.f, 0.85f, 600.f },
      { 2.6f, -0.15f, 0.25f, 70.f, 110.f, 9500.f, 1.0f, 400.f },
      { 1.5f, -0.20f, 0.20f, 80.f, 70.f, 8500.f, 1.0f, 300.f } },
    3, TS_Heavy, 17.f, true,
    -8.f, 10.f, 11.f, 0.22f, 0.55f, 4200.f, 110.f, 0.20f, 80.f, 9000.f,
    -4.6f, kNoAcoustic },
  { "Catacomb", "Экстремально тяжёлый djent/metal: собранный низ, детальная атака", AmpCategory::HighGain,
    90.f, 5.f, 3500.f, -4.f, 44.f,
    4,
    { { 2.6f, -0.04f, 0.20f, 50.f, 120.f, 14000.f, 0.45f, 1100.f },
      { 3.0f, -0.12f, 0.25f, 60.f, 220.f, 12000.f, 0.8f, 700.f },
      { 2.8f, -0.16f, 0.20f, 60.f, 140.f, 10000.f, 1.0f, 500.f },
      { 1.6f, -0.20f, 0.18f, 70.f, 90.f, 9000.f, 1.0f, 300.f } },
    3, TS_Tight, 18.f, true,
    -10.f, 8.f, 12.f, 0.20f, 0.60f, 4800.f, 85.f, 0.12f, 70.f, 9500.f,
    -4.3f, kNoAcoustic },
  { "Abyssal Lead", "Насыщенный lead с длинным sustain для соло", AmpCategory::HighGain,
    45.f, 2.f, 2000.f, -4.f, 42.f,
    4,
    { { 2.2f, -0.05f, 0.45f, 140.f, 40.f, 9000.f, 0.7f, 500.f },
      { 2.6f, -0.10f, 0.45f, 160.f, 70.f, 8000.f, 1.0f, 400.f },
      { 2.4f, -0.15f, 0.40f, 160.f, 60.f, 7000.f, 1.0f, 300.f },
      { 1.6f, -0.20f, 0.35f, 180.f, 45.f, 6500.f, 1.0f, 300.f } },
    3, TS_Lead, 16.f, true,
    -4.f, 14.f, 7.f, 0.30f, 0.30f, 3400.f, 95.f, 0.50f, 60.f, 7500.f,
    -5.6f, kNoAcoustic },
  // ------------------------------------------------------------------ ACOUSTIC
  { "Silver Current Acoustic", "Чистый преамп для пьезо и DI: прозрачность и контроль резкости", AmpCategory::Acoustic,
    55.f, 0.f, 0.f, -12.f, 18.f,
    0, {}, 0, TS_Scoop, 0.f, false,
    0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
    12.7f, { { 105.f, 220.f, 440.f }, { 4.f, 3.5f, 2.5f }, { 4.f, 3.f, 2.f }, 1500.f, 16000.f, 0.6f } },
  { "Cedar Room Acoustic", "Тёплый акустический канал: окраска корпуса и пространство", AmpCategory::Acoustic,
    45.f, 0.f, 0.f, -12.f, 18.f,
    0, {}, 0, TS_Scoop, 0.f, false,
    0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
    12.8f, { { 92.f, 196.f, 385.f }, { 5.f, 4.f, 3.f }, { 7.f, 5.f, 3.5f }, 1300.f, 11000.f, 1.0f } },
};

} // namespace

const AmpVoice& ampVoice(int index) noexcept
{
  if (index < 0) index = 0;
  if (index >= kNumAmpModels) index = kNumAmpModels - 1;
  return kVoices[index];
}

} // namespace tsl
