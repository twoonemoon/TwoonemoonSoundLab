// SPDX-License-Identifier: GPL-3.0-or-later
// Хроматический тюнер: стрелка ±50 центов, строб, нота/октава, частота.
// Анализ выполняется в UI thread по данным из lock-free кольца движка.
#pragma once

#include "Controls.h"
#include "DSP/PitchDetector.h"

class TwoonemoonSoundLab;

namespace tslui {

class TunerView : public IControl
{
public:
  TunerView(const IRECT& r, TwoonemoonSoundLab* plugin);
  bool IsDirty() override;
  void Draw(IGraphics& g) override;

private:
  void pull();

  TwoonemoonSoundLab* mPlugin;
  tsl::PitchDetector mDetector;
  double mPreparedRate = 0.0;
  std::vector<float> mBuf;
  int mSinceEstimate = 0;
  float mFreq = 0.f, mCents = 0.f, mShownCents = 0.f, mClarity = 0.f;
  int mHoldFrames = 0;
  float mStrobePhase = 0.f;
  bool mActive = false;
};

} // namespace tslui
