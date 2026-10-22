// SPDX-License-Identifier: GPL-3.0-or-later
// Рендер заводских пресетов через полный движок: измерение уровня (RMS/пик), проверка
// NaN/клиппинга, рекомендации выравнивания громкости и WAV-превью для QA.
//   tsl_render --calibrate            — таблица уровней и рекомендуемых поправок
//   tsl_render --render <каталог>     — WAV-превью каждого пресета (стерео, 48 кГц, float)
// ВАЖНО: синтетический сигнал не заменяет прослушивания на реальной гитаре.
#include "../../Source/Audio/Engine.h"
#include "../../Source/Cabinet/Cabinet.h"
#include "../../Source/DSP/AudioFile.h"
#include "../../Source/Presets/Preset.h"
#include "../TestSignals.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace tsl;

namespace {

struct Result
{
  float rmsDb, peakDb;
  bool finite;
  std::vector<float> l, r;
};

Result renderPreset(const Preset& p, const std::vector<float>& input, double sr = 48000.0)
{
  Engine e;
  e.prepare(sr, 128);
  const ParamValues v = resolvePresetParams(p);
  for (int i = 0; i < kNumParams; ++i) e.setParam(i, v[static_cast<size_t>(i)]);
  if (p.hasChain) e.setChainConfig(p.chain);
  // Дать кабинету собрать IR и применить конфигурацию графа
  TransportInfo t;
  t.tempo = 120.0;
  t.hostTempoValid = true;
  {
    std::vector<float> z(4096, 0.f), zr(4096, 0.f);
    for (int k = 0; k < 4; ++k)
    {
      const float* in[2] = { z.data(), zr.data() };
      float* out[2] = { z.data(), zr.data() };
      e.process(in, 1, out, 2, 4096, t);
    }
    if (auto* cab = static_cast<Cabinet*>(e.module(ModuleId::Cabinet))) cab->waitIdle();
    for (int k = 0; k < 4; ++k)
    {
      const float* in[2] = { z.data(), zr.data() };
      float* out[2] = { z.data(), zr.data() };
      e.process(in, 1, out, 2, 4096, t);
    }
  }
  Result r;
  r.l = input;
  r.r.assign(input.size(), 0.f);
  for (size_t pos = 0; pos < input.size(); pos += 128)
  {
    const int n = static_cast<int>(std::min<size_t>(128, input.size() - pos));
    const float* in[2] = { r.l.data() + pos, r.r.data() + pos };
    float* out[2] = { r.l.data() + pos, r.r.data() + pos };
    e.process(in, 1, out, 2, n, t);
  }
  double acc = 0.0;
  float pk = 0.f;
  r.finite = true;
  const size_t from = static_cast<size_t>(sr * 0.25);
  for (size_t i = from; i < r.l.size(); ++i)
  {
    const float m = 0.5f * (r.l[i] + r.r[i]);
    acc += static_cast<double>(m) * m;
    pk = std::max({ pk, std::fabs(r.l[i]), std::fabs(r.r[i]) });
    if (!std::isfinite(r.l[i]) || !std::isfinite(r.r[i])) r.finite = false;
  }
  r.rmsDb = static_cast<float>(10.0 * std::log10(acc / std::max<size_t>(1, r.l.size() - from) + 1e-20));
  r.peakDb = 20.f * std::log10(std::max(pk, 1e-9f));
  return r;
}

} // namespace

int main(int argc, char** argv)
{
  const bool calibrate = argc > 1 && std::strcmp(argv[1], "--calibrate") == 0;
  const bool render = argc > 2 && std::strcmp(argv[1], "--render") == 0;
  if (!calibrate && !render)
  {
    std::printf("usage: tsl_render --calibrate | --render <dir>\n");
    return 1;
  }
  const float target = -20.f; // целевой RMS моно-суммы (дБFS) для синтетического сигнала
  const auto input = tsltest::pluckedGuitar(48000.f, 6.f, -12.f);
  int failures = 0;
  for (const auto& p : factoryPresets())
  {
    const Result r = renderPreset(p, input);
    const float currentTrim = p.params.count("global.output") ? p.params.at("global.output") : 0.f;
    const float trim = currentTrim + (target - r.rmsDb);
    if (calibrate)
      std::printf("%-26s %-9s rms %6.1f dBFS  peak %6.1f dBFS  -> output %+5.1f dB%s\n", p.name.c_str(), p.category.c_str(), r.rmsDb, r.peakDb,
                  trim, r.finite ? "" : "  NaN!");
    if (!r.finite) ++failures;
    if (render)
    {
      std::string file = std::string(argv[2]) + "/" + PresetLibrary::sanitizeFileName(p.name) + ".wav";
      writeWav(file, { r.l, r.r }, 48000.0);
    }
  }
  return failures == 0 ? 0 : 2;
}
