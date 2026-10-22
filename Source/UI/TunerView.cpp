// SPDX-License-Identifier: GPL-3.0-or-later
#include "TunerView.h"

#include "TwoonemoonSoundLab.h"

#include <cmath>

namespace tslui {

TunerView::TunerView(const IRECT& r, TwoonemoonSoundLab* plugin) : IControl(r), mPlugin(plugin)
{
  mIgnoreMouse = true;
  mBuf.resize(4096);
}

void TunerView::pull()
{
  tsl::Engine& e = mPlugin->engine();
  if (!e.isPrepared()) return;
  if (e.sampleRate() != mPreparedRate)
  {
    mPreparedRate = e.sampleRate();
    mDetector.prepare(mPreparedRate);
  }
  auto& ring = e.tunerRing();
  ring.discardOldest(static_cast<size_t>(mPreparedRate * 0.25)); // после паузы не анализировать устаревшее
  size_t got;
  while ((got = ring.pop(mBuf.data(), mBuf.size())) > 0)
  {
    mDetector.push(mBuf.data(), static_cast<int>(got));
    mSinceEstimate += static_cast<int>(got);
  }
  if (mSinceEstimate < static_cast<int>(mPreparedRate * 0.04)) return; // ~25 оценок/с
  mSinceEstimate = 0;
  const auto r = mDetector.estimate();
  if (r.frequency > 0.f && r.clarity > 0.82f)
  {
    mFreq = r.frequency;
    mClarity = r.clarity;
    const float a4 = static_cast<float>(mPlugin->GetParam(tsl::kTunerRef)->Value());
    mCents = tsl::PitchDetector::noteFor(mFreq, a4).cents;
    mHoldFrames = 30;
    mActive = true;
  }
  else if (mHoldFrames > 0)
    --mHoldFrames;
  else
    mActive = false;
}

bool TunerView::IsDirty()
{
  pull();
  return true; // анимация строба/стрелки
}

void TunerView::Draw(IGraphics& g)
{
  const float a4 = static_cast<float>(mPlugin->GetParam(tsl::kTunerRef)->Value());
  const auto note = tsl::PitchDetector::noteFor(mFreq, a4);
  mShownCents += (mCents - mShownCents) * 0.3f;
  const bool inTune = mActive && std::fabs(mShownCents) < 2.f;
  const IColor indicator = !mActive ? col::text3 : (inTune ? col::ok : (std::fabs(mShownCents) < 10.f ? col::warn : col::accent));

  // Нота
  const IRECT noteR = mRECT.GetFromTop(mRECT.H() * 0.42f);
  char buf[32];
  if (mActive)
  {
    g.DrawText(txt(64.f, inTune ? col::ok : col::text, font::semibold), note.name, noteR.GetHShifted(-14.f));
    snprintf(buf, sizeof(buf), "%d", note.octave);
    g.DrawText(txt(20.f, col::text2, font::medium, EAlign::Near), buf, IRECT(noteR.MW() + 30.f, noteR.MH(), noteR.MW() + 80.f, noteR.B - 8.f));
  }
  else
    g.DrawText(txt(20.f, col::text3, font::medium), "Сыграйте открытую струну", noteR);

  // Шкала ±50 центов
  const IRECT scale = mRECT.GetReducedFromTop(mRECT.H() * 0.45f).GetFromTop(mRECT.H() * 0.25f);
  const float cx = scale.MW();
  for (int c = -50; c <= 50; c += 5)
  {
    const float x = cx + scale.W() * 0.5f * c / 50.f;
    const float h = (c % 25 == 0) ? scale.H() * 0.7f : (c % 10 == 0 ? scale.H() * 0.45f : scale.H() * 0.25f);
    g.DrawLine(c == 0 ? col::ok : IColor(70, 255, 255, 255), x, scale.B - h, x, scale.B, nullptr, c == 0 ? 2.f : 1.f);
  }
  if (mActive)
  {
    const float x = cx + scale.W() * 0.5f * std::clamp(mShownCents, -50.f, 50.f) / 50.f;
    g.PathClear();
    g.PathMoveTo(x, scale.T - 6.f);
    g.PathLineTo(x - 6.f, scale.T - 16.f);
    g.PathLineTo(x + 6.f, scale.T - 16.f);
    g.PathClose();
    g.PathFill(indicator);
    g.DrawLine(indicator, x, scale.T - 6.f, x, scale.B, nullptr, 2.f);
  }

  // Строб: полосы движутся со скоростью, пропорциональной отклонению
  const IRECT strobe = mRECT.GetFromBottom(mRECT.H() * 0.22f).GetPadded(0.f, -6.f, 0.f, -18.f);
  g.PathClear();
  g.PathRoundRect(strobe, 6.f);
  g.PathFill(IColor(255, 11, 12, 15));
  if (mActive) mStrobePhase += mShownCents * 0.08f;
  g.PathClipRegion(strobe.GetPadded(-2.f));
  const float period = 18.f;
  const float off = std::fmod(mStrobePhase, period);
  for (float x = strobe.L - period * 2.f + off; x < strobe.R + period; x += period)
    g.FillRect(mActive ? IColor(inTune ? 200 : 120, indicator.R, indicator.G, indicator.B) : IColor(30, 255, 255, 255),
               IRECT(x, strobe.T, x + period * 0.5f, strobe.B));
  g.PathClipRegion();

  // Частота и отклонение
  if (mActive)
    snprintf(buf, sizeof(buf), "%.2f Hz   %+.1f ct", mFreq, mShownCents);
  else
    snprintf(buf, sizeof(buf), "A4 = %.1f Hz", a4);
  g.DrawText(txt(12.f, col::text2, font::mono), buf, mRECT.GetFromBottom(16.f));
}

} // namespace tslui
