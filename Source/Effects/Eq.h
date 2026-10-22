// SPDX-License-Identifier: GPL-3.0-or-later
// Параметрический эквалайзер: 5 полос (Bell/Low Shelf/High Shelf/Notch), HPF/LPF 12/24 дБ/окт,
// выходной уровень. Сглаживание параметров (лог-частота, gain, Q) по блокам.
// Анализатор: моно-сумма входа и выхода в lock-free кольца для UI.
#pragma once

#include "../Audio/Module.h"
#include "../DSP/Biquad.h"
#include "../DSP/Smoother.h"
#include "../DSP/SpscRing.h"

#include <functional>

namespace tsl {

class Eq final : public Module
{
public:
  static constexpr int kBands = 5;

  Eq() noexcept : Module(ModuleId::Eq) {}

  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

  SpscRing<float>& preRing() noexcept { return mPreRing; }
  SpscRing<float>& postRing() noexcept { return mPostRing; }

  /** Суммарная АЧХ (дБ) по значениям параметров — для кривой в UI (не audio thread). */
  static double responseDb(const std::function<float(int)>& param, double f, double sampleRate);

private:
  struct BandState
  {
    OnePoleSmoother logFreq, gain, q;
    int type = 0;
    bool on = true;
    Biquad bq[2];
    float lastF = -1.f, lastG = -100.f, lastQ = -1.f;
    int lastType = -1;
  };

  void designBand(BandState& b, bool force) noexcept;
  void designCuts() noexcept;

  BandState mBand[kBands];
  Biquad mLow[2][2], mHigh[2][2]; // [ступень][канал]
  float mLowHz = 20.f, mHighHz = 20000.f;
  int mLowSlope = 0, mHighSlope = 0;
  bool mCutsDirty = true;
  LinearSmoother mOut;
  SpscRing<float> mPreRing, mPostRing;
  std::vector<float> mMono;
};

} // namespace tsl
