// SPDX-License-Identifier: GPL-3.0-or-later
// Главное окно: шапка с навигацией, страницы, нижняя панель Input/Output.
#include "MainUI.h"

#include "Controls.h"
#include "Theme.h"
#include "TwoonemoonSoundLab.h"
#include "HelpContent.h"
#include "TunerView.h"
#include "AmpFaces.h"
#include "Amps/AmpVoices.h"
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
    const float cur = shownScale();
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
    mCtx->plugin->requestUIScale(kScales[idx]);
  }
  void Draw(IGraphics& g) override
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(shownScale() * 100.f + 0.5f));
    SetLabelStr(buf);
    ActionButton::Draw(g);
  }

private:
  /** Компактный режим рисуется в масштабе 1.0, но для пользователя это «50%». */
  float shownScale() const { return GetUI()->Width() < kW ? 0.5f : GetUI()->GetDrawScale(); }
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


// ============================================================================ Компактный режим
/** Название пресета для компактной шапки. */
class CompactPresetName : public IControl
{
public:
  CompactPresetName(const IRECT& r, TwoonemoonSoundLab* plugin) : IControl(r), mPlugin(plugin) { mIgnoreMouse = true; }
  bool IsDirty() override
  {
    return mPlugin->currentPresetName() != mName || mPlugin->presetModified() != mMod || IControl::IsDirty();
  }
  void Draw(IGraphics& g) override
  {
    mName = mPlugin->currentPresetName();
    mMod = mPlugin->presetModified();
    drawCapsLabel(g, mMod ? "Пресет · изменён" : "Пресет", mRECT.GetFromTop(13.f), mMod ? col::warn : col::text3, 8.5f);
    g.DrawText(txt(14.f, col::text, font::semibold), mName.c_str(), mRECT.GetReducedFromTop(13.f));
  }

private:
  TwoonemoonSoundLab* mPlugin;
  std::string mName;
  bool mMod = false;
};

/** Фон панели усилителя в стиле текущей модели. */
class CompactAmpPanel : public IControl
{
public:
  CompactAmpPanel(const IRECT& r, TwoonemoonSoundLab* plugin) : IControl(r), mPlugin(plugin) { mIgnoreMouse = true; }
  bool IsDirty() override { return mPlugin->GetParam(kAmpModel)->Int() != mModel || IControl::IsDirty(); }
  void Draw(IGraphics& g) override
  {
    mModel = mPlugin->GetParam(kAmpModel)->Int();
    const AmpFaceStyle& st = ampFaceStyle(mModel);
    g.PathClear();
    g.PathRoundRect(mRECT, 12.f);
    g.PathFill(IPattern::CreateLinearGradient(mRECT.L, mRECT.T, mRECT.L, mRECT.B, { { st.tolexHi, 0.f }, { st.tolex, 1.f } }));
    drawBrushedRect(g, mRECT.GetPadded(-8.f, -42.f, -8.f, -8.f), 8.f, st.panel, st.panelHi, st.seed);
  }

private:
  TwoonemoonSoundLab* mPlugin;
  int mModel = -1;
};


void buildCompactUI(IGraphics* g, TwoonemoonSoundLab& plugin, std::shared_ptr<UIContext> ctxPtr)
{
  UIContext& ctx = *ctxPtr;
  TwoonemoonSoundLab* pl = &plugin;
  Engine& engine = plugin.engine();
  const IRECT b = g->GetBounds();
  g->AttachControl(new Canvas(b, [](IGraphics& gr, const IRECT& r) { drawBackdrop(gr, r); }));

  // ---- Шапка: марка, пресет, тюнер, масштаб, bypass
  const IRECT header = b.GetFromTop(46.f);
  g->AttachControl(new Canvas(header, [](IGraphics& gr, const IRECT& r) {
    const IRECT mark(r.L + 14.f, r.T + 9.f, r.L + 110.f, r.B - 9.f);
    gr.DrawText(txt(12.f, col::text, font::semibold, EAlign::Near), "TWOONEMOON", mark.GetFromTop(15.f));
    drawCapsLabel(gr, "Sound Lab", mark.GetFromBottom(11.f), col::text3, 8.f, EAlign::Near);
    gr.FillRect(col::hairline, IRECT(r.L, r.B - 1.f, r.R, r.B));
  }));
  const float hy = header.MH();
  g->AttachControl(new IconButton(IRECT(112.f, hy - 13.f, 138.f, hy + 13.f), IconButton::Icon::Left, [pl]() { pl->stepPreset(-1); }))->SetTooltip("Предыдущий пресет");
  g->AttachControl(new CompactPresetName(IRECT(140.f, header.T + 6.f, 336.f, header.B - 6.f), pl));
  g->AttachControl(new IconButton(IRECT(338.f, hy - 13.f, 364.f, hy + 13.f), IconButton::Icon::Right, [pl]() { pl->stepPreset(1); }))->SetTooltip("Следующий пресет");
  g->AttachControl(new ActionButton(IRECT(370.f, hy - 13.f, 430.f, hy + 13.f), "Тюнер", [g]() { setTunerVisible(g, true); }));
  g->AttachControl(new ScaleButton(IRECT(434.f, hy - 13.f, 482.f, hy + 13.f), &ctx))->SetTooltip("Масштаб интерфейса");
  g->AttachControl(new ToggleChip(IRECT(486.f, hy - 13.f, 588.f, hy + 13.f), kBypass, "Bypass", col::hot));

  // ---- Усилитель: модель и шесть основных регуляторов
  const IRECT amp(12.f, header.B + 8.f, b.R - 12.f, header.B + 154.f);
  g->AttachControl(new CompactAmpPanel(amp, pl));
  g->AttachControl(new EnumSelector(IRECT(amp.L + 10.f, amp.T + 7.f, amp.L + 250.f, amp.T + 33.f), kAmpModel));
  g->AttachControl(new Label(IRECT(amp.L + 262.f, amp.T + 7.f, amp.R - 12.f, amp.T + 33.f), "Компактный режим: основные регуляторы",
                             txt(10.5f, col::text2, font::regular, EAlign::Far)));
  {
    const IRECT knobs = amp.GetPadded(-14.f, -44.f, -14.f, -10.f);
    const int ids[6] = { kAmpGain, kAmpBass, kAmpMid, kAmpTreble, kAmpMaster, kAmpLevel };
    const char* names[6] = { "Gain", "Bass", "Middle", "Treble", "Master", "Volume" };
    const float w = knobs.W() / 6.f;
    std::vector<Knob*> ks;
    for (int i = 0; i < 6; ++i)
    {
      auto* k = new Knob(IRECT(knobs.L + w * i, knobs.T, knobs.L + w * (i + 1), knobs.B), ids[i], names[i], KnobStyle::Steel, col::accent, ids[i] == kAmpLevel);
      g->AttachControl(k);
      ks.push_back(k);
    }
    ctx.onParam([pl, ks](int idx) {
      if (idx != kAmpModel && idx != -1 && idx != -2) return;
      const AmpFaceStyle& st = ampFaceStyle(pl->GetParam(kAmpModel)->Int());
      for (auto* k : ks) { k->SetStyle(st.knob); k->SetAccent(st.knobAccent); k->SetLabelColor(st.label); }
    });
    ctx.notify(-1);
  }

  // ---- Эффекты: шесть колонок (вкл/выкл + главный регулятор)
  struct Col { ModuleId id; int knob; const char* label; };
  const Col cols[6] = { { ModuleId::Gate, kGateThreshold, "Threshold" }, { ModuleId::Overdrive, kOdDrive, "Drive" }, { ModuleId::Distortion, kDistDrive, "Drive" },
                        { ModuleId::Chorus, kChoDepth, "Depth" },        { ModuleId::Delay, kDlyMix, "Mix" },          { ModuleId::Reverb, kRvbMix, "Mix" } };
  const IRECT fx(12.f, amp.B + 8.f, b.R - 12.f, b.B - 70.f);
  g->AttachControl(new Canvas(fx, [](IGraphics& gr, const IRECT& r) { drawHardwarePanel(gr, r, 12.f); }));
  const float cw = (fx.W() - 12.f) / 6.f;
  for (int i = 0; i < 6; ++i)
  {
    const float x = fx.L + 6.f + cw * i;
    const IColor c = moduleColor(cols[i].id);
    g->AttachControl(new ToggleChip(IRECT(x + 4.f, fx.T + 8.f, x + cw - 4.f, fx.T + 32.f), moduleEnableParam(cols[i].id), moduleShortName(cols[i].id), c))
      ->SetTooltip(moduleDisplayName(cols[i].id));
    g->AttachControl(new Knob(IRECT(x, fx.T + 36.f, x + cw, fx.B - 4.f), cols[i].knob, cols[i].label, KnobStyle::Small, c));
  }

  // ---- Низ: вход/выход и возврат к полному интерфейсу
  const IRECT footer = b.GetFromBottom(64.f);
  g->AttachControl(new Canvas(footer, [](IGraphics& gr, const IRECT& r) {
    gr.FillRect(IColor(255, 12, 13, 16), r);
    gr.FillRect(col::hairline, IRECT(r.L, r.T, r.R, r.T + 1.f));
  }));
  const IRECT fr = footer.GetPadded(-12.f, -6.f, -12.f, -4.f);
  g->AttachControl(new LevelMeterView(fr.GetFromLeft(18.f), &engine.inputMeter(), "In"));
  g->AttachControl(new Knob(IRECT(fr.L + 24.f, fr.T, fr.L + 84.f, fr.B), kInputTrim, "Input", KnobStyle::Small, col::accent, true));
  g->AttachControl(new LevelMeterView(fr.GetFromRight(18.f), &engine.outputMeter(), "Out"));
  g->AttachControl(new Knob(IRECT(fr.R - 84.f, fr.T, fr.R - 24.f, fr.B), kOutputTrim, "Output", KnobStyle::Small, col::accent, false));
  g->AttachControl(new ActionButton(IRECT(fr.MW() - 110.f, fr.MH() - 14.f, fr.MW() + 110.f, fr.MH() + 14.f), "Полный интерфейс (100%)", [ctxPtr]() {
    ctxPtr->prefs.uiScale = 1.f;
    ctxPtr->prefs.save();
    ctxPtr->plugin->requestUIScale(1.f);
  }));

  // ---- Тюнер (оверлей)
  g->AttachControl(new OverlayBlocker(b, [g]() { setTunerVisible(g, false); }), kNoTag, kTunerGroup);
  const IRECT panel = b.GetCentredInside(460.f, 300.f);
  g->AttachControl(new Canvas(panel, [](IGraphics& gr, const IRECT& r) {
    drawHardwarePanel(gr, r, 16.f, col::bg2);
    gr.DrawText(txt(14.f, col::text, font::semibold, EAlign::Near), "Тюнер", r.GetPadded(-18.f, -12.f, 0.f, 0.f).GetFromTop(20.f));
  }), kNoTag, kTunerGroup);
  g->AttachControl(new TunerView(panel.GetPadded(-24.f, -40.f, -24.f, -58.f), pl), kNoTag, kTunerGroup);
  const IRECT pb = panel.GetFromBottom(50.f).GetPadded(-18.f, 0.f, -18.f, -12.f);
  g->AttachControl(new ToggleChip(IRECT(pb.L, pb.MH() - 13.f, pb.L + 150.f, pb.MH() + 13.f), kTunerMute, "Mute выхода", col::warn), kNoTag, kTunerGroup);
  g->AttachControl(new ActionButton(IRECT(pb.R - 110.f, pb.MH() - 14.f, pb.R, pb.MH() + 14.f), "Закрыть", [g]() { setTunerVisible(g, false); }, false), kNoTag, kTunerGroup);
  setTunerVisible(g, getenv("TSL_SHOW_TUNER") != nullptr);

  // Контекстное меню параметров и горячие клавиши — как в полном режиме
  auto* paramMenu = new ParamContextMenu(pl);
  g->AttachControl(paramMenu);
  paramHooks().openMenu = [paramMenu](IControl& src, int param, const IRECT& r) { paramMenu->open(src, param, r); };
  g->SetKeyHandlerFunc([pl, g](const IKeyPress& key, bool isUp) {
    if (isUp) return false;
    if (key.C && (key.VK == kVK_Z || key.utf8[0] == 'z' || key.utf8[0] == 'Z'))
    {
      key.S ? pl->redo() : pl->undo();
      return true;
    }
    if (key.VK == kVK_LEFT) { pl->stepPreset(-1); return true; }
    if (key.VK == kVK_RIGHT) { pl->stepPreset(1); return true; }
    if (key.utf8[0] == 't' || key.utf8[0] == 'T') { setTunerVisible(g, true); return true; }
    if (key.VK == kVK_ESCAPE) { setTunerVisible(g, false); return true; }
    return false;
  });
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

  if (g->Width() < kW)
  {
    buildCompactUI(g, plugin, ctxPtr);
    return;
  }

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
