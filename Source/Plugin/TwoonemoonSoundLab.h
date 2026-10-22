// SPDX-License-Identifier: GPL-3.0-or-later
// Класс плагина: мост между iPlug2 (хост, параметры, состояние, UI) и движком tsl::Engine.
#pragma once

#include "IPlug_include_in_plug_hdr.h"

#include "Audio/Engine.h"
#include "Audio/Params.h"
#include "Presets/Preset.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class TwoonemoonSoundLab final : public iplug::Plugin
{
public:
  explicit TwoonemoonSoundLab(const iplug::InstanceInfo& info);

  // --- Состояние ---
  bool SerializeState(iplug::IByteChunk& chunk) const override;
  int UnserializeState(const iplug::IByteChunk& chunk, int startPos) override;
  void OnRestoreState() override;

  // --- Доступ для UI (UI thread) ---
  tsl::Engine& engine() { return mEngine; }
  void setChainOrder(const tsl::ChainOrder& order);
  void setChainConfig(const tsl::ChainConfig& cfg);
  tsl::ChainConfig chainConfig() const { return mEngine.chainConfig(); }
  tsl::ChainOrder chainOrder() const { return mEngine.chainOrder(); }
  /** Изменение порядка цепочки — счётчик, по которому UI понимает, что нужно перерисоваться. */
  uint32_t stateSerial() const { return mStateSerial.load(); }
  /** Загрузить пользовательский IR в слот кабинета (0 = A, 1 = B); путь сохраняется в состоянии проекта. */
  void loadUserIR(int slot, const std::string& path);
  std::string userIRPath(int slot) const { return mUserIR[slot & 1]; }
  /** UI подписывается на изменения параметров (вызывается в UI thread). */
  void setUIParamListener(std::function<void(int)> fn) { mUIParamListener = std::move(fn); }
  void OnParamChangeUI(int paramIdx, iplug::EParamSource source) override;
  void OnUIClose() override;
  void EndInformHostOfParamChangeFromUI(int paramIdx) override;

  // --- Пресеты (UI thread) ---
  tsl::PresetLibrary& library();
  /** Загрузить пресет: короткое затухание, применение в момент тишины, нарастание. */
  void loadPreset(const tsl::Preset& p, bool withFade = true);
  /** Снимок текущего состояния (все параметры, граф, IR). */
  tsl::Preset snapshot() const;
  const std::string& currentPresetName() const { return mCurrentName; }
  const std::string& currentPresetId() const { return mCurrentId; }
  /** Отличается ли состояние от загруженного пресета (без учёта Input/Output/Bypass/темпа). */
  bool presetModified() const;
  /** Отметить текущее состояние как сохранённый пресет (после Save/Save As). */
  void markSaved(const tsl::Preset& p);
  /** Соседний пресет в общем списке (заводские + пользовательские), dir = ±1. */
  void stepPreset(int dir);
  std::vector<const tsl::Preset*> allPresets();

  // A/B
  int abSlot() const { return mABActive; }
  void switchAB();
  void copyToOtherAB();

  // Undo / Redo
  void pushUndo();
  bool undo();
  bool redo();
  bool canUndo() const { return mUndoPos > 0; }
  bool canRedo() const { return mUndoPos + 1 < static_cast<int>(mUndo.size()); }

#if IPLUG_DSP
  void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnActivate(bool active) override;
  void ProcessMidiMsg(const iplug::IMidiMsg& msg) override;
#endif

  void OnIdle() override;

#if defined(CLAP_API)
  // Исправления поведения CLAP-обёртки (см. docs/TESTING.md)
  bool stateSave(const clap_ostream* pStream) noexcept override;
  bool stateLoad(const clap_istream* pStream) noexcept override;
  bool paramsValueToText(clap_id paramIdx, double value, char* display, uint32_t size) noexcept override;
  bool audioPortsGetConfig(uint32_t index, clap_audio_ports_config* pConfig) const noexcept override;
#endif

private:
  void initParams();
  void updateTailSize();
  void applyNow(const tsl::ParamValues& v, const tsl::ChainConfig& chain);
  void finishPresetLoad();
  void handlePresetAudio(); // audio thread

  enum PresetState : int { kPresetIdle = 0, kPresetRequested, kPresetFading, kPresetApplied };

  tsl::Engine mEngine;
  std::vector<float> mInBuf[2], mOutBuf[2];
  int mBufSize = 0;
  std::atomic<uint32_t> mStateSerial{ 0 };
  std::string mUserIR[2];
  std::function<void(int)> mUIParamListener;

  // Пресеты
  std::unique_ptr<tsl::PresetLibrary> mLibrary;
  std::string mCurrentName = "Init", mCurrentId;
  tsl::ParamValues mLoadedValues{};
  tsl::ChainConfig mLoadedChain;
  tsl::ParamValues mPendingValues{};
  tsl::ChainConfig mPendingChain;
  std::string mPendingIR[2];
  std::atomic<int> mPresetState{ kPresetIdle };
  uint64_t mRequestBlocks = 0;
  std::chrono::steady_clock::time_point mRequestTime;
  tsl::Preset mAB[2];
  bool mABValid[2] = { false, false };
  int mABActive = 0;
  std::vector<tsl::Preset> mUndo;
  int mUndoPos = -1;
  bool mApplyingHistory = false;
  std::unique_ptr<tsl::Preset> mQueued;
  bool mQueuedFade = true;
};
