// SPDX-License-Identifier: GPL-3.0-or-later
// Базовый интерфейс модуля сигнальной цепочки.
//
// Контракт потоков:
//  * prepare() — только вне audio thread (может аллоцировать).
//  * reset(), update(), process() — audio thread, без аллокаций, блокировок и I/O.
#pragma once

#include "Params.h"
#include <array>

namespace tsl {

/** Идентификаторы модулей цепочки. Значения сохраняются в пресетах (через строковые ключи). */
enum class ModuleId : int
{
  Gate = 0,
  Compressor,
  Boost,
  Overdrive,
  Distortion,
  Fuzz,
  Amp,
  Cabinet,
  Eq,
  Chorus,
  Phaser,
  Flanger,
  Tremolo,
  Vibrato,
  Rotary,
  EnvFilter,
  Delay,
  Reverb,
  Wah,
  Octave,
  Pitch,
  Harmonizer,
  Count
};

constexpr int kNumModules = static_cast<int>(ModuleId::Count);

const char* moduleKey(ModuleId id) noexcept;       // стабильный ключ для пресетов
const char* moduleDisplayName(ModuleId id) noexcept;
const char* moduleShortName(ModuleId id) noexcept; // 2–4 символа для компактных схем
ModuleId moduleIdForKey(const char* key) noexcept; // ModuleId::Count если не найден
int moduleEnableParam(ModuleId id) noexcept;       // ParamId параметра «On»

/** Текущие (plain) значения всех параметров. Пишется движком один раз за блок. */
using ParamValues = std::array<float, kNumParams>;

struct TransportInfo
{
  double tempo = 120.0;
  double ppqPos = 0.0;
  bool playing = false;
  bool hostTempoValid = false;
};

struct PrepareInfo
{
  double sampleRate = 48000.0;
  int maxBlockSize = 512;
};

/** Блок обработки. Пока сигнал моно, валиден только l; модуль, создающий стерео, заполняет r и ставит stereo=true. */
struct AudioBlock
{
  float* l = nullptr;
  float* r = nullptr;
  int numFrames = 0;
  bool stereo = false;
  const float* keyInput = nullptr; // «чистый» DI-сигнал после Input Trim (для ключа гейта и т.п.)
  const TransportInfo* transport = nullptr;
};

class Module
{
public:
  explicit Module(ModuleId id) noexcept : mId(id) {}
  virtual ~Module() = default;

  ModuleId id() const noexcept { return mId; }

  virtual void prepare(const PrepareInfo& info) = 0;
  virtual void reset() noexcept = 0;
  virtual void update(const ParamValues& p) noexcept = 0;
  virtual void process(AudioBlock& block) noexcept = 0;

  /** Добавляемая модулем задержка (отсчёты). */
  virtual int latencySamples() const noexcept { return 0; }
  /** Модуль имеет «хвост» (delay/reverb) — после выключения его надо доиграть/погасить. */
  virtual bool hasTail() const noexcept { return false; }

protected:
  const ModuleId mId;
  double mSampleRate = 48000.0;
  int mMaxBlock = 512;
};

} // namespace tsl
