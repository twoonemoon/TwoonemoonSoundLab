// SPDX-License-Identifier: GPL-3.0-or-later
// Класс плагина: мост между iPlug2 (хост, параметры, состояние, UI) и движком tsl::Engine.
#pragma once

#include "IPlug_include_in_plug_hdr.h"

#include "Audio/Engine.h"
#include "Audio/Params.h"

#include <atomic>
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
  tsl::ChainOrder chainOrder() const { return mEngine.chainOrder(); }
  /** Изменение порядка цепочки — счётчик, по которому UI понимает, что нужно перерисоваться. */
  uint32_t stateSerial() const { return mStateSerial.load(); }

#if IPLUG_DSP
  void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnActivate(bool active) override;
  void ProcessMidiMsg(const iplug::IMidiMsg& msg) override;
#endif

  void OnIdle() override;

private:
  void initParams();

  tsl::Engine mEngine;
  std::vector<float> mInBuf[2], mOutBuf[2];
  int mBufSize = 0;
  std::atomic<uint32_t> mStateSerial{ 0 };
};
