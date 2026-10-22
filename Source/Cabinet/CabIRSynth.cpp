// SPDX-License-Identifier: GPL-3.0-or-later
#include "CabIRSynth.h"

#include "../DSP/DspCommon.h"
#include "../DSP/FFT.h"

#include <cmath>

namespace tsl {

namespace {

struct Peak
{
  float f, gainDb, widthOct;
};

struct SpeakerSpec
{
  const char* name;
  float fs, q;        // резонанс подвижной системы в оформлении
  bool openBack;
  float openHpHz;     // дополнительный спад НЧ открытого корпуса (акустическое «короткое замыкание»)
  Peak peaks[4];      // зоны излома диффузора и окраска
  float hfFc, hfOrder;
  float ripple;       // амплитуда «рваности» верха (дБ)
  float rippleSeed;
};

// Собственные вымышленные модели (названия и характеристики — оригинальные)
const SpeakerSpec kSpeakers[kNumCabSpeakers] = {
  { "Aurum 1x12 Open", 85.f, 0.9f, true, 115.f, { { 1200.f, 2.5f, 0.8f }, { 2600.f, 4.0f, 0.7f }, { 4200.f, 2.0f, 0.5f }, { 400.f, -1.0f, 1.0f } }, 5800.f, 3.f, 2.0f, 0.7f },
  { "Lumen 2x12 Open", 80.f, 0.8f, true, 105.f, { { 900.f, 1.5f, 1.0f }, { 2200.f, 3.0f, 0.6f }, { 3600.f, 4.0f, 0.6f }, { 250.f, 1.0f, 1.0f } }, 6800.f, 3.f, 2.5f, 1.9f },
  { "Monolith 4x12", 100.f, 1.3f, false, 0.f, { { 1600.f, 3.0f, 0.9f }, { 2800.f, 3.5f, 0.6f }, { 600.f, -2.0f, 1.0f }, { 180.f, 2.0f, 0.8f } }, 5200.f, 4.f, 2.5f, 2.9f },
  { "Vanta 4x12", 95.f, 1.15f, false, 0.f, { { 500.f, -3.0f, 1.2f }, { 2200.f, 3.0f, 0.8f }, { 4500.f, 4.0f, 0.6f }, { 120.f, 1.5f, 0.7f } }, 6200.f, 4.f, 2.0f, 4.3f },
  { "Cedar 2x12", 92.f, 1.05f, false, 0.f, { { 300.f, 2.0f, 1.0f }, { 1800.f, 2.5f, 0.9f }, { 3200.f, 1.0f, 0.6f }, { 800.f, -1.0f, 0.8f } }, 4600.f, 3.f, 1.5f, 5.1f },
  { "Ember 1x10", 120.f, 0.9f, true, 140.f, { { 1400.f, 3.0f, 1.0f }, { 3000.f, 2.0f, 0.6f }, { 650.f, 1.0f, 0.8f }, { 5000.f, -2.0f, 0.5f } }, 5000.f, 3.f, 2.0f, 6.7f },
};

struct MicSpec
{
  const char* name;
  float hpHz;
  Peak peaks[2];
  float lpHz, lpOrder;
  float proximityDb; // максимальный эффект близости (на 1 см)
};

const MicSpec kMics[kNumCabMics] = {
  { "Forge D7 (dynamic)", 55.f, { { 5200.f, 4.5f, 0.55f }, { 2500.f, 1.5f, 0.6f } }, 13000.f, 2.f, 6.f },
  { "Pearl C9 (condenser)", 22.f, { { 11000.f, 2.0f, 0.8f }, { 200.f, 0.f, 1.f } }, 20000.f, 2.f, 3.f },
  { "Silk R2 (ribbon)", 28.f, { { 300.f, 1.5f, 0.9f }, { 3000.f, -1.0f, 1.0f } }, 7000.f, 1.2f, 9.f },
};

inline float peakDb(const Peak& p, float f)
{
  if (p.gainDb == 0.f) return 0.f;
  const float o = std::log2(f / p.f) / p.widthOct;
  return p.gainDb * std::exp(-0.5f * o * o);
}

inline float hp2Db(float f, float fc, float q)
{
  const float r = f / fc;
  const float r2 = r * r;
  const float den = (1.f - r2) * (1.f - r2) + (r / q) * (r / q);
  return 10.f * std::log10(std::max(1e-12f, r2 * r2 / den));
}

inline float lpOrderDb(float f, float fc, float order)
{
  return -10.f * std::log10(1.f + std::pow(f / fc, 2.f * order));
}

inline float smoothstep(float a, float b, float x)
{
  const float t = clampv((x - a) / (b - a), 0.f, 1.f);
  return t * t * (3.f - 2.f * t);
}

float rawResponseDb(const CabMicSetup& s, float f)
{
  const SpeakerSpec& sp = kSpeakers[clampv(s.speaker, 0, kNumCabSpeakers - 1)];
  const MicSpec& mic = kMics[clampv(s.mic, 0, kNumCabMics - 1)];
  const float p = clampv(s.position, 0.f, 1.f);
  const float ang = clampv(s.angleDeg, 0.f, 45.f) / 45.f;
  const float d = clampv(s.distanceCm, 1.f, 30.f);
  const float dn = std::log(d) / std::log(30.f);
  f = std::max(f, 5.f);

  float db = hp2Db(f, sp.fs, sp.q);
  if (sp.openBack) db += 10.f * std::log10(1.f / (1.f + std::pow(sp.openHpHz / f, 2.f))) * 0.8f;
  for (const Peak& pk : sp.peaks) db += peakDb(pk, f);
  // Положение микрофона: центр ярче, край — темнее и плотнее в нижней середине
  const float fc = sp.hfFc * (1.25f - 0.7f * p) * (1.f - 0.25f * ang);
  db += lpOrderDb(f, fc, sp.hfOrder);
  db += peakDb({ 4200.f, 3.5f * (1.f - p) - 3.0f * p, 0.6f }, f);
  db += peakDb({ 280.f, 2.5f * p, 0.9f }, f);
  // «Рваность» верха — излом диффузора
  db += sp.ripple * std::sin(kTwoPi * std::log2(f) * 5.3f + sp.rippleSeed) * smoothstep(1500.f, 3000.f, f) *
        (1.f - smoothstep(8000.f, 14000.f, f));
  // Внеосевое положение: спад ВЧ
  db += -9.f * ang * smoothstep(1500.f, 6000.f, f);
  // Дистанция: эффект близости и лёгкий спад ВЧ
  db += mic.proximityDb * (1.f - dn) / (1.f + (f / 150.f) * (f / 150.f));
  db += -2.5f * dn * smoothstep(4000.f, 12000.f, f);
  // Микрофон
  db += hp2Db(f, mic.hpHz, 0.7f);
  for (const Peak& pk : mic.peaks) db += peakDb(pk, f);
  db += lpOrderDb(f, mic.lpHz, mic.lpOrder);
  return db;
}

float normalizationDb(const CabMicSetup& s)
{
  // Средняя мощность в полосе 120 Гц … 5 кГц (лог-сетка) → 0 дБ
  double acc = 0.0;
  const int N = 64;
  for (int i = 0; i < N; ++i)
  {
    const float f = 120.f * std::pow(5000.f / 120.f, static_cast<float>(i) / (N - 1));
    acc += std::pow(10.0, rawResponseDb(s, f) / 10.0);
  }
  return static_cast<float>(10.0 * std::log10(acc / N));
}

} // namespace

const char* cabSpeakerName(int i) noexcept { return kSpeakers[clampv(i, 0, kNumCabSpeakers - 1)].name; }
const char* cabMicName(int i) noexcept { return kMics[clampv(i, 0, kNumCabMics - 1)].name; }

float cabModelResponseDb(const CabMicSetup& s, float f) { return rawResponseDb(s, f) - normalizationDb(s); }

std::vector<float> synthesizeCabIR(const CabMicSetup& s, double sampleRate)
{
  int N = 8192;
  while (N < sampleRate * 0.16) N <<= 1;
  const int half = N / 2;
  const float norm = normalizationDb(s);

  RealFFT fft;
  fft.prepare(N);
  std::vector<float> re(static_cast<size_t>(half)), im(static_cast<size_t>(half)), time(static_cast<size_t>(N));

  // ln|H| на сетке бинов (упакованный формат: re[0] — DC, im[0] — Найквист)
  auto lnMag = [&](int k) {
    const float f = static_cast<float>(k) * static_cast<float>(sampleRate) / static_cast<float>(N);
    float db = rawResponseDb(s, f) - norm;
    db = std::max(db, -120.f);
    return db * 0.1151292546f; // ln(10)/20
  };
  for (int k = 1; k < half; ++k)
  {
    re[static_cast<size_t>(k)] = lnMag(k);
    im[static_cast<size_t>(k)] = 0.f;
  }
  re[0] = lnMag(0);
  im[0] = lnMag(half);

  // Вещественный кепстр
  fft.inverse(re.data(), im.data(), time.data());
  // Свёртка кепстра в каузальный (минимально-фазовый)
  for (int n = 1; n < half; ++n) time[static_cast<size_t>(n)] *= 2.f;
  for (int n = half + 1; n < N; ++n) time[static_cast<size_t>(n)] = 0.f;
  fft.forward(time.data(), re.data(), im.data());
  // exp() комплексного спектра
  for (int k = 1; k < half; ++k)
  {
    const float m = std::exp(re[static_cast<size_t>(k)]);
    const float ph = im[static_cast<size_t>(k)];
    re[static_cast<size_t>(k)] = m * std::cos(ph);
    im[static_cast<size_t>(k)] = m * std::sin(ph);
  }
  re[0] = std::exp(re[0]);
  im[0] = std::exp(im[0]);
  fft.inverse(re.data(), im.data(), time.data());

  const int length = static_cast<int>(sampleRate * 0.045);
  std::vector<float> ir(static_cast<size_t>(length));
  for (int i = 0; i < length; ++i) ir[static_cast<size_t>(i)] = time[static_cast<size_t>(i)];

  // Отражение от пола/стены: путь длиннее прямого на Δ, уровень растёт с дистанцией
  const float dm = clampv(s.distanceCm, 1.f, 30.f) * 0.01f;
  const float delta = std::sqrt(dm * dm + 0.8f * 0.8f) - dm;
  const int refl = static_cast<int>(delta / 343.f * static_cast<float>(sampleRate));
  const float rg = 0.06f + 0.22f * (dm / 0.30f);
  std::vector<float> direct = ir;
  for (int i = refl; i < length; ++i) ir[static_cast<size_t>(i)] += rg * direct[static_cast<size_t>(i - refl)] * 0.8f;

  // Плавное затухание конца окна
  const int fade = length / 5;
  for (int i = 0; i < fade; ++i)
  {
    const float w = 0.5f + 0.5f * std::cos(kPi * static_cast<float>(i) / static_cast<float>(fade));
    ir[static_cast<size_t>(length - fade + i)] *= w;
  }
  for (auto& v : ir)
    if (!std::isfinite(v)) v = 0.f;
  return ir;
}

} // namespace tsl
