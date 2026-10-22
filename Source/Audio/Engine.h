// SPDX-License-Identifier: GPL-3.0-or-later
// Аудиодвижок TwoonemoonSoundLab: владеет модулями, порядком цепочки, уровнями
// и аналитическими буферами. Не зависит от плагинного фреймворка.
//
// Потоки:
//  * prepare() — вне audio thread (аллокации разрешены).
//  * setParam()/process() — audio thread.
//  * setChainOrder() — любой не-аудио поток; передача lock-free через атомарный
//    индекс двойного буфера, audio thread подхватывает новый порядок в начале блока
//    с коротким crossfade.
//  * Измерители, кольцевые буферы тюнера/анализатора — читаются UI без блокировок.
#pragma once

#include "Module.h"
#include "../DSP/Meter.h"
#include "../DSP/Smoother.h"
#include "../DSP/SpscRing.h"
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace tsl {

using ChainOrder = std::array<ModuleId, kNumModules>;

ChainOrder defaultChainOrder() noexcept;
/** Проверка, что порядок — перестановка всех модулей. */
bool isValidChainOrder(const ChainOrder& order) noexcept;

/**
 * Конфигурация графа: последовательная цепочка с необязательной параллельной секцией
 * [splitStart, splitEnd) в порядке order. Внутри секции модули с битом в laneB идут по пути B,
 * остальные — по пути A; на выходе секции пути смешиваются параметром Parallel A/B Mix.
 */
struct ChainConfig
{
  ChainOrder order = defaultChainOrder();
  bool parallel = false;
  uint8_t splitStart = 0, splitEnd = 0;
  uint32_t laneB = 0; // бит = static_cast<int>(ModuleId)

  bool operator==(const ChainConfig& o) const noexcept
  {
    return order == o.order && parallel == o.parallel && splitStart == o.splitStart && splitEnd == o.splitEnd && laneB == o.laneB;
  }
  bool operator!=(const ChainConfig& o) const noexcept { return !(*this == o); }
  bool onLaneB(ModuleId id) const noexcept { return (laneB >> static_cast<int>(id)) & 1u; }
};

bool isValidChainConfig(const ChainConfig& c) noexcept;

class Engine
{
public:
  Engine();
  ~Engine();

  void prepare(double sampleRate, int maxBlockSize);
  bool isPrepared() const noexcept { return mPrepared; }
  double sampleRate() const noexcept { return mSampleRate; }

  /** Audio thread: обновить значение параметра (plain). Дешёвая операция. */
  void setParam(int id, float plainValue) noexcept;
  float param(int id) const noexcept { return mParams[static_cast<size_t>(id)]; }

  /** Любой не-аудио поток. */
  void setChainOrder(const ChainOrder& order) noexcept;
  ChainOrder chainOrder() const noexcept;
  void setChainConfig(const ChainConfig& cfg) noexcept;
  ChainConfig chainConfig() const noexcept;

  /**
   * Audio thread. in/out — массивы каналов, nIn/nOut ∈ {1,2}. in и out могут совпадать.
   * Длина блока произвольная: внутри разбивается на куски ≤ maxBlockSize.
   */
  void process(const float* const* in, int nIn, float** out, int nOut, int numFrames, const TransportInfo& t) noexcept;

  void reset() noexcept;

  /** Переходное затухание всего выхода (смена пресета): audio thread. */
  void beginTransitionFade() noexcept { mTransition.setTarget(0.f); }
  bool transitionSilent() const noexcept { return mTransition.current() <= 0.f && !mTransition.isSmoothing(); }
  void endTransitionFade() noexcept { mTransition.setTarget(1.f); }
  /** Счётчик обработанных блоков — по нему UI понимает, идёт ли аудиопоток. */
  uint64_t processedBlocks() const noexcept { return mBlocks.load(std::memory_order_relaxed); }

  /** Суммарная добавленная задержка (отсчёты) — сообщается хосту. */
  int latencySamples() const noexcept;

  // --- UI-доступ (lock-free) ---
  LevelMeter& inputMeter() noexcept { return mInMeter; }
  LevelMeter& outputMeter() noexcept { return mOutMeter; }
  SpscRing<float>& tunerRing() noexcept { return mTunerRing; }
  SpscRing<float>& analyzerRing() noexcept { return mAnalyzerRing; }
  Module* module(ModuleId id) noexcept { return mModules[static_cast<size_t>(id)].get(); }
  /** Счётчик срабатываний защиты (NaN/Inf/перегруз) — для диагностики. */
  uint32_t safetyTrips() const noexcept { return mSafetyTrips.load(std::memory_order_relaxed); }

private:
  void processChunk(const float* const* in, int nIn, float** out, int nOut, int offset, int n,
                    const TransportInfo& t) noexcept;
  void updateModules() noexcept;
  void runModule(ModuleId id, AudioBlock& block) noexcept;

  bool mPrepared = false;
  double mSampleRate = 48000.0;
  int mMaxBlock = 512;

  ParamValues mParams{};
  bool mParamsDirty = true;

  std::array<std::unique_ptr<Module>, kNumModules> mModules;

  // Порядок цепочки: двойной буфер + атомарный индекс опубликованной версии
  std::array<ChainConfig, 2> mOrderSlots{};
  std::atomic<int> mPublishedOrder{ 0 };
  std::atomic<uint32_t> mOrderSerial{ 0 };
  uint32_t mAppliedSerial = 0;
  ChainConfig mActiveCfg{};
  ChainConfig mPendingCfg{};
  LinearSmoother mParallelA, mParallelB;
  bool mOrderFadeOut = false; // фаза: гасим выход перед сменой порядка
  LinearSmoother mOrderFade;

  // Плавное включение/выключение модулей
  std::array<LinearSmoother, kNumModules> mModuleMix;
  std::array<bool, kNumModules> mModuleActive{};

  LinearSmoother mInGain, mOutGain, mBypassMix, mTunerMute, mTransition;
  std::atomic<uint64_t> mBlocks{ 0 };

  // Рабочие буферы (выделены в prepare)
  std::vector<float> mL, mR, mDryL, mDryR, mKey, mInL, mInR, mBL, mBR;

  LevelMeter mInMeter, mOutMeter;
  SpscRing<float> mTunerRing, mAnalyzerRing;
  std::atomic<uint32_t> mSafetyTrips{ 0 };
};

} // namespace tsl
