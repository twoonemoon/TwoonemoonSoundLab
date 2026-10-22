// SPDX-License-Identifier: GPL-3.0-or-later
// Главное окно: шапка с навигацией, страницы, нижняя панель Input/Output.
#include "MainUI.h"

#include "Controls.h"
#include "Theme.h"
#include "TwoonemoonSoundLab.h"
#include "TunerView.h"
#include "UIContext.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>

namespace tslui {

namespace {

using namespace tsl;

constexpr float kW = PLUG_WIDTH;
constexpr float kH = PLUG_HEIGHT;
constexpr float kHeaderH = 64.f;
constexpr float kFooterH = 92.f;

const char* const kPageGroups[] = { "page.play", "page.chain", "page.amp", "page.cab", "page.fx", "page.eq" };
const char* const kPageTitles[] = { "Игра", "Цепочка", "Усилитель", "Кабинет", "Эффекты", "EQ" };
constexpr int kNumPages = 6;
enum Page { kPagePlay, kPageChain, kPageAmp, kPageCab, kPageFx, kPageEq };

/** Масштабы интерфейса (векторная перерисовка, не растяжение bitmap). */
constexpr float kScales[] = { 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 2.5f };

class ScaleButton : public ActionButton
{
public:
  explicit ScaleButton(const IRECT& r) : ActionButton(r, "100%", nullptr, true) {}
  void OnMouseDown(float, float, const IMouseMod&) override
  {
    mMenu.Clear();
    mMenu.SetRootTitle("Масштаб интерфейса");
    const float cur = GetUI()->GetDrawScale();
    for (float s : kScales)
    {
      char buf[16];
      snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(s * 100.f + 0.5f));
      auto* item = mMenu.AddItem(buf);
      if (std::fabs(s - cur) < 0.01f) item->SetChecked(true);
    }
    GetUI()->CreatePopupMenu(*this, mMenu, mRECT);
  }
  void OnPopupMenuSelection(IPopupMenu* pMenu, int) override
  {
    if (!pMenu) return;
    const int idx = pMenu->GetChosenItemIdx();
    if (idx < 0 || idx >= static_cast<int>(std::size(kScales))) return;
    GetUI()->Resize(static_cast<int>(kW), static_cast<int>(kH), kScales[idx]);
  }
  void Draw(IGraphics& g) override
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(g.GetDrawScale() * 100.f + 0.5f));
    SetLabelStr(buf);
    ActionButton::Draw(g);
  }

private:
  IPopupMenu mMenu;
};

/** Краткий обзор цепочки в нижней панели: перерисовывается при изменении включённых модулей/порядка. */
class ChainStrip : public IControl
{
public:
  ChainStrip(const IRECT& r, TwoonemoonSoundLab* plugin) : IControl(r), mPlugin(plugin) { mIgnoreMouse = true; }
  bool IsDirty() override
  {
    uint32_t mask = 0;
    for (int i = 0; i < kNumModules; ++i)
      if (mPlugin->GetParam(moduleEnableParam(static_cast<ModuleId>(i)))->Bool()) mask |= 1u << i;
    const uint32_t serial = mPlugin->stateSerial();
    if (mask != mMask || serial != mSerial) return true;
    return IControl::IsDirty();
  }
  void Draw(IGraphics& gr) override
  {
    mMask = 0;
    mSerial = mPlugin->stateSerial();
    const ChainOrder order = mPlugin->chainOrder();
    const float w = mRECT.W() / static_cast<float>(kNumModules);
    for (int i = 0; i < kNumModules; ++i)
    {
      const ModuleId id = order[static_cast<size_t>(i)];
      const bool on = mPlugin->GetParam(moduleEnableParam(id))->Bool();
      if (on) mMask |= 1u << static_cast<int>(id);
      const IRECT cell = IRECT(mRECT.L + w * i, mRECT.T, mRECT.L + w * (i + 1), mRECT.B).GetPadded(-1.5f, -8.f, -1.5f, -8.f);
      gr.PathClear();
      gr.PathRoundRect(cell, cell.H() * 0.5f);
      gr.PathFill(on ? IColor(40, 196, 92, 166) : col::bg2);
      gr.PathClear();
      gr.PathRoundRect(cell.GetPadded(-0.5f), cell.H() * 0.5f);
      gr.PathStroke(on ? IColor(150, 196, 92, 166) : col::hairline, 1.f);
      gr.DrawText(txt(9.5f, on ? col::text : col::text3, font::semibold), moduleShortName(id), cell);
    }
  }

private:
  TwoonemoonSoundLab* mPlugin;
  uint32_t mMask = 0xFFFFFFFF, mSerial = 0xFFFFFFFF;
};

/** Затемнение под оверлеем; клик вне панели закрывает оверлей. */
class OverlayBlocker : public IControl
{
public:
  OverlayBlocker(const IRECT& r, std::function<void()> onClose) : IControl(r), mOnClose(std::move(onClose)) {}
  void Draw(IGraphics& g) override { g.FillRect(IColor(170, 6, 6, 8), mRECT); }
  void OnMouseDown(float, float, const IMouseMod&) override
  {
    if (mOnClose) mOnClose();
  }

private:
  std::function<void()> mOnClose;
};

constexpr const char* kTunerGroup = "overlay.tuner";

void setTunerVisible(IGraphics* g, bool visible)
{
  g->ForControlInGroup(kTunerGroup, [visible](IControl* c) { c->Hide(!visible); });
  g->SetAllControlsDirty();
}

void showPage(UIContext& ctx, int page)
{
  for (int i = 0; i < kNumPages; ++i)
  {
    const bool hide = i != page;
    ctx.g->ForControlInGroup(kPageGroups[i], [hide](IControl* c) { c->Hide(hide); });
  }
  ctx.notify(-1); // страницы с подгруппами (усилитель) уточняют видимость
  ctx.g->SetAllControlsDirty();
}

} // namespace

void buildMainUI(IGraphics* g, TwoonemoonSoundLab& plugin)
{
  g->EnableMouseOver(true);
  g->EnableTooltips(true);
  loadFonts(g);
  g->AttachPopupMenuControl(txt(13.f, col::text, font::medium));
  g->SetLayoutOnResize(false);

  auto ctxPtr = std::make_shared<UIContext>();
  UIContext& ctx = *ctxPtr;
  ctx.g = g;
  ctx.plugin = &plugin;
  plugin.setUIParamListener([ctxPtr](int idx) { ctxPtr->notify(idx); });

  const IRECT b = g->GetBounds();
  Engine& engine = plugin.engine();

  // ---------------------------------------------------------------- Фон
  g->AttachControl(new Canvas(b, [](IGraphics& gr, const IRECT& r) { drawBackdrop(gr, r); }));

  // ---------------------------------------------------------------- Шапка
  const IRECT header = b.GetFromTop(kHeaderH);
  g->AttachControl(new Canvas(header, [](IGraphics& gr, const IRECT& r) {
    const IRECT mark = r.GetFromLeft(220.f).GetPadded(-24.f, -14.f, 0.f, -14.f);
    gr.DrawText(txt(17.f, col::text, font::semibold, EAlign::Near), "TWOONEMOON", mark.GetFromTop(20.f));
    drawCapsLabel(gr, "Sound Lab", mark.GetFromBottom(14.f), col::text3, 10.f, EAlign::Near);
    gr.FillRect(col::accent, IRECT(mark.L, mark.MH() + 2.f, mark.L + 18.f, mark.MH() + 3.f));
    gr.FillRect(col::hairline, IRECT(r.L, r.B - 1.f, r.R, r.B));
  }));

  std::vector<std::string> tabs(kPageTitles, kPageTitles + kNumPages);
  const float tabW = kNumPages * 104.f;
  auto* tabBar = new TabBar(IRECT(575.f - tabW * 0.5f, header.MH() - 19.f, 575.f + tabW * 0.5f, header.MH() + 19.f), tabs,
                            [ctxPtr](int page) { showPage(*ctxPtr, page); });
  g->AttachControl(tabBar);

  const float hy = header.MH();
  g->AttachControl(new ToggleChip(IRECT(kW - 116.f, hy - 14.f, kW - 20.f, hy + 14.f), kBypass, "Bypass", col::hot));
  g->AttachControl(new ActionButton(IRECT(kW - 212.f, hy - 14.f, kW - 126.f, hy + 14.f), "Тюнер", [g]() { setTunerVisible(g, true); }));
  g->AttachControl(new ScaleButton(IRECT(kW - 278.f, hy - 14.f, kW - 222.f, hy + 14.f)));

  // ---------------------------------------------------------------- Нижняя панель
  const IRECT footer = b.GetFromBottom(kFooterH);
  g->AttachControl(new Canvas(footer, [](IGraphics& gr, const IRECT& r) {
    gr.FillRect(IColor(255, 12, 13, 16), r);
    gr.FillRect(col::hairline, IRECT(r.L, r.T, r.R, r.T + 1.f));
  }));
  const IRECT fIn = footer.GetFromLeft(260.f).GetPadded(-18.f, -10.f, 0.f, -8.f);
  g->AttachControl(new LevelMeterView(fIn.GetFromLeft(22.f), &engine.inputMeter(), "In"));
  g->AttachControl(new Knob(fIn.GetReducedFromLeft(34.f).GetFromLeft(70.f), kInputTrim, "Input", KnobStyle::Steel, col::accent, true));
  g->AttachControl(new EnumSelector(fIn.GetReducedFromLeft(116.f).GetFromLeft(120.f).GetCentredInside(120.f, 44.f).GetVShifted(-4.f), kInputMode, "Вход"));

  const IRECT fOut = footer.GetFromRight(200.f).GetPadded(0.f, -10.f, -18.f, -8.f);
  g->AttachControl(new LevelMeterView(fOut.GetFromRight(22.f), &engine.outputMeter(), "Out"));
  g->AttachControl(new Knob(fOut.GetReducedFromRight(34.f).GetFromRight(70.f), kOutputTrim, "Output", KnobStyle::Steel, col::accent, false));

  // Обзор цепочки
  g->AttachControl(new ChainStrip(footer.GetPadded(-280.f, -22.f, -220.f, -22.f), &plugin));

  // ---------------------------------------------------------------- Страницы
  ctx.content = b.GetReducedFromTop(kHeaderH).GetReducedFromBottom(kFooterH).GetPadded(-20.f);
  buildPlayPage(ctx, kPageGroups[kPagePlay]);
  buildChainPage(ctx, kPageGroups[kPageChain]);
  buildAmpPage(ctx, kPageGroups[kPageAmp]);
  buildCabPage(ctx, kPageGroups[kPageCab]);
  buildFxPage(ctx, kPageGroups[kPageFx]);
  buildEqPage(ctx, kPageGroups[kPageEq]);

  // ---------------------------------------------------------------- Оверлей тюнера (поверх всего)
  {
    g->AttachControl(new OverlayBlocker(b.GetReducedFromTop(kHeaderH), [g]() { setTunerVisible(g, false); }), kNoTag, kTunerGroup);
    const IRECT panel = b.GetCentredInside(640.f, 400.f).GetVShifted(10.f);
    g->AttachControl(new Canvas(panel, [](IGraphics& gr, const IRECT& r) {
      drawHardwarePanel(gr, r, 18.f, col::bg2);
      gr.DrawText(txt(15.f, col::text, font::semibold, EAlign::Near), "Тюнер", r.GetPadded(-22.f, -16.f, 0.f, 0.f).GetFromTop(20.f));
    }), kNoTag, kTunerGroup);
    g->AttachControl(new TunerView(panel.GetPadded(-30.f, -50.f, -30.f, -84.f), &plugin), kNoTag, kTunerGroup);
    const IRECT bottom = panel.GetFromBottom(72.f).GetPadded(-24.f, 0.f, -24.f, -14.f);
    g->AttachControl(new ToggleChip(IRECT(bottom.L, bottom.MH() - 13.f, bottom.L + 150.f, bottom.MH() + 13.f), kTunerMute, "Mute выхода", col::warn), kNoTag, kTunerGroup);
    g->AttachControl(new Knob(IRECT(bottom.MW() - 40.f, bottom.T - 10.f, bottom.MW() + 40.f, bottom.B + 6.f), kTunerRef, "A4 Ref", KnobStyle::Small), kNoTag, kTunerGroup);
    g->AttachControl(new ActionButton(IRECT(bottom.R - 120.f, bottom.MH() - 14.f, bottom.R, bottom.MH() + 14.f), "Закрыть",
                                      [g]() { setTunerVisible(g, false); }, false), kNoTag, kTunerGroup);
    setTunerVisible(g, getenv("TSL_SHOW_TUNER") != nullptr);
  }

  // Стартовая страница; TSL_START_PAGE — для автоматических скриншотов при разработке
  int startPage = 0;
  if (const char* sp = getenv("TSL_START_PAGE")) startPage = std::clamp(atoi(sp), 0, kNumPages - 1);
  ctx.openModule = [ctxPtr, tabBar](ModuleId id) {
    int page = kPageFx;
    if (id == ModuleId::Amp) page = kPageAmp;
    else if (id == ModuleId::Cabinet) page = kPageCab;
    else if (id == ModuleId::Eq) page = kPageEq;
    tabBar->SetSelected(page);
    showPage(*ctxPtr, page);
    if (page == kPageFx && ctxPtr->selectFx) ctxPtr->selectFx(id);
  };
  tabBar->SetSelected(startPage);
  showPage(ctx, startPage);
}

} // namespace tslui
