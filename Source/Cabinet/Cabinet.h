// SPDX-License-Identifier: GPL-3.0-or-later
// Cab Studio: два кабинета/микрофона (A/B), свёртка с нулевой задержкой, собственные
// синтезированные IR или пользовательские WAV, Level/Pan/Phase/Delay alignment.
//
// Потоки:
//  * audio thread — только свёртка, публикация желаемой конфигурации (атомики) и
//    захват готового ядра (atomic exchange) с кроссфейдом;
//  * фоновый поток (worker) — чтение WAV, ресемплинг, синтез IR, создание ядер,
//    удаление отработавших ядер;
//  * UI — запрос загрузки файла и чтение статуса/превью (под mutex, без audio thread).
#pragma once

#include "../Audio/Module.h"
#include "../DSP/Biquad.h"
#include "../DSP/Convolver.h"
#include "../DSP/DelayLine.h"
#include "../DSP/Smoother.h"
#include "CabIRSynth.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tsl {

constexpr int kCabUserIRIndex = kNumCabSpeakers; // значение параметра Speaker = «User IR»

class Cabinet final : public Module
{
public:
  static constexpr int kPartSize = 64;
  static constexpr double kMaxIRSeconds = 0.35; // длиннее — это уже комната, обрезается с плавным затуханием

  Cabinet();
  ~Cabinet() override;

  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

  // ---- UI API (не audio thread) ----
  struct SlotInfo
  {
    std::string userPath;   // путь пользовательского IR (может быть пустым)
    std::string message;    // статус/ошибка для UI (рус.)
    bool error = false;
    int irLength = 0;       // в отсчётах текущей частоты
    double fileRate = 0.0;  // исходная частота файла
    int fileChannels = 0;
  };
  /** Асинхронно загрузить WAV в слот (0 = A, 1 = B). Пустой путь — очистить. */
  void requestUserIR(int slot, const std::string& path);
  SlotInfo slotInfo(int slot) const;
  /** Копия текущего IR слота для отрисовки формы волны. */
  std::vector<float> irPreview(int slot) const;
  uint32_t previewSerial() const noexcept { return mPreviewSerial.load(std::memory_order_acquire); }

  /** Ждать, пока worker обработает все запросы (для тестов/офлайн-рендера). */
  bool waitIdle(int timeoutMs = 3000);

private:
  struct Slot
  {
    // Желаемая конфигурация (пишет audio thread, читает worker)
    std::atomic<int> speaker{ 2 }, mic{ 0 };
    std::atomic<float> position{ 0.3f }, distance{ 3.f }, angle{ 0.f };
    std::atomic<uint32_t> cfgVersion{ 1 };
    uint32_t builtVersion = 0; // worker

    // Передача ядер
    std::atomic<ConvolutionKernel*> pending{ nullptr };
    std::atomic<ConvolutionKernel*> retired{ nullptr };

    // Audio thread
    ConvolutionKernel* active = nullptr;
    ConvolutionKernel* fadingFrom = nullptr;
    int fadePos = 0;
    ZeroLatencyConvolver conv[2];
    DelayLine delay[2];
    LinearSmoother level, panL, panR, onMix, delaySmp;
    bool wasOn = false;

    // Worker / UI (под mMutex)
    std::string userPath, loadedPath;
    uint32_t userRequest = 0, userLoaded = 0;
    std::vector<float> userIROrig; // моно, исходная частота
    double userRate = 0.0;
    SlotInfo info;
    std::vector<float> preview;
  };

  void workerLoop();
  bool processRequests(); // worker: true если что-то сделано
  ConvolutionKernel* buildKernel(Slot& s, int slotIdx, double sampleRate); // под mMutex
  void collectRetired();

  Slot mSlot[2];
  int mFadeLen = 1024;
  bool mSnapOnUpdate = true;
  Biquad mLowCut[2], mHighCut[2];
  float mLowCutHz = 60.f, mHighCutHz = 12000.f;
  std::vector<float> mTmp[2][2]; // [slot][channel]
  std::vector<float> mInCopy[2];

  RealFFT mBuildFft; // под mMutex
  mutable std::mutex mMutex;
  std::thread mWorker;
  std::atomic<bool> mQuit{ false };
  std::atomic<bool> mBusy{ false };
  std::atomic<uint32_t> mPreviewSerial{ 0 };
  std::atomic<double> mBuildRate{ 48000.0 };
};

} // namespace tsl
