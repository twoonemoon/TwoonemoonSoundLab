// SPDX-License-Identifier: GPL-3.0-or-later
// Замер нагрузки CPU и добавленной задержки движка. Однопоточный офлайн-прогон:
// доля реального времени = время обработки / длительность аудио (одно ядро).
#include "../../Source/Amps/Amp.h"
#include "../../Source/Audio/Engine.h"
#include "../TestSignals.h"

#include <chrono>
#include <cstdio>
#include <functional>
#include <string>

using namespace tsl;

namespace {

struct Chain
{
  const char* name;
  std::function<void(Engine&)> setup;
};

void allOff(Engine& e)
{
  for (int i = 0; i < kNumModules; ++i) e.setParam(moduleEnableParam(static_cast<ModuleId>(i)), 0.f);
}

double runOnce(const Chain& c, double sr, int block, double seconds, int osIdx)
{
  Engine e;
  e.prepare(sr, block);
  allOff(e);
  e.setParam(kOversampling, static_cast<float>(osIdx));
  c.setup(e);
  // Дать фоновому потоку кабинета собрать ядра
  auto warm = tsltest::pluckedGuitar(static_cast<float>(sr), 0.5f);
  std::vector<float> r(warm.size(), 0.f);
  TransportInfo t;
  for (size_t p = 0; p + static_cast<size_t>(block) <= warm.size(); p += static_cast<size_t>(block))
  {
    const float* in[2] = { warm.data() + p, r.data() + p };
    float* out[2] = { warm.data() + p, r.data() + p };
    e.process(in, 1, out, 2, block, t);
  }
  auto sig = tsltest::pluckedGuitar(static_cast<float>(sr), static_cast<float>(seconds));
  std::vector<float> l = sig, rr(sig.size(), 0.f);
  const auto t0 = std::chrono::steady_clock::now();
  for (size_t p = 0; p + static_cast<size_t>(block) <= l.size(); p += static_cast<size_t>(block))
  {
    const float* in[2] = { l.data() + p, rr.data() + p };
    float* out[2] = { l.data() + p, rr.data() + p };
    e.process(in, 1, out, 2, block, t);
  }
  const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  return el / seconds;
}

} // namespace

int main()
{
  const Chain chains[] = {
    { "gate+amp(Brass)+cab", [](Engine& e) { e.setParam(kGateOn, 1); e.setParam(kAmpOn, 1); e.setParam(kAmpModel, 4); e.setParam(kCabOn, 1); } },
    { "clean amp(Lumen)+cab", [](Engine& e) { e.setParam(kAmpOn, 1); e.setParam(kAmpModel, 0); e.setParam(kCabOn, 1); } },
    { "heavy: gate+dist+amp(Catacomb)+cab", [](Engine& e) {
        e.setParam(kGateOn, 1); e.setParam(kDistOn, 1); e.setParam(kAmpOn, 1); e.setParam(kAmpModel, 8); e.setParam(kCabOn, 1); } },
    { "max: all modules + dual cab", [](Engine& e) {
        for (int i = 0; i < kNumModules; ++i) e.setParam(moduleEnableParam(static_cast<ModuleId>(i)), 1.f);
        e.setParam(kAmpModel, 8); e.setParam(kCabBOn, 1); e.setParam(kCabBPan, 60.f); } },
  };
  std::printf("Chain                                    | SR     | block | OS | CPU (1 core) | latency\n");
  std::printf("-----------------------------------------|--------|-------|----|--------------|--------\n");
  for (const auto& c : chains)
  {
    for (double sr : { 44100.0, 48000.0, 96000.0 })
      for (int block : { 32, 64, 128 })
      {
        if (sr != 48000.0 && block != 64) continue;
        for (int os : { 0, 1, 2 })
        {
          if (!(sr == 48000.0 && block == 64) && os != 1) continue;
          const double rt = runOnce(c, sr, block, 6.0, os);
          Engine probe;
          probe.prepare(sr, block);
          std::printf("%-41s| %6.0f | %5d | %dx | %10.2f %% | %d smp\n", c.name, sr, block, os == 0 ? 2 : os == 1 ? 4 : 8,
                      rt * 100.0, probe.latencySamples());
        }
      }
  }
  Amp a;
  a.prepare({ 48000.0, 64 });
  std::printf("\nГрупповая задержка оверсэмплинга усилителя (НЧ, 4x): %.2f отсчёта при 48 кГц (%.3f мс)\n",
              a.oversamplingGroupDelay(), a.oversamplingGroupDelay() / 48.0);
  return 0;
}
