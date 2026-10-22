// SPDX-License-Identifier: GPL-3.0-or-later
// Главное окно: шапка с навигацией, страницы, нижняя панель Input/Output.
#include "MainUI.h"

#include "Controls.h"
#include "Theme.h"
#include "TwoonemoonSoundLab.h"
#include "HelpContent.h"
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

const char* const kPageGroups[] = { "page.play", "page.presets", "page.chain", "page.amp", "page.cab", "page.fx", "page.eq", "page.settings", "page.help" };
const char* const kPageTitles[] = { "Игра", "Пресеты", "Цепочка", "Усилитель", "Кабинет", "Эффекты", "EQ" };
constexpr int kNumTabs = 7;  // страницы во вкладках
constexpr int kNumPages = 9; // + служебные (настройки, справка) — открываются иконками в шапке
enum Page { kPagePlay, kPagePresets, kPageChain, kPageAmp, kPageCab, kPageFx, kPageEq, kPageSettings, kPageHelp };

/** Масштабы интерфейса (векторная перерисовка, не растяжение bitmap). */
constexpr float kScales[] = { 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 2.5f };

class ScaleButton : public ActionButton
{
public:
  ScaleButton(const IRECT& r, UIContext* ctx) : ActionButton(r, "100%", nullptr, true), mCtx(ctx) {}
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
    mCtx->prefs.uiScale = kScales[idx];
    mCtx->prefs.save();
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
  UIContext* mCtx;
};

/** Контекстное меню параметра: MIDI Learn, удаление привязки, точное значение, сброс. */
class ParamContextMenu : public IControl
{
public:
  ParamContextMenu(TwoonemoonSoundLab* plugin) : IControl(IRECT()), mPlugin(plugin) { mIgnoreMouse = true; }
  void Draw(IGraphics&) override {}
  void open(IControl& src, int param, const IRECT& r)
  {
    mSrc = &src;
    mParam = param;
    auto& mm = mPlugin->midiMap();
    const auto m = mm.mappingFor(param);
    mMenu.Clear();
    mMenu.SetRootTitle(paramSpec(param).name);
    mMenu.AddItem(mm.learnTarget() == param ? "Отменить MIDI Learn" : "MIDI Learn");
    char buf[64];
    if (m.cc >= 0) snprintf(buf, sizeof(buf), "Убрать MIDI-привязку (CC %d)", m.cc);
    else snprintf(buf, sizeof(buf), "Убрать MIDI-привязку");
    auto* rm = mMenu.AddItem(buf);
    if (m.cc < 0) rm->SetEnabled(false);
    mMenu.AddSeparator();
    mMenu.AddItem("Ввести значение…");
    mMenu.AddItem("Сбросить к значению по умолчанию");
    GetUI()->CreatePopupMenu(*this, mMenu, r);
  }
  void OnPopupMenuSelection(IPopupMenu* pMenu, int) override
  {
    if (!pMenu || !mSrc || mParam < 0) return;
    auto& mm = mPlugin->midiMap();
    switch (pMenu->GetChosenItemIdx())
    {
      case 0: mm.startLearn(mm.learnTarget() == mParam ? -1 : mParam); break;
      case 1: mm.removeForParam(mParam); break;
      case 3: mSrc->PromptUserInput(mSrc->GetRECT().GetFromBottom(18.f)); break;
      case 4: mSrc->SetValueToDefault(); mSrc->SetDirty(true); break;
      default: break;
    }
  }

private:
  TwoonemoonSoundLab* mPlugin;
  IControl* mSrc = nullptr;
  int mParam = -1;
  IPopupMenu mMenu;
};

constexpr const char* kOnboardingGroup = "overlay.onboarding";

/** Карточка первого знакомства: шаг, текст, «Назад/Далее/Пропустить». */
class OnboardingCard : public IControl
{
public:
  OnboardingCard(const IRECT& r, UIContext* ctx) : IControl(r), mCtx(ctx) {}
  void setStep(int s) { mStep = s; SetDirty(false); }
  void Draw(IGraphics& g) override
  {
    const auto& steps = onboardingSteps();
    drawHardwarePanel(g, mRECT, 18.f, col::bg2);
    g.PathClear();
    g.PathRoundRect(IRECT(mRECT.L, mRECT.T, mRECT.R, mRECT.T + 4.f), 2.f);
    g.PathFill(col::accent);
    const auto& st = steps[static_cast<size_t>(mStep)];
    const IRECT in = mRECT.GetPadded(-26.f);
    g.DrawText(txt(20.f, col::text, font::semibold, EAlign::Near), st.title, in.GetFromTop(30.f));
    g.DrawMultiLineText(txt(13.f, col::text2, font::regular, EAlign::Near, EVAlign::Top), st.body, IRECT(in.L, in.T + 42.f, in.R, in.B - 50.f));
    // точки прогресса
    for (size_t i = 0; i < steps.size(); ++i)
      g.FillCircle(static_cast<int>(i) == mStep ? col::accent : col::bg3, in.L + 6.f + i * 16.f, in.B - 16.f, 4.f);
    mBack = IRECT(in.R - 300.f, in.B - 32.f, in.R - 210.f, in.B);
    mNext = IRECT(in.R - 110.f, in.B - 32.f, in.R, in.B);
    mSkip = IRECT(in.R - 200.f, in.B - 32.f, in.R - 120.f, in.B);
    auto btn = [&](const IRECT& r, const char* l, bool hl) {
      g.PathClear();
      g.PathRoundRect(r, r.H() * 0.5f);
      g.PathFill(hl ? IColor(70, 196, 92, 166) : col::bg3);
      g.DrawText(txt(12.f, col::text, font::medium), l, r);
    };
    const bool last = mStep + 1 >= static_cast<int>(steps.size());
    if (mStep > 0) btn(mBack, "Назад", false);
    if (!last) btn(mSkip, "Пропустить", false);
    btn(mNext, last ? "Начать" : "Далее", true);
  }
  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    const int n = static_cast<int>(onboardingSteps().size());
    if (mNext.Contains(x, y))
    {
      if (mStep + 1 >= n) finish();
      else mStep++;
    }
    else if (mBack.Contains(x, y) && mStep > 0) mStep--;
    else if (mSkip.Contains(x, y)) finish();
    SetDirty(false);
  }

private:
  void finish()
  {
    mCtx->prefs.onboardingDone = true;
    mCtx->prefs.save();
    mCtx->g->ForControlInGroup(kOnboardingGroup, [](IControl* c) { c->Hide(true); });
    mCtx->g->SetAllControlsDirty();
  }
  UIContext* mCtx;
  int mStep = 0;
  IRECT mBack, mNext, mSkip;
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
    // Показываем только включённые модули в порядке обработки (выключенных — 25, места на все нет)
    mMask = 0;
    mSerial = mPlugin->stateSerial();
    const ChainOrder order = mPlugin->chainOrder();
    std::vector<ModuleId> on;
    for (ModuleId id : order)
      if (mPlugin->GetParam(moduleEnableParam(id))->Bool())
      {
        on.push_back(id);
        mMask |= 1u << static_cast<int>(id);
      }
    if (on.empty())
    {
      gr.DrawText(txt(11.f, col::text3, font::regular), "Все модули выключены — сигнал проходит без обработки", mRECT);
      return;
    }
    const float cw = std::min(64.f, (mRECT.W() - 10.f * (on.size() - 1)) / on.size());
    float x = mRECT.MW() - (cw * on.size() + 10.f * (on.size() - 1)) * 0.5f;
    for (size_t i = 0; i < on.size(); ++i)
    {
      const IRECT cell(x, mRECT.T + 8.f, x + cw, mRECT.B - 8.f);
      const IColor c = moduleColor(on[i]);
      gr.PathClear();
      gr.PathRoundRect(cell, cell.H() * 0.5f);
      gr.PathFill(IColor(46, c.R, c.G, c.B));
      gr.PathClear();
      gr.PathRoundRect(cell.GetPadded(-0.5f), cell.H() * 0.5f);
      gr.PathStroke(IColor(170, c.R, c.G, c.B), 1.f);
      gr.DrawText(txt(9.5f, col::text, font::semibold), moduleShortName(on[i]), cell);
      if (i + 1 < on.size()) gr.DrawLine(col::text3, cell.R + 2.f, cell.MH(), cell.R + 8.f, cell.MH(), nullptr, 1.f);
      x += cw + 10.f;
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
  loadFonts(g);
  g->AttachPopupMenuControl(txt(13.f, col::text, font::medium));
  g->SetLayoutOnResize(false);

  auto ctxPtr = std::make_shared<UIContext>();
  UIContext& ctx = *ctxPtr;
  ctx.g = g;
  ctx.plugin = &plugin;
  ctx.prefs = UserSettings::load();
  g->EnableTooltips(ctx.prefs.tooltips);
  uiFlags().reduceMotion = ctx.prefs.reduceMotion;
  // Хуки параметрических контролов
  paramHooks().midiState = [&plugin](int param) {
    auto& mm = plugin.midiMap();
    if (mm.learnTarget() == param) return 2;
    return mm.mappingFor(param).cc >= 0 ? 1 : 0;
  };
  paramHooks().tooltip = [](int param) { return paramTooltip(paramSpec(param).key); };
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

  std::vector<std::string> tabs(kPageTitles, kPageTitles + kNumTabs);
  auto* tabBar = new TabBar(IRECT(228.f, header.MH() - 19.f, 228.f + kNumTabs * 87.f, header.MH() + 19.f), tabs,
                            [ctxPtr](int page) { showPage(*ctxPtr, page); });
  g->AttachControl(tabBar);

  const float hy = header.MH();
  g->AttachControl(new ToggleChip(IRECT(kW - 116.f, hy - 14.f, kW - 20.f, hy + 14.f), kBypass, "Bypass", col::hot));
  g->AttachControl(new ActionButton(IRECT(kW - 212.f, hy - 14.f, kW - 126.f, hy + 14.f), "Тюнер", [g]() { setTunerVisible(g, true); }));
  g->AttachControl(new ScaleButton(IRECT(kW - 278.f, hy - 14.f, kW - 222.f, hy + 14.f), &ctx));
  g->AttachControl(new IconButton(IRECT(kW - 318.f, hy - 14.f, kW - 290.f, hy + 14.f), IconButton::Icon::Help, [ctxPtr]() {
    if (ctxPtr->gotoPage) ctxPtr->gotoPage(kPageHelp);
  }))->SetTooltip("Справка");
  g->AttachControl(new IconButton(IRECT(kW - 352.f, hy - 14.f, kW - 324.f, hy + 14.f), IconButton::Icon::Gear, [ctxPtr]() {
    if (ctxPtr->gotoPage) ctxPtr->gotoPage(kPageSettings);
  }))->SetTooltip("Настройки");

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
  buildPresetsPage(ctx, kPageGroups[kPagePresets]);
  buildChainPage(ctx, kPageGroups[kPageChain]);
  buildAmpPage(ctx, kPageGroups[kPageAmp]);
  buildCabPage(ctx, kPageGroups[kPageCab]);
  buildFxPage(ctx, kPageGroups[kPageFx]);
  buildEqPage(ctx, kPageGroups[kPageEq]);
  buildSettingsPage(ctx, kPageGroups[kPageSettings]);
  buildHelpPage(ctx, kPageGroups[kPageHelp]);

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
  ctx.gotoPage = [ctxPtr, tabBar](int page) {
    if (page < 0) page = kPagePresets;
    tabBar->SetSelected(page < kNumTabs ? page : -1);
    showPage(*ctxPtr, page);
  };
  // Контекстное меню параметров (правый клик по ручке)
  auto* paramMenu = new ParamContextMenu(&plugin);
  g->AttachControl(paramMenu);
  paramHooks().openMenu = [paramMenu](IControl& src, int param, const IRECT& r) { paramMenu->open(src, param, r); };
  // Первое знакомство
  {
    g->AttachControl(new OverlayBlocker(b, nullptr), kNoTag, kOnboardingGroup);
    auto* card = new OnboardingCard(b.GetCentredInside(560.f, 250.f), &ctx);
    g->AttachControl(card, kNoTag, kOnboardingGroup);
    ctx.showOnboarding = [g, card]() {
      card->setStep(0);
      g->ForControlInGroup(kOnboardingGroup, [](IControl* c) { c->Hide(false); });
      g->SetAllControlsDirty();
    };
    const bool show = (!ctx.prefs.onboardingDone && getenv("TSL_START_PAGE") == nullptr) || getenv("TSL_SHOW_ONBOARDING") != nullptr;
    g->ForControlInGroup(kOnboardingGroup, [show](IControl* c) { c->Hide(!show); });
  }
  // Горячие клавиши
  g->SetKeyHandlerFunc([ctxPtr, g](const IKeyPress& key, bool isUp) {
    if (isUp) return false;
    TwoonemoonSoundLab* pl = ctxPtr->plugin;
    if (key.C && (key.VK == kVK_Z || key.utf8[0] == 'z' || key.utf8[0] == 'Z'))
    {
      key.S ? pl->redo() : pl->undo();
      return true;
    }
    if (key.VK == kVK_LEFT) { pl->stepPreset(-1); return true; }
    if (key.VK == kVK_RIGHT) { pl->stepPreset(1); return true; }
    if (key.utf8[0] == 't' || key.utf8[0] == 'T')
    {
      setTunerVisible(g, true);
      return true;
    }
    if (key.VK == kVK_ESCAPE) { setTunerVisible(g, false); return true; }
    return false;
  });
  tabBar->SetSelected(startPage < kNumTabs ? startPage : -1);
  showPage(ctx, startPage);
}

} // namespace tslui
