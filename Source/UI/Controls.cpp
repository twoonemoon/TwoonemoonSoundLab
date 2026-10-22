// SPDX-License-Identifier: GPL-3.0-or-later
#include "Controls.h"

#include "DSP/Meter.h"

#include <cmath>

namespace tslui {

namespace {

constexpr float kStartAngle = -135.f;
constexpr float kEndAngle = 135.f;

std::string displayWithUnit(const IParam* p)
{
  if (!p) return {};
  WDL_String s;
  p->GetDisplay(s);
  std::string out = s.Get();
  const char* lbl = p->GetLabel();
  if (lbl && *lbl && p->Type() != IParam::kTypeEnum && p->Type() != IParam::kTypeBool)
  {
    if (lbl[0] != ':') out += " ";
    out += lbl;
  }
  return out;
}

IStrokeOptions roundStroke()
{
  IStrokeOptions o;
  o.mCapOption = ELineCap::Round;
  o.mJoinOption = ELineJoin::Round;
  return o;
}

IColor mix(const IColor& a, const IColor& b, float t)
{
  return IColor(static_cast<int>(a.A + (b.A - a.A) * t), static_cast<int>(a.R + (b.R - a.R) * t),
                static_cast<int>(a.G + (b.G - a.G) * t), static_cast<int>(a.B + (b.B - a.B) * t));
}

} // namespace

// ============================================================================
// Knob

Knob::Knob(const IRECT& bounds, int paramIdx, const char* label, KnobStyle style, const IColor& accent, bool bipolar)
: IKnobControlBase(bounds, paramIdx, EDirection::Vertical, 4.0)
, mLabel(label ? label : "")
, mStyle(style)
, mAccent(accent)
, mBipolar(bipolar)
{
  OnResize();
}

void Knob::OnResize()
{
  const float labelH = std::max(14.f, mRECT.H() * 0.18f);
  const IRECT top = mRECT.GetReducedFromBottom(labelH);
  const float d = std::min(top.W(), top.H());
  mKnobRect = top.GetCentredInside(d, d);
  mLabelRect = mRECT.GetFromBottom(labelH);
  mValueRect = mLabelRect;
  mTargetRECT = mKnobRect.Union(mLabelRect);
}

void Knob::Draw(IGraphics& g)
{
  const float cx = mKnobRect.MW(), cy = mKnobRect.MH();
  const float R = mKnobRect.W() * 0.5f;
  const float v = static_cast<float>(GetValue());
  const float angle = kStartAngle + v * (kEndAngle - kStartAngle);
  const bool small = mStyle == KnobStyle::Small;

  // Дуга-трек
  const float arcR = R - 1.5f;
  const float arcW = small ? 2.f : 2.6f;
  g.PathClear();
  g.PathArc(cx, cy, arcR, kStartAngle, kEndAngle);
  g.PathStroke(col::bg3, arcW, roundStroke());
  // Дуга значения
  const float from = mBipolar ? 0.f : kStartAngle;
  if (std::fabs(angle - from) > 0.5f)
  {
    g.PathClear();
    g.PathArc(cx, cy, arcR, std::min(from, angle), std::max(from, angle));
    g.PathStroke(mAccent, arcW, roundStroke());
    if (mMouseDown || mHover)
    {
      g.PathClear();
      g.PathArc(cx, cy, arcR, std::min(from, angle), std::max(from, angle));
      g.PathStroke(IColor(50, mAccent.R, mAccent.G, mAccent.B), arcW + 4.f, roundStroke());
    }
  }

  // Корпус ручки
  const float bodyR = R * (small ? 0.70f : 0.74f);
  // Тень
  g.PathClear();
  g.PathCircle(cx, cy + bodyR * 0.10f, bodyR * 1.04f);
  g.PathFill(IPattern::CreateRadialGradient(cx, cy + bodyR * 0.12f, bodyR * 1.25f,
                                            { { IColor(160, 0, 0, 0), 0.75f }, { IColor(0, 0, 0, 0), 1.f } }));
  // Накатанный обод
  IColor rimBase = col::steelLo, rimHi = col::steelMid;
  IColor faceBase = col::steel, faceHi = col::steelHi;
  if (mStyle == KnobStyle::Pearl) { faceBase = col::pearlShade; faceHi = col::pearl; rimBase = IColor(255, 150, 142, 140); rimHi = col::pearlShade; }
  if (mStyle == KnobStyle::Black) { faceBase = IColor(255, 40, 41, 46); faceHi = IColor(255, 92, 94, 102); rimBase = IColor(255, 18, 18, 21); rimHi = IColor(255, 52, 54, 60); }
  g.PathClear();
  g.PathCircle(cx, cy, bodyR);
  g.PathFill(IPattern::CreateLinearGradient(cx, cy - bodyR, cx, cy + bodyR, { { rimHi, 0.f }, { rimBase, 1.f } }));
  if (!small)
  {
    const int teeth = 36;
    for (int i = 0; i < teeth; ++i)
    {
      const float a = (static_cast<float>(i) / teeth) * 6.2831853f;
      const float c = std::cos(a), s = std::sin(a);
      g.PathClear();
      g.PathMoveTo(cx + c * bodyR * 0.86f, cy + s * bodyR * 0.86f);
      g.PathLineTo(cx + c * bodyR * 0.99f, cy + s * bodyR * 0.99f);
      const float light = 0.5f - 0.5f * s; // светлее сверху
      g.PathStroke(IColor(static_cast<int>(20 + 50 * light), 255, 255, 255), 1.f);
    }
  }
  // Шапка
  const float capR = bodyR * (small ? 0.88f : 0.80f);
  drawSpunMetal(g, cx, cy, capR, faceBase, faceHi);
  g.PathClear();
  g.PathCircle(cx, cy, capR);
  g.PathStroke(IColor(90, 0, 0, 0), 1.f);

  // Указатель
  const float rad = (angle - 90.f) * 0.01745329f;
  const float c = std::cos(rad), s = std::sin(rad);
  const IColor pointer = (mStyle == KnobStyle::Pearl) ? col::textDark : (mStyle == KnobStyle::Black ? col::pearl : IColor(255, 30, 31, 36));
  g.PathClear();
  g.PathMoveTo(cx + c * capR * 0.30f, cy + s * capR * 0.30f);
  g.PathLineTo(cx + c * capR * 0.92f, cy + s * capR * 0.92f);
  g.PathStroke(pointer, small ? 1.6f : 2.2f, roundStroke());
  // Точка-индикатор на треке
  g.FillCircle(mAccent, cx + c * arcR, cy + s * arcR, arcW * 0.9f);

  // Подпись / значение
  if (mMouseDown || mHover)
  {
    const std::string val = displayWithUnit(GetParam());
    g.DrawText(txt(small ? 10.f : 11.f, col::text, font::monoMedium), val.c_str(), mValueRect);
  }
  else
    drawCapsLabel(g, mLabel.c_str(), mLabelRect, col::text2, small ? 9.f : 10.f);
}

void Knob::OnMouseDown(float x, float y, const IMouseMod& mod)
{
  if (mod.C || mod.R) // Cmd+клик / правый клик — точное значение
  {
    PromptUserInput(mValueRect.GetPadded(4.f, 0.f, 4.f, 2.f));
    return;
  }
  IKnobControlBase::OnMouseDown(x, y, mod);
}

void Knob::OnMouseDblClick(float, float, const IMouseMod&)
{
  SetValueToDefault();
  SetDirty(true);
}

void Knob::OnMouseOver(float x, float y, const IMouseMod& mod)
{
  mHover = true;
  IKnobControlBase::OnMouseOver(x, y, mod);
  SetDirty(false);
}

void Knob::OnMouseOut()
{
  mHover = false;
  IKnobControlBase::OnMouseOut();
  SetDirty(false);
}

// ============================================================================
// FootSwitch

FootSwitch::FootSwitch(const IRECT& bounds, int paramIdx, const char* label, const IColor& led)
: IControl(bounds, paramIdx), mLabel(label ? label : ""), mLed(led)
{
}

void FootSwitch::Draw(IGraphics& g)
{
  const bool on = GetValue() > 0.5;
  const float labelH = mLabel.empty() ? 0.f : 14.f;
  const IRECT area = mRECT.GetReducedFromBottom(labelH);
  const float R = std::min(area.W(), area.H()) * 0.5f;
  const float cx = area.MW(), cy = area.MH() + R * 0.08f;

  // LED над переключателем
  const float ledY = area.T + R * 0.18f;
  const float ledR = std::max(2.5f, R * 0.12f);
  if (on)
  {
    g.PathClear();
    g.PathCircle(cx, ledY, ledR * 3.2f);
    g.PathFill(IPattern::CreateRadialGradient(cx, ledY, ledR * 3.2f,
                                              { { IColor(110, mLed.R, mLed.G, mLed.B), 0.f }, { IColor(0, mLed.R, mLed.G, mLed.B), 1.f } }));
  }
  g.FillCircle(on ? mLed : IColor(255, 44, 34, 42), cx, ledY, ledR);
  g.PathClear();
  g.PathCircle(cx - ledR * 0.3f, ledY - ledR * 0.3f, ledR * 0.35f);
  g.PathFill(IColor(on ? 160 : 40, 255, 255, 255));

  // Шестигранная гайка + металлическая кнопка
  const float nutR = R * 0.62f;
  const float bcy = cy + R * 0.12f;
  g.PathClear();
  for (int i = 0; i < 6; ++i)
  {
    const float a = 0.5236f + i * 1.0472f;
    const float px = cx + std::cos(a) * nutR, py = bcy + std::sin(a) * nutR;
    if (i == 0) g.PathMoveTo(px, py); else g.PathLineTo(px, py);
  }
  g.PathClose();
  g.PathFill(IPattern::CreateLinearGradient(cx, bcy - nutR, cx, bcy + nutR, { { col::steel, 0.f }, { col::steelLo, 1.f } }));
  const float btnR = nutR * (on ? 0.66f : 0.70f);
  drawSpunMetal(g, cx, bcy + (on ? 1.f : 0.f), btnR, col::steel, col::steelHi);
  g.PathClear();
  g.PathCircle(cx, bcy + (on ? 1.f : 0.f), btnR);
  g.PathStroke(IColor(110, 0, 0, 0), 1.f);

  if (!mLabel.empty())
    drawCapsLabel(g, mLabel.c_str(), mRECT.GetFromBottom(labelH), on ? col::text : col::text3, 9.f);
}

void FootSwitch::OnMouseDown(float, float, const IMouseMod&)
{
  SetValue(GetValue() > 0.5 ? 0.0 : 1.0);
  SetDirty(true);
}

// ============================================================================
// ToggleChip

ToggleChip::ToggleChip(const IRECT& bounds, int paramIdx, const char* label, const IColor& on)
: IControl(bounds, paramIdx), mLabel(label ? label : ""), mOn(on)
{
}

void ToggleChip::Draw(IGraphics& g)
{
  const bool on = GetValue() > 0.5;
  const float r = mRECT.H() * 0.5f;
  g.PathClear();
  g.PathRoundRect(mRECT, r);
  g.PathFill(on ? IColor(60, mOn.R, mOn.G, mOn.B) : col::bg3);
  g.PathClear();
  g.PathRoundRect(mRECT.GetPadded(-0.5f), r);
  g.PathStroke(on ? IColor(180, mOn.R, mOn.G, mOn.B) : col::hairline, 1.f);
  const float ledR = r * 0.32f;
  g.FillCircle(on ? mOn : col::steelLo, mRECT.L + r, mRECT.MH(), ledR);
  drawCapsLabel(g, mLabel.c_str(), mRECT.GetReducedFromLeft(r * 1.6f).GetReducedFromRight(r * 0.6f), on ? col::text : col::text2,
                std::min(10.f, mRECT.H() * 0.42f), EAlign::Near);
}

void ToggleChip::OnMouseDown(float, float, const IMouseMod&)
{
  SetValue(GetValue() > 0.5 ? 0.0 : 1.0);
  SetDirty(true);
}

// ============================================================================
// EnumSelector

EnumSelector::EnumSelector(const IRECT& bounds, int paramIdx, const char* label)
: IControl(bounds, paramIdx), mLabel(label ? label : "")
{
}

void EnumSelector::Draw(IGraphics& g)
{
  const float labelH = mLabel.empty() ? 0.f : 14.f;
  mBody = mRECT.GetReducedFromBottom(labelH);
  const float h = mBody.H();
  mLeft = mBody.GetFromLeft(h);
  mRight = mBody.GetFromRight(h);
  g.PathClear();
  g.PathRoundRect(mBody, h * 0.5f);
  g.PathFill(col::bg3);
  g.PathClear();
  g.PathRoundRect(mBody.GetPadded(-0.5f), h * 0.5f);
  g.PathStroke(col::hairline, 1.f);
  // Стрелки
  auto arrow = [&](const IRECT& r, bool left) {
    const float cx = r.MW(), cy = r.MH(), s = h * 0.14f;
    g.PathClear();
    g.PathMoveTo(cx + (left ? s * 0.5f : -s * 0.5f), cy - s);
    g.PathLineTo(cx + (left ? -s * 0.5f : s * 0.5f), cy);
    g.PathLineTo(cx + (left ? s * 0.5f : -s * 0.5f), cy + s);
    g.PathStroke(col::text2, 1.4f, roundStroke());
  };
  arrow(mLeft, true);
  arrow(mRight, false);
  WDL_String s;
  if (GetParam()) GetParam()->GetDisplay(s);
  g.DrawText(txt(std::min(12.f, h * 0.48f), col::text, font::medium), s.Get(), mBody.GetReducedFromLeft(h).GetReducedFromRight(h));
  if (labelH > 0.f) drawCapsLabel(g, mLabel.c_str(), mRECT.GetFromBottom(labelH), col::text2, 9.f);
}

void EnumSelector::OnMouseDown(float x, float y, const IMouseMod& mod)
{
  const IParam* p = GetParam();
  if (!p) return;
  const int n = static_cast<int>(p->GetRange()) + 1;
  const int cur = p->Int() - static_cast<int>(p->GetMin());
  if (mLeft.Contains(x, y) || mRight.Contains(x, y))
  {
    const int next = (cur + (mLeft.Contains(x, y) ? n - 1 : 1)) % n;
    SetValue(p->ToNormalized(p->GetMin() + next));
    SetDirty(true);
    return;
  }
  mMenu.Clear();
  for (int i = 0; i < n; ++i)
  {
    const char* str = p->GetDisplayText(i);
    auto* item = mMenu.AddItem(str ? str : "?");
    if (i == cur) item->SetChecked(true);
  }
  GetUI()->CreatePopupMenu(*this, mMenu, mBody);
}

void EnumSelector::OnPopupMenuSelection(IPopupMenu* pMenu, int)
{
  if (!pMenu || !GetParam()) return;
  const int idx = pMenu->GetChosenItemIdx();
  if (idx < 0) return;
  SetValue(GetParam()->ToNormalized(GetParam()->GetMin() + idx));
  SetDirty(true);
}

// ============================================================================
// LevelMeterView

LevelMeterView::LevelMeterView(const IRECT& bounds, tsl::LevelMeter* meter, const char* label)
: IControl(bounds), mMeter(meter), mLabel(label ? label : "")
{
}

bool LevelMeterView::IsDirty()
{
  if (!mMeter) return false;
  const float p = mMeter->peak(), r = mMeter->rms();
  if (std::fabs(p - mShownPeak) > 1e-4f || std::fabs(r - mShownRms) > 1e-4f || mHoldFrames > 0) return true;
  return IControl::IsDirty();
}

void LevelMeterView::Draw(IGraphics& g)
{
  const float labelH = mLabel.empty() ? 0.f : 14.f;
  const IRECT bar = mRECT.GetReducedFromBottom(labelH).GetReducedFromTop(8.f);
  mShownPeak = mMeter ? mMeter->peak() : 0.f;
  mShownRms = mMeter ? mMeter->rms() : 0.f;
  const bool clip = mMeter && mMeter->clipped();

  auto dbToFrac = [](float lin) {
    const float db = 20.f * std::log10(std::max(lin, 1e-6f));
    return std::clamp((db + 60.f) / 66.f, 0.f, 1.f); // −60…+6 дБ
  };
  // Удержание пика
  const float pf = dbToFrac(mShownPeak);
  if (pf >= mHold) { mHold = pf; mHoldFrames = 60; }
  else if (mHoldFrames > 0) --mHoldFrames;
  else mHold = std::max(pf, mHold - 0.01f);

  g.PathClear();
  g.PathRoundRect(bar, 3.f);
  g.PathFill(IColor(255, 12, 13, 16));
  const IRECT inner = bar.GetPadded(-2.f);
  const float zero = dbToFrac(1.f);
  // RMS-заливка
  const float rf = dbToFrac(mShownRms);
  if (rf > 0.f)
  {
    const IRECT rr(inner.L, inner.B - inner.H() * rf, inner.R, inner.B);
    g.PathClear();
    g.PathRoundRect(rr, 1.5f);
    g.PathFill(IPattern::CreateLinearGradient(inner.L, inner.B, inner.L, inner.T,
                                              { { IColor(255, 120, 126, 136), 0.f }, { col::steelHi, zero - 0.12f }, { col::warn, zero - 0.03f }, { col::hot, zero + 0.02f } }));
  }
  // Пик — тонкая линия
  if (pf > 0.f)
  {
    const float y = inner.B - inner.H() * pf;
    g.FillRect(pf >= zero ? col::hot : col::pearl, IRECT(inner.L, y - 1.f, inner.R, y + 0.5f));
  }
  if (mHold > 0.f)
  {
    const float y = inner.B - inner.H() * mHold;
    g.FillRect(IColor(150, 236, 231, 228), IRECT(inner.L, y - 0.5f, inner.R, y + 0.5f));
  }
  // Риски: 0, −6, −12, −24, −48 дБ
  for (float db : { 0.f, -6.f, -12.f, -24.f, -48.f })
  {
    const float f = (db + 60.f) / 66.f;
    const float y = inner.B - inner.H() * f;
    g.FillRect(IColor(db == 0.f ? 120 : 50, 255, 255, 255), IRECT(bar.R + 1.f, y - 0.5f, bar.R + 4.f, y + 0.5f));
  }
  // Индикатор клипа
  const IRECT clipR(bar.L, mRECT.T, bar.R, mRECT.T + 5.f);
  g.PathClear();
  g.PathRoundRect(clipR, 2.f);
  g.PathFill(clip ? col::hot : IColor(255, 40, 30, 34));
  if (labelH > 0.f) drawCapsLabel(g, mLabel.c_str(), mRECT.GetFromBottom(labelH), col::text2, 9.f);
}

void LevelMeterView::OnMouseDown(float, float, const IMouseMod&)
{
  if (mMeter) mMeter->clearClip();
  mHold = 0.f;
  SetDirty(false);
}

// ============================================================================
// ReductionMeter

ReductionMeter::ReductionMeter(const IRECT& bounds, std::function<float()> source, const char* label, float rangeDb)
: IControl(bounds), mSource(std::move(source)), mLabel(label ? label : ""), mRange(rangeDb)
{
  mIgnoreMouse = true;
}

bool ReductionMeter::IsDirty()
{
  const float v = mSource ? mSource() : 0.f;
  return std::fabs(v - mShown) > 0.05f || IControl::IsDirty();
}

void ReductionMeter::Draw(IGraphics& g)
{
  mShown = mSource ? mSource() : 0.f;
  const IRECT bar = mRECT.GetFromBottom(6.f);
  g.PathClear();
  g.PathRoundRect(bar, 3.f);
  g.PathFill(IColor(255, 12, 13, 16));
  const float frac = std::clamp(-mShown / mRange, 0.f, 1.f);
  if (frac > 0.f)
  {
    const IRECT f(bar.R - bar.W() * frac, bar.T, bar.R, bar.B);
    g.PathClear();
    g.PathRoundRect(f, 3.f);
    g.PathFill(col::accent);
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%.1f dB", mShown);
  const IRECT textR = mRECT.GetReducedFromBottom(8.f);
  drawCapsLabel(g, mLabel.c_str(), textR, col::text3, 9.f, EAlign::Near);
  g.DrawText(txt(10.f, col::text2, font::mono, EAlign::Far), buf, textR);
}

// ============================================================================
// TabBar

TabBar::TabBar(const IRECT& bounds, std::vector<std::string> tabs, std::function<void(int)> onSelect)
: IControl(bounds), mTabs(std::move(tabs)), mOnSelect(std::move(onSelect))
{
}

int TabBar::hitTest(float x) const
{
  if (mTabs.empty()) return -1;
  const IRECT inner = mRECT.GetPadded(-4.f);
  const float w = inner.W() / static_cast<float>(mTabs.size());
  const int i = static_cast<int>((x - inner.L) / w);
  return (i >= 0 && i < static_cast<int>(mTabs.size())) ? i : -1;
}

void TabBar::Draw(IGraphics& g)
{
  drawGlass(g, mRECT, mRECT.H() * 0.5f, 1.f);
  const IRECT inner = mRECT.GetPadded(-4.f);
  const float w = inner.W() / static_cast<float>(mTabs.size());
  // Плавное перемещение выделения
  const float target = static_cast<float>(mSelected);
  mAnimPos += (target - mAnimPos) * 0.35f;
  if (std::fabs(target - mAnimPos) < 0.01f) mAnimPos = target;
  else SetDirty(false);
  const IRECT sel(inner.L + w * mAnimPos, inner.T, inner.L + w * (mAnimPos + 1.f), inner.B);
  g.PathClear();
  g.PathRoundRect(sel, sel.H() * 0.5f);
  g.PathFill(IPattern::CreateLinearGradient(sel.L, sel.T, sel.L, sel.B, { { IColor(70, 255, 255, 255), 0.f }, { IColor(26, 255, 255, 255), 1.f } }));
  g.PathClear();
  g.PathRoundRect(sel.GetPadded(-0.5f), sel.H() * 0.5f);
  g.PathStroke(IColor(60, 255, 255, 255), 1.f);
  // Акцентная точка под активной вкладкой
  g.FillCircle(col::accent, sel.MW(), sel.B - 4.f, 1.6f);

  for (size_t i = 0; i < mTabs.size(); ++i)
  {
    const IRECT cell(inner.L + w * i, inner.T, inner.L + w * (i + 1), inner.B - 2.f);
    const bool active = static_cast<int>(i) == mSelected;
    const IColor c = active ? col::text : (static_cast<int>(i) == mHover ? IColor(255, 200, 204, 210) : col::text2);
    g.DrawText(txt(std::min(13.f, mRECT.H() * 0.36f), c, active ? font::semibold : font::medium), mTabs[i].c_str(), cell);
  }
}

void TabBar::OnMouseDown(float x, float, const IMouseMod&)
{
  const int i = hitTest(x);
  if (i >= 0 && i != mSelected)
  {
    mSelected = i;
    if (mOnSelect) mOnSelect(i);
    SetDirty(false);
  }
}

void TabBar::OnMouseOver(float x, float, const IMouseMod&)
{
  const int h = hitTest(x);
  if (h != mHover) { mHover = h; SetDirty(false); }
}

void TabBar::OnMouseOut()
{
  mHover = -1;
  SetDirty(false);
}

void TabBar::SetSelected(int i)
{
  mSelected = std::clamp(i, 0, static_cast<int>(mTabs.size()) - 1);
  SetDirty(false);
}

// ============================================================================
// Label / ActionButton

Label::Label(const IRECT& bounds, const char* str, const IText& text, bool caps)
: IControl(bounds), mStr(str ? str : ""), mText(text), mCaps(caps)
{
  mIgnoreMouse = true;
}

void Label::Draw(IGraphics& g)
{
  if (mCaps) drawCapsLabel(g, mStr.c_str(), mRECT, mText.mFGColor, mText.mSize, mText.mAlign);
  else g.DrawText(mText, mStr.c_str(), mRECT);
}

ActionButton::ActionButton(const IRECT& bounds, const char* label, std::function<void()> action, bool glass)
: IControl(bounds), mLabel(label ? label : ""), mAction(std::move(action)), mGlass(glass)
{
}

void ActionButton::Draw(IGraphics& g)
{
  const float r = mRECT.H() * 0.5f;
  if (mGlass) drawGlass(g, mRECT, r, mHover ? 1.3f : 1.f);
  else
  {
    g.PathClear();
    g.PathRoundRect(mRECT, r);
    g.PathFill(mHover ? col::bg3 : col::bg2);
    g.PathClear();
    g.PathRoundRect(mRECT.GetPadded(-0.5f), r);
    g.PathStroke(col::hairline, 1.f);
  }
  if (mHighlight)
  {
    g.PathClear();
    g.PathRoundRect(mRECT.GetPadded(-0.5f), r);
    g.PathStroke(col::accent, 1.2f);
  }
  g.DrawText(txt(std::min(12.f, mRECT.H() * 0.42f), mHighlight ? col::text : col::text2, font::medium), mLabel.c_str(), mRECT);
}

void ActionButton::OnMouseDown(float, float, const IMouseMod&)
{
  if (mAction) mAction();
  SetDirty(false);
}

void ActionButton::OnMouseOver(float, float, const IMouseMod&)
{
  mHover = true;
  SetDirty(false);
}

void ActionButton::OnMouseOut()
{
  mHover = false;
  SetDirty(false);
}

} // namespace tslui
