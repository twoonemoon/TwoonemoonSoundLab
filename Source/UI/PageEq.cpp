// SPDX-License-Identifier: GPL-3.0-or-later
// Страница «EQ»: интерактивная АЧХ с перетаскиваемыми узлами, спектр pre/post, спектрограмма.
// Анализ выполняется в UI thread по данным lock-free колец модуля EQ; FFT-план создаётся заранее.
#include "UIContext.h"

#include "TwoonemoonSoundLab.h"
#include "DSP/FFT.h"
#include "Effects/Eq.h"

#include <cmath>

namespace tslui {

using namespace tsl;

namespace {

constexpr float kFMin = 20.f, kFMax = 20000.f, kDbRange = 18.f;
constexpr int kFftSize = 4096;

IColor bandColor(int b)
{
  static const IColor c[5] = { IColor(255, 120, 180, 240), IColor(255, 120, 214, 176), IColor(255, 227, 168, 87), IColor(255, 214, 110, 186), IColor(255, 150, 120, 240) };
  return c[b % 5];
}

class EqView : public IControl
{
public:
  EqView(const IRECT& r, UIContext* ctx) : IControl(r), mCtx(ctx)
  {
    mFft.prepare(kFftSize);
    mBuf.assign(kFftSize, 0.f);
    mPre.assign(kFftSize, 0.f);
    mPost.assign(kFftSize, 0.f);
    mWin.resize(kFftSize);
    for (int i = 0; i < kFftSize; ++i) mWin[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(kTwoPi * i / (kFftSize - 1));
    mSpecPre.assign(kPoints, -120.f);
    mSpecPost.assign(kPoints, -120.f);
  }

  bool IsDirty() override
  {
    pull();
    return true;
  }

  void Draw(IGraphics& g) override
  {
    const IRECT plot = plotRect();
    g.PathClear();
    g.PathRoundRect(mRECT, 12.f);
    g.PathFill(IColor(255, 10, 11, 14));
    // Сетка
    for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
    {
      const float x = xForF(f, plot);
      g.DrawLine(IColor(f == 100.f || f == 1000.f || f == 10000.f ? 34 : 16, 255, 255, 255), x, plot.T, x, plot.B, nullptr, 1.f);
      char buf[16];
      snprintf(buf, sizeof(buf), f >= 1000.f ? "%.0fk" : "%.0f", f >= 1000.f ? f / 1000.f : f);
      g.DrawText(txt(9.f, col::text3, font::mono), buf, IRECT(x - 20.f, plot.B + 4.f, x + 20.f, plot.B + 16.f));
    }
    for (float d : { -12.f, -6.f, 0.f, 6.f, 12.f })
    {
      const float y = yForDb(d, plot);
      g.DrawLine(IColor(d == 0.f ? 40 : 14, 255, 255, 255), plot.L, y, plot.R, y, nullptr, 1.f);
      char buf[8];
      snprintf(buf, sizeof(buf), "%+.0f", d);
      g.DrawText(txt(9.f, col::text3, font::mono, EAlign::Far), buf, IRECT(plot.L - 34.f, y - 6.f, plot.L - 6.f, y + 6.f));
    }
    // Спектры (−90…0 дБFS отображаются на высоту графика)
    auto drawSpectrum = [&](const std::vector<float>& s, const IColor& fill, const IColor& line) {
      g.PathClear();
      g.PathMoveTo(plot.L, plot.B);
      for (int i = 0; i < kPoints; ++i)
      {
        const float x = plot.L + plot.W() * i / (kPoints - 1);
        const float y = plot.B - plot.H() * std::clamp((s[static_cast<size_t>(i)] + 90.f) / 90.f, 0.f, 1.f);
        g.PathLineTo(x, y);
      }
      g.PathLineTo(plot.R, plot.B);
      g.PathClose();
      g.PathFill(fill);
      g.PathClear();
      for (int i = 0; i < kPoints; ++i)
      {
        const float x = plot.L + plot.W() * i / (kPoints - 1);
        const float y = plot.B - plot.H() * std::clamp((s[static_cast<size_t>(i)] + 90.f) / 90.f, 0.f, 1.f);
        if (i == 0) g.PathMoveTo(x, y); else g.PathLineTo(x, y);
      }
      g.PathStroke(line, 1.f);
    };
    drawSpectrum(mSpecPre, IColor(14, 255, 255, 255), IColor(40, 255, 255, 255));
    drawSpectrum(mSpecPost, IColor(30, 196, 92, 166), IColor(110, 214, 110, 186));

    // Кривая EQ
    auto param = [this](int id) { return static_cast<float>(mCtx->plugin->GetParam(id)->Value()); };
    const double sr = mCtx->plugin->engine().isPrepared() ? mCtx->plugin->engine().sampleRate() : 48000.0;
    const bool on = param(kEqOn) >= 0.5f;
    g.PathClear();
    for (int i = 0; i < 240; ++i)
    {
      const float f = kFMin * std::pow(kFMax / kFMin, i / 239.f);
      const float db = static_cast<float>(Eq::responseDb(param, f, sr));
      const float x = xForF(f, plot), y = yForDb(db, plot);
      if (i == 0) g.PathMoveTo(x, y); else g.PathLineTo(x, y);
    }
    IStrokeOptions o;
    o.mJoinOption = ELineJoin::Round;
    g.PathStroke(on ? col::text : col::text3, 2.f, o);

    // Узлы полос
    for (int b = 0; b < Eq::kBands; ++b)
    {
      const int base = kEqB1On + b * 5;
      const bool bon = param(base) >= 0.5f;
      const float f = param(base + 2), gdb = param(base + 3);
      const int type = static_cast<int>(param(base + 1));
      const float x = xForF(f, plot), y = type == 3 ? yForDb(0.f, plot) : yForDb(gdb, plot);
      const IColor c = bandColor(b);
      if (b == mDrag || b == mHover)
      {
        g.PathClear();
        g.PathCircle(x, y, 16.f);
        g.PathFill(IColor(50, c.R, c.G, c.B));
      }
      g.FillCircle(bon ? c : col::bg3, x, y, 8.f);
      g.PathClear();
      g.PathCircle(x, y, 8.f);
      g.PathStroke(IColor(200, 255, 255, 255), 1.2f);
      char lbl[4];
      snprintf(lbl, sizeof(lbl), "%d", b + 1);
      g.DrawText(txt(10.f, IColor(255, 20, 20, 24), font::bold), lbl, IRECT(x - 8.f, y - 8.f, x + 8.f, y + 8.f));
      if (b == mDrag || b == mHover)
      {
        char info[64];
        snprintf(info, sizeof(info), "%s  %.0f Hz  %+.1f dB  Q %.2f", mCtx->plugin->GetParam(base + 1)->GetDisplayText(type), f, gdb, param(base + 4));
        g.DrawText(txt(11.f, col::text, font::mono), info, IRECT(plot.L, plot.T + 4.f, plot.R, plot.T + 20.f));
      }
    }
    // Ручки срезов
    for (int k = 0; k < 2; ++k)
    {
      const int pid = k == 0 ? kEqLowCut : kEqHighCut;
      const float f = param(pid);
      const float x = xForF(f, plot);
      const bool active = k == 0 ? f > 20.5f : f < 19900.f;
      const bool hl = mDrag == 10 + k || mHover == 10 + k;
      g.DrawLine(active ? IColor(hl ? 200 : 110, 255, 255, 255) : IColor(40, 255, 255, 255), x, plot.T + 24.f, x, plot.B, nullptr, hl ? 2.f : 1.f);
      g.PathClear();
      g.PathRoundRect(IRECT(x - 14.f, plot.B - 18.f, x + 14.f, plot.B - 4.f), 4.f);
      g.PathFill(active ? col::bg3 : col::bg2);
      g.DrawText(txt(9.f, col::text2, font::semibold), k == 0 ? "HP" : "LP", IRECT(x - 14.f, plot.B - 18.f, x + 14.f, plot.B - 4.f));
    }
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    mDrag = hit(x, y);
    if (mDrag < 0) return;
    if (mod.R && mDrag < 10)
    {
      // Правый клик: меню типа полосы
      mMenu.Clear();
      const int base = kEqB1On + mDrag * 5;
      const IParam* p = mCtx->plugin->GetParam(base + 1);
      for (int i = 0; i < 4; ++i)
      {
        auto* it = mMenu.AddItem(p->GetDisplayText(i));
        if (p->Int() == i) it->SetChecked(true);
      }
      mMenu.AddSeparator();
      mMenu.AddItem(mCtx->plugin->GetParam(base)->Bool() ? "Выключить полосу" : "Включить полосу");
      mMenuBand = mDrag;
      mDrag = -1;
      GetUI()->CreatePopupMenu(*this, mMenu, x, y);
      return;
    }
    for (int pid : paramsFor(mDrag)) mCtx->beginEdit(pid);
  }

  void OnMouseDrag(float x, float y, float, float, const IMouseMod& mod) override
  {
    if (mDrag < 0) return;
    const IRECT plot = plotRect();
    const float f = fForX(std::clamp(x, plot.L, plot.R), plot);
    if (mDrag >= 10)
    {
      const int pid = mDrag == 10 ? kEqLowCut : kEqHighCut;
      const IParam* p = mCtx->plugin->GetParam(pid);
      mCtx->setParam(pid, std::clamp(static_cast<double>(f), p->GetMin(), p->GetMax()));
      return;
    }
    const int base = kEqB1On + mDrag * 5;
    mCtx->setParam(base + 2, std::clamp(f, kFMin, kFMax));
    if (!mod.S) mCtx->setParam(base + 3, std::clamp(dbForY(y, plot), -kDbRange, kDbRange));
  }

  void OnMouseUp(float, float, const IMouseMod&) override
  {
    if (mDrag >= 0)
      for (int pid : paramsFor(mDrag)) mCtx->endEdit(pid);
    mDrag = -1;
  }

  void OnMouseDblClick(float x, float y, const IMouseMod&) override
  {
    const int h = hit(x, y);
    if (h < 0) return;
    if (h >= 10) mCtx->setParam(h == 10 ? kEqLowCut : kEqHighCut, h == 10 ? 20.0 : 20000.0);
    else mCtx->setParam(kEqB1On + h * 5 + 3, 0.0);
  }

  void OnMouseWheel(float x, float y, const IMouseMod&, float d) override
  {
    const int h = hit(x, y, 40.f);
    if (h < 0 || h >= 10) return;
    const int qp = kEqB1On + h * 5 + 4;
    const double q = mCtx->plugin->GetParam(qp)->Value();
    mCtx->setParam(qp, std::clamp(q * std::pow(1.1, d), 0.1, 10.0));
  }

  void OnMouseOver(float x, float y, const IMouseMod&) override { mHover = hit(x, y); }
  void OnMouseOut() override { mHover = -1; }

  void OnPopupMenuSelection(IPopupMenu* pMenu, int) override
  {
    if (!pMenu || mMenuBand < 0) return;
    const int idx = pMenu->GetChosenItemIdx();
    const int base = kEqB1On + mMenuBand * 5;
    if (idx >= 0 && idx < 4) mCtx->setParam(base + 1, idx);
    else if (idx == 5) mCtx->setParam(base, mCtx->plugin->GetParam(base)->Bool() ? 0.0 : 1.0);
    mMenuBand = -1;
  }

private:
  static constexpr int kPoints = 256;

  IRECT plotRect() const { return mRECT.GetPadded(-44.f, -12.f, -14.f, -24.f); }
  static float xForF(float f, const IRECT& p) { return p.L + p.W() * std::log(f / kFMin) / std::log(kFMax / kFMin); }
  static float fForX(float x, const IRECT& p) { return kFMin * std::pow(kFMax / kFMin, (x - p.L) / p.W()); }
  static float yForDb(float db, const IRECT& p) { return p.MH() - p.H() * 0.5f * std::clamp(db, -kDbRange - 6.f, kDbRange + 6.f) / (kDbRange + 6.f); }
  static float dbForY(float y, const IRECT& p) { return -(y - p.MH()) / (p.H() * 0.5f) * (kDbRange + 6.f); }

  std::vector<int> paramsFor(int h) const
  {
    if (h >= 10) return { h == 10 ? kEqLowCut : kEqHighCut };
    const int base = kEqB1On + h * 5;
    return { base + 2, base + 3 };
  }

  int hit(float x, float y, float radius = 14.f) const
  {
    const IRECT plot = plotRect();
    auto param = [this](int id) { return static_cast<float>(mCtx->plugin->GetParam(id)->Value()); };
    int best = -1;
    float bestD = radius;
    for (int b = 0; b < Eq::kBands; ++b)
    {
      const int base = kEqB1On + b * 5;
      const float bx = xForF(param(base + 2), plot);
      const float by = static_cast<int>(param(base + 1)) == 3 ? yForDb(0.f, plot) : yForDb(param(base + 3), plot);
      const float d = std::hypot(bx - x, by - y);
      if (d < bestD) { bestD = d; best = b; }
    }
    if (best >= 0) return best;
    for (int k = 0; k < 2; ++k)
    {
      const float hx = xForF(param(k == 0 ? kEqLowCut : kEqHighCut), plot);
      if (std::fabs(hx - x) < 14.f && y > plot.B - 24.f) return 10 + k;
    }
    return -1;
  }

  void pull()
  {
    Engine& e = mCtx->plugin->engine();
    auto* eq = static_cast<Eq*>(e.module(ModuleId::Eq));
    if (!eq || !e.isPrepared()) return;
    auto update = [&](SpscRing<float>& ring, std::vector<float>& hist, std::vector<float>& spec) {
      ring.discardOldest(kFftSize);
      std::vector<float> tmp(kFftSize);
      const size_t got = ring.pop(tmp.data(), tmp.size());
      if (got == 0) return;
      // сдвиг истории
      std::move(hist.begin() + static_cast<long>(got), hist.end(), hist.begin());
      std::copy(tmp.begin(), tmp.begin() + static_cast<long>(got), hist.end() - static_cast<long>(got));
      for (int i = 0; i < kFftSize; ++i) mBuf[static_cast<size_t>(i)] = hist[static_cast<size_t>(i)] * mWin[static_cast<size_t>(i)];
      std::vector<float> re(kFftSize / 2), im(kFftSize / 2);
      mFft.forward(mBuf.data(), re.data(), im.data());
      const double sr = e.sampleRate();
      for (int i = 0; i < kPoints; ++i)
      {
        const double f0 = kFMin * std::pow(kFMax / kFMin, (i - 0.5) / (kPoints - 1));
        const double f1 = kFMin * std::pow(kFMax / kFMin, (i + 0.5) / (kPoints - 1));
        int k0 = std::max(1, static_cast<int>(f0 / sr * kFftSize)), k1 = std::min(kFftSize / 2 - 1, static_cast<int>(f1 / sr * kFftSize));
        if (k1 < k0) k1 = k0;
        double mx = 0.0;
        for (int k = k0; k <= k1; ++k) mx = std::max(mx, static_cast<double>(re[static_cast<size_t>(k)]) * re[static_cast<size_t>(k)] + static_cast<double>(im[static_cast<size_t>(k)]) * im[static_cast<size_t>(k)]);
        const float db = static_cast<float>(10.0 * std::log10(mx + 1e-20) + 20.0 * std::log10(4.0 / kFftSize));
        float& s = spec[static_cast<size_t>(i)];
        s = db > s ? db : s + (db - s) * 0.25f; // быстрый подъём, плавный спад
      }
    };
    update(eq->preRing(), mPre, mSpecPre);
    update(eq->postRing(), mPost, mSpecPost);
  }

  UIContext* mCtx;
  RealFFT mFft;
  std::vector<float> mBuf, mPre, mPost, mWin, mSpecPre, mSpecPost;
  int mDrag = -1, mHover = -1, mMenuBand = -1;
  IPopupMenu mMenu;
};

} // namespace

void buildEqPage(UIContext& ctx, const char* page)
{
  const IRECT content = ctx.content;
  const IRECT top = content.GetFromTop(40.f);
  ctx.add(new ToggleChip(IRECT(top.L, top.T + 6.f, top.L + 100.f, top.T + 32.f), kEqOn, "EQ On"), page);
  ctx.add(new Label(IRECT(top.L + 120.f, top.T, top.R, top.B),
                    "Перетаскивайте узлы: по горизонтали — частота, по вертикали — уровень; колесо — Q; двойной клик — сброс; правый клик — тип полосы.",
                    txt(11.f, col::text3, font::regular, EAlign::Near)),
          page);
  const IRECT view = IRECT(content.L, top.B + 4.f, content.R, top.B + 330.f);
  ctx.add(new EqView(view, &ctx), page);

  // Полосы
  const IRECT bands = IRECT(content.L, view.B + 12.f, content.R, content.B);
  const float bw = (bands.W() - 180.f) / 5.f;
  for (int b = 0; b < Eq::kBands; ++b)
  {
    const IRECT r(bands.L + b * bw, bands.T, bands.L + (b + 1) * bw - 8.f, bands.B);
    const int base = kEqB1On + b * 5;
    const IColor c = bandColor(b);
    ctx.add(new Canvas(r, [b, c](IGraphics& g, const IRECT& rr) {
              drawHardwarePanel(g, rr, 10.f);
              g.FillRect(c, IRECT(rr.L + 10.f, rr.T, rr.R - 10.f, rr.T + 3.f));
              char buf[16];
              snprintf(buf, sizeof(buf), "Полоса %d", b + 1);
              g.DrawText(txt(12.f, col::text, font::semibold, EAlign::Near), buf, IRECT(rr.L + 12.f, rr.T + 8.f, rr.R, rr.T + 26.f));
            }),
            page);
    ctx.add(new ToggleChip(IRECT(r.R - 64.f, r.T + 8.f, r.R - 10.f, r.T + 28.f), base, "On", c), page);
    ctx.add(new EnumSelector(IRECT(r.L + 8.f, r.T + 34.f, r.R - 8.f, r.T + 60.f), base + 1), page);
    const float kw = (r.W() - 8.f) / 3.f;
    const char* names[3] = { "Freq", "Gain", "Q" };
    for (int k = 0; k < 3; ++k)
      ctx.add(new Knob(IRECT(r.L + 4.f + k * kw, r.T + 66.f, r.L + 4.f + (k + 1) * kw, r.B - 6.f), base + 2 + k, names[k], KnobStyle::Small, c, k == 1), page);
  }
  const IRECT cuts(bands.R - 172.f, bands.T, bands.R, bands.B);
  ctx.add(new Canvas(cuts, [](IGraphics& g, const IRECT& rr) {
            drawHardwarePanel(g, rr, 10.f);
            g.DrawText(txt(12.f, col::text, font::semibold, EAlign::Near), "Срезы и уровень", IRECT(rr.L + 12.f, rr.T + 8.f, rr.R, rr.T + 26.f));
          }),
          page);
  ctx.add(new EnumSelector(IRECT(cuts.L + 8.f, cuts.T + 34.f, cuts.MW() - 2.f, cuts.T + 58.f), kEqLowSlope), page);
  ctx.add(new EnumSelector(IRECT(cuts.MW() + 2.f, cuts.T + 34.f, cuts.R - 8.f, cuts.T + 58.f), kEqHighSlope), page);
  const float kw = (cuts.W() - 8.f) / 3.f;
  const int cutIds[3] = { kEqLowCut, kEqHighCut, kEqOutput };
  const char* cutNames[3] = { "Low Cut", "High Cut", "Output" };
  for (int k = 0; k < 3; ++k)
    ctx.add(new Knob(IRECT(cuts.L + 4.f + k * kw, cuts.T + 66.f, cuts.L + 4.f + (k + 1) * kw, cuts.B - 6.f), cutIds[k], cutNames[k], KnobStyle::Small, col::accent, k == 2), page);
}

} // namespace tslui
