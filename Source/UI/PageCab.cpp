// SPDX-License-Identifier: GPL-3.0-or-later
// Страница «Кабинет» (Cab Studio): два кабинета/микрофона, IR, положение микрофона.
#include "UIContext.h"

#include "TwoonemoonSoundLab.h"
#include "Cabinet/Cabinet.h"
#include "DSP/FFT.h"

#include <cmath>

namespace tslui {

using namespace tsl;

namespace {

/**
 * Диффузор динамика сверху: перетаскивание метки микрофона меняет Position (центр ↔ край).
 * Угол показывается наклоном капсюля, дистанция — размером и тенью.
 */
class ConeControl : public IControl
{
public:
  ConeControl(const IRECT& r, int posParam, int angleParam, int distParam, int speakerParam, TwoonemoonSoundLab* plugin)
  : IControl(r, posParam), mAngleParam(angleParam), mDistParam(distParam), mSpeakerParam(speakerParam), mPlugin(plugin)
  {
  }

  void Draw(IGraphics& g) override
  {
    const bool user = mPlugin->GetParam(mSpeakerParam)->Int() >= kCabUserIRIndex;
    const float cx = mRECT.MW(), cy = mRECT.MH();
    const float R = std::min(mRECT.W(), mRECT.H()) * 0.46f;
    // Рама и диффузор
    g.PathClear();
    g.PathCircle(cx, cy + 3.f, R + 6.f);
    g.PathFill(IColor(120, 0, 0, 0));
    g.PathClear();
    g.PathCircle(cx, cy, R + 5.f);
    g.PathFill(IPattern::CreateLinearGradient(cx, cy - R, cx, cy + R, { { col::steelMid, 0.f }, { col::steelLo, 1.f } }));
    g.PathClear();
    g.PathCircle(cx, cy, R);
    g.PathFill(IPattern::CreateRadialGradient(cx - R * 0.2f, cy - R * 0.3f, R * 1.3f, { { IColor(255, 52, 52, 58), 0.f }, { IColor(255, 18, 18, 21), 1.f } }));
    for (int i = 1; i < 7; ++i)
    {
      g.PathClear();
      g.PathCircle(cx, cy, R * (0.3f + 0.1f * i));
      g.PathStroke(IColor(22, 255, 255, 255), 0.8f);
    }
    // Пылезащитный колпачок
    g.PathClear();
    g.PathCircle(cx, cy, R * 0.26f);
    g.PathFill(IPattern::CreateRadialGradient(cx - R * 0.08f, cy - R * 0.1f, R * 0.3f, { { IColor(255, 96, 96, 104), 0.f }, { IColor(255, 30, 30, 34), 1.f } }));

    if (user)
    {
      g.DrawText(txt(11.f, col::text2, font::medium), "Пользовательский IR", mRECT.GetFromBottom(18.f));
      return;
    }
    // Микрофон
    const float pos = static_cast<float>(GetValue());
    const float ang = static_cast<float>(mPlugin->GetParam(mAngleParam)->Value() / 45.0);
    const float dist = static_cast<float>(mPlugin->GetParam(mDistParam)->GetNormalized());
    const float mx = cx + pos * R * 0.95f, my = cy;
    const float size = 9.f - 3.f * dist;
    g.PathClear();
    g.PathCircle(mx + 2.f + 6.f * dist, my + 3.f + 6.f * dist, size + 2.f);
    g.PathFill(IColor(static_cast<int>(120 - 70 * dist), 0, 0, 0));
    g.PathClear();
    g.PathEllipse(mx, my, size, size * (1.f - 0.45f * ang), 0.f);
    g.PathFill(IPattern::CreateLinearGradient(mx, my - size, mx, my + size, { { col::steelHi, 0.f }, { col::steelMid, 1.f } }));
    g.PathClear();
    g.PathEllipse(mx, my, size * 0.55f, size * 0.55f * (1.f - 0.45f * ang), 0.f);
    g.PathFill(col::accent);
    drawCapsLabel(g, "центр", IRECT(cx - 40.f, mRECT.B - 14.f, cx, mRECT.B), col::text3, 8.f, EAlign::Near);
    drawCapsLabel(g, "край", IRECT(cx + R - 30.f, mRECT.B - 14.f, cx + R + 10.f, mRECT.B), col::text3, 8.f, EAlign::Far);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override { setFromX(x); }
  void OnMouseDrag(float x, float y, float, float, const IMouseMod&) override { setFromX(x); }
  void OnMouseDblClick(float, float, const IMouseMod&) override
  {
    SetValueToDefault();
    SetDirty(true);
  }

private:
  void setFromX(float x)
  {
    const float R = std::min(mRECT.W(), mRECT.H()) * 0.46f;
    const float v = std::clamp((x - mRECT.MW()) / (R * 0.95f), 0.f, 1.f);
    SetValue(v);
    SetDirty(true);
  }
  int mAngleParam, mDistParam, mSpeakerParam;
  TwoonemoonSoundLab* mPlugin;
};

/** АЧХ текущего IR слота (лог-шкала частот) и форма волны. */
class IRView : public IControl
{
public:
  IRView(const IRECT& r, Cabinet* cab, int slot, Engine* engine) : IControl(r), mCab(cab), mSlot(slot), mEngine(engine) { mIgnoreMouse = true; }

  bool IsDirty() override
  {
    if (mCab && mCab->previewSerial() != mSerial) return true;
    return IControl::IsDirty();
  }

  void Draw(IGraphics& g) override
  {
    if (mCab && mCab->previewSerial() != mSerial)
    {
      mSerial = mCab->previewSerial();
      recompute();
    }
    g.PathClear();
    g.PathRoundRect(mRECT, 8.f);
    g.PathFill(IColor(255, 11, 12, 15));
    const IRECT plot = mRECT.GetPadded(-10.f, -8.f, -10.f, -18.f);
    // Сетка частот
    for (float f : { 100.f, 1000.f, 10000.f })
    {
      const float x = plot.L + plot.W() * std::log(f / 40.f) / std::log(20000.f / 40.f);
      g.DrawLine(IColor(30, 255, 255, 255), x, plot.T, x, plot.B, nullptr, 1.f);
      char buf[16];
      snprintf(buf, sizeof(buf), f >= 1000.f ? "%.0fk" : "%.0f", f >= 1000.f ? f / 1000.f : f);
      g.DrawText(txt(9.f, col::text3, font::mono), buf, IRECT(x - 20.f, plot.B + 2.f, x + 20.f, plot.B + 14.f));
    }
    for (float d : { -12.f, 0.f, 12.f })
    {
      const float y = plot.MH() - plot.H() * 0.5f * d / 24.f;
      g.DrawLine(IColor(d == 0.f ? 40 : 18, 255, 255, 255), plot.L, y, plot.R, y, nullptr, 1.f);
    }
    if (mResp.empty()) return;
    g.PathClear();
    for (size_t i = 0; i < mResp.size(); ++i)
    {
      const float x = plot.L + plot.W() * static_cast<float>(i) / (mResp.size() - 1);
      const float y = plot.MH() - plot.H() * 0.5f * std::clamp(mResp[i], -24.f, 24.f) / 24.f;
      if (i == 0) g.PathMoveTo(x, y); else g.PathLineTo(x, y);
    }
    IStrokeOptions o;
    o.mJoinOption = ELineJoin::Round;
    g.PathStroke(col::accent, 1.6f, o);
  }

private:
  void recompute()
  {
    mResp.clear();
    if (!mCab) return;
    const auto ir = mCab->irPreview(mSlot);
    if (ir.empty()) return;
    int N = 2048;
    while (N < static_cast<int>(ir.size())) N <<= 1;
    RealFFT fft;
    fft.prepare(N);
    std::vector<float> t(static_cast<size_t>(N), 0.f), re(static_cast<size_t>(N / 2)), im(static_cast<size_t>(N / 2));
    std::copy(ir.begin(), ir.end(), t.begin());
    fft.forward(t.data(), re.data(), im.data());
    const double sr = mEngine ? mEngine->sampleRate() : 48000.0;
    const int points = 160;
    std::vector<float> db(static_cast<size_t>(points));
    for (int i = 0; i < points; ++i)
    {
      const double f = 40.0 * std::pow(20000.0 / 40.0, i / double(points - 1));
      const int k = std::clamp(static_cast<int>(f / sr * N), 1, N / 2 - 1);
      const double m = std::sqrt(static_cast<double>(re[k]) * re[k] + static_cast<double>(im[k]) * im[k]);
      db[static_cast<size_t>(i)] = static_cast<float>(20.0 * std::log10(std::max(m, 1e-9)));
    }
    // Сглаживание 1/6 октавы и нормировка к среднему в 200–4000 Гц
    std::vector<float> sm(db.size());
    for (size_t i = 0; i < db.size(); ++i)
    {
      float a = 0.f;
      int c = 0;
      for (int j = -3; j <= 3; ++j)
      {
        const int idx = static_cast<int>(i) + j;
        if (idx >= 0 && idx < points) { a += db[static_cast<size_t>(idx)]; ++c; }
      }
      sm[i] = a / c;
    }
    float ref = 0.f;
    int rc = 0;
    for (int i = 0; i < points; ++i)
    {
      const double f = 40.0 * std::pow(20000.0 / 40.0, i / double(points - 1));
      if (f > 200 && f < 4000) { ref += sm[static_cast<size_t>(i)]; ++rc; }
    }
    ref /= std::max(rc, 1);
    for (auto& v : sm) v -= ref;
    mResp = std::move(sm);
  }

  Cabinet* mCab;
  int mSlot;
  Engine* mEngine;
  uint32_t mSerial = 0xFFFFFFFF;
  std::vector<float> mResp;
};

/** Строка статуса слота (имя файла/ошибка). */
class SlotStatus : public IControl
{
public:
  SlotStatus(const IRECT& r, Cabinet* cab, int slot) : IControl(r), mCab(cab), mSlot(slot) { mIgnoreMouse = true; }
  bool IsDirty() override { return (mCab && mCab->previewSerial() != mSerial) || IControl::IsDirty(); }
  void Draw(IGraphics& g) override
  {
    if (!mCab) return;
    mSerial = mCab->previewSerial();
    const auto info = mCab->slotInfo(mSlot);
    if (info.message.empty()) return;
    g.DrawText(txt(11.f, info.error ? col::hot : col::text2, font::regular, EAlign::Near), info.message.c_str(), mRECT);
  }

private:
  Cabinet* mCab;
  int mSlot;
  uint32_t mSerial = 0xFFFFFFFF;
};

} // namespace

void buildCabPage(UIContext& ctx, const char* page)
{
  const IRECT content = ctx.content;
  TwoonemoonSoundLab* plugin = ctx.plugin;
  auto* cab = static_cast<Cabinet*>(plugin->engine().module(ModuleId::Cabinet));

  const IRECT cols = content.GetReducedFromBottom(96.f);
  for (int slot = 0; slot < 2; ++slot)
  {
    const IRECT r = slot == 0 ? cols.GetFromLeft(cols.W() * 0.5f - 8.f) : cols.GetFromRight(cols.W() * 0.5f - 8.f);
    const int b = slot == 0 ? kCabAOn : kCabBOn;
    ctx.section(r, slot == 0 ? "Кабинет A" : "Кабинет B", slot == 0 ? "Основной микрофон" : "Второй микрофон / кабинет", page);
    ctx.add(new ToggleChip(IRECT(r.R - 104.f, r.T + 14.f, r.R - 18.f, r.T + 38.f), b, "On"), page);

    const IRECT body = r.GetPadded(-16.f, -56.f, -16.f, -14.f);
    const IRECT left = body.GetFromLeft(210.f);
    const IRECT right = body.GetReducedFromLeft(222.f);
    ctx.add(new ConeControl(left.GetFromTop(210.f), b + 3, b + 5, b + 4, b + 1, plugin), page);

    // Селекторы
    ctx.add(new EnumSelector(IRECT(right.L, right.T, right.R, right.T + 44.f), b + 1, "Speaker"), page);
    ctx.add(new EnumSelector(IRECT(right.L, right.T + 52.f, right.R, right.T + 96.f), b + 2, "Mic"), page);
    // Микрофонные регуляторы
    const IRECT kr1(right.L, right.T + 104.f, right.R, right.T + 190.f);
    const float kw = kr1.W() / 3.f;
    const int micIds[] = { b + 3, b + 4, b + 5 };
    const char* micNames[] = { "Position", "Distance", "Angle" };
    for (int i = 0; i < 3; ++i) ctx.add(new Knob(IRECT(kr1.L + kw * i, kr1.T, kr1.L + kw * (i + 1), kr1.B), micIds[i], micNames[i], KnobStyle::Small), page);
    // Микс
    const IRECT kr2(left.L, left.T + 222.f, left.R + 10.f, left.T + 300.f);
    const float kw2 = kr2.W() / 3.f;
    const int mixIds[] = { b + 6, b + 7, b + 9 };
    const char* mixNames[] = { "Level", "Pan", "Delay" };
    for (int i = 0; i < 3; ++i)
      ctx.add(new Knob(IRECT(kr2.L + kw2 * i, kr2.T, kr2.L + kw2 * (i + 1), kr2.B), mixIds[i], mixNames[i], KnobStyle::Small, col::accent, i == 1), page);
    ctx.add(new ToggleChip(IRECT(left.L, kr2.B + 8.f, left.L + 110.f, kr2.B + 32.f), b + 8, "Phase Inv"), page);

    // АЧХ и загрузка IR
    const IRECT irArea(right.L, right.T + 200.f, right.R, body.B - 40.f);
    ctx.add(new IRView(irArea, cab, slot, &plugin->engine()), page);
    const int speakerParam = b + 1;
    ctx.add(new ActionButton(IRECT(left.L + 120.f, kr2.B + 8.f, left.L + 220.f, kr2.B + 32.f), "Загрузить IR…", [&ctx, plugin, slot, speakerParam]() {
              WDL_String file, dir;
              ctx.g->PromptForFile(file, dir, EFileAction::Open, "wav", [plugin, slot, speakerParam, &ctx](const WDL_String& f, const WDL_String&) {
                if (f.GetLength() == 0) return;
                plugin->loadUserIR(slot, f.Get());
                if (IControl* c = ctx.g->GetControlWithParamIdx(speakerParam))
                {
                  c->SetValue(plugin->GetParam(speakerParam)->ToNormalized(kCabUserIRIndex));
                  c->SetDirty(true);
                }
              });
            }, false),
            page);
    ctx.add(new SlotStatus(IRECT(right.L, body.B - 34.f, right.R, body.B - 14.f), cab, slot), page);
  }

  // Общие регуляторы кабинета
  const IRECT bottom = content.GetFromBottom(84.f);
  ctx.add(new Canvas(bottom, [](IGraphics& g, const IRECT& r) {
            drawGlass(g, r, 14.f, 0.9f);
            drawCapsLabel(g, "Cab Studio", r.GetPadded(-18.f, -10.f, 0.f, 0.f).GetFromTop(14.f), col::text3, 9.f, EAlign::Near);
            g.DrawText(txt(11.f, col::text3, font::regular, EAlign::Near),
                       "Встроенные кабинеты и микрофоны — собственные алгоритмические модели (не измерения реальных устройств).",
                       IRECT(r.L + 420.f, r.MH() - 8.f, r.R - 20.f, r.MH() + 8.f));
          }),
          page);
  const IRECT br = bottom.GetPadded(-18.f, -18.f, -18.f, -4.f);
  ctx.add(new ToggleChip(IRECT(br.L, br.MH() - 12.f, br.L + 110.f, br.MH() + 12.f), kCabOn, "Cab On"), page);
  ctx.add(new Knob(IRECT(br.L + 130.f, br.T, br.L + 210.f, br.B), kCabLowCut, "Low Cut", KnobStyle::Small), page);
  ctx.add(new Knob(IRECT(br.L + 220.f, br.T, br.L + 300.f, br.B), kCabHighCut, "High Cut", KnobStyle::Small), page);
}

} // namespace tslui
