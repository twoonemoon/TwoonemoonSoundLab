// SPDX-License-Identifier: GPL-3.0-or-later
#include "MainUI.h"

#include "Controls.h"
#include "Theme.h"
#include "TwoonemoonSoundLab.h"

#include "Effects/Compressor.h"
#include "Effects/NoiseGate.h"

#include <array>

namespace tslui {

namespace {

using namespace tsl;

constexpr float kW = PLUG_WIDTH;
constexpr float kH = PLUG_HEIGHT;
constexpr float kHeaderH = 64.f;
constexpr float kFooterH = 92.f;

const char* const kPageGroups[] = { "page.play" };
const char* const kPageTitles[] = { "Игра" };
constexpr int kNumPages = 1;

/** Масштабы интерфейса (векторная перерисовка, не растяжение bitmap). */
constexpr float kScales[] = { 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 2.5f };

class ScaleButton : public ActionButton
{
public:
  ScaleButton(const IRECT& r) : ActionButton(r, "100%", nullptr, true) {}
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

struct PedalStyle
{
  IColor body, bodyHi, engrave, label;
  KnobStyle knob;
  IColor led;
  int seed;
};

void showPage(IGraphics* g, int page)
{
  for (int i = 0; i < kNumPages; ++i)
  {
    const bool hide = i != page;
    g->ForControlInGroup(kPageGroups[i], [hide](IControl* c) { c->Hide(hide); });
  }
  g->SetAllControlsDirty();
}

/** Корпус педали. */
void drawPedalBody(IGraphics& g, const IRECT& r, const PedalStyle& st, const char* name, const char* sub)
{
  // Тень
  g.PathClear();
  g.PathRoundRect(r.GetTranslated(0.f, 5.f), 14.f);
  g.PathFill(IColor(120, 0, 0, 0));
  // Корпус
  g.PathClear();
  g.PathRoundRect(r, 14.f);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.R, r.B, { { st.bodyHi, 0.f }, { st.body, 0.6f }, { st.body, 1.f } }));
  // Орнамент-рельеф в верхней части
  drawOrnament(g, r.GetFromTop(r.H() * 0.42f).GetPadded(-10.f), st.seed, IColor(60, 0, 0, 0), IColor(40, 255, 255, 255), 0.8f);
  // Кромка
  g.PathClear();
  g.PathRoundRect(r.GetPadded(-0.5f), 13.5f);
  g.PathStroke(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B, { { IColor(70, 255, 255, 255), 0.f }, { IColor(90, 0, 0, 0), 1.f } }), 1.f);
  // Гравированное название
  const IRECT nameR = r.GetFromBottom(86.f).GetFromTop(26.f);
  g.DrawText(txt(15.f, IColor(120, 0, 0, 0), font::bold), name, nameR.GetTranslated(0.f, 1.f));
  g.DrawText(txt(15.f, st.label, font::bold), name, nameR);
  drawCapsLabel(g, sub, nameR.GetTranslated(0.f, 17.f), st.engrave, 8.f);
}

} // namespace

void buildMainUI(IGraphics* g, TwoonemoonSoundLab& plugin)
{
  g->EnableMouseOver(true);
  g->EnableTooltips(true);
  loadFonts(g);
  g->AttachPopupMenuControl(txt(13.f, col::text, font::medium));
  g->SetLayoutOnResize(false);

  const IRECT b = g->GetBounds();
  Engine& engine = plugin.engine();

  // ---------------------------------------------------------------- Фон
  g->AttachControl(new Canvas(b, [](IGraphics& gr, const IRECT& r) { drawBackdrop(gr, r); }));

  // ---------------------------------------------------------------- Шапка
  const IRECT header = b.GetFromTop(kHeaderH);
  g->AttachControl(new Canvas(header, [](IGraphics& gr, const IRECT& r) {
    // Словесный знак: тонкая сталь + акцентная черта
    const IRECT mark = r.GetFromLeft(300.f).GetPadded(-24.f, -14.f, 0.f, -14.f);
    gr.DrawText(txt(17.f, col::text, font::semibold, EAlign::Near), "TWOONEMOON", mark.GetFromTop(20.f));
    drawCapsLabel(gr, "Sound Lab", mark.GetFromBottom(14.f), col::text3, 10.f, EAlign::Near);
    gr.FillRect(col::accent, IRECT(mark.L, mark.MH() + 2.f, mark.L + 18.f, mark.MH() + 3.f));
    gr.FillRect(col::hairline, IRECT(r.L, r.B - 1.f, r.R, r.B));
  }));

  std::vector<std::string> tabs(kPageTitles, kPageTitles + kNumPages);
  auto* tabBar = new TabBar(header.GetCentredInside(kNumPages * 120.f, 38.f), tabs, [g](int page) { showPage(g, page); });
  g->AttachControl(tabBar);

  const IRECT hdrRight = header.GetFromRight(330.f).GetPadded(0.f, -14.f, -20.f, -14.f);
  g->AttachControl(new ToggleChip(hdrRight.GetFromRight(110.f).GetCentredInside(100.f, 28.f), kBypass, "Bypass", col::hot));
  g->AttachControl(new ToggleChip(hdrRight.GetFromRight(220.f).GetFromLeft(105.f).GetCentredInside(100.f, 28.f), kTunerMute, "Mute", col::warn));
  g->AttachControl(new ScaleButton(hdrRight.GetFromLeft(70.f).GetCentredInside(64.f, 28.f)));

  // ---------------------------------------------------------------- Нижняя панель (Input / Output)
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

  // Центр нижней панели — краткий статус цепочки (рисуется по факту включённых модулей)
  const IRECT fMid = footer.GetPadded(-280.f, -20.f, -220.f, -20.f);
  g->AttachControl(new Canvas(fMid, [&plugin](IGraphics& gr, const IRECT& r) {
    const ChainOrder order = plugin.chainOrder();
    const float w = r.W() / static_cast<float>(kNumModules);
    for (int i = 0; i < kNumModules; ++i)
    {
      const ModuleId id = order[static_cast<size_t>(i)];
      const bool on = plugin.GetParam(moduleEnableParam(id))->Bool();
      const IRECT cell = IRECT(r.L + w * i, r.T, r.L + w * (i + 1), r.B).GetPadded(-4.f, -6.f, -4.f, -6.f);
      gr.PathClear();
      gr.PathRoundRect(cell, cell.H() * 0.5f);
      gr.PathFill(on ? IColor(40, 196, 92, 166) : col::bg2);
      gr.PathClear();
      gr.PathRoundRect(cell.GetPadded(-0.5f), cell.H() * 0.5f);
      gr.PathStroke(on ? IColor(150, 196, 92, 166) : col::hairline, 1.f);
      gr.DrawText(txt(11.f, on ? col::text : col::text3, font::medium), moduleDisplayName(id), cell);
      if (i + 1 < kNumModules)
        gr.FillRect(col::hairline, IRECT(cell.R + 2.f, cell.MH() - 0.5f, cell.R + 6.f, cell.MH() + 0.5f));
    }
  }))->SetIgnoreMouse(true);

  // ---------------------------------------------------------------- Страница «Игра»
  const IRECT content = b.GetReducedFromTop(kHeaderH).GetReducedFromBottom(kFooterH).GetPadded(-20.f);
  const char* play = kPageGroups[0];

  // Динамика: Gate + Compressor
  const IRECT dynRow = content.GetFromTop(210.f);
  const IRECT gateR = dynRow.GetFromLeft(dynRow.W() * 0.5f - 8.f);
  const IRECT compR = dynRow.GetFromRight(dynRow.W() * 0.5f - 8.f);

  auto panel = [&](const IRECT& r, const char* title, const char* subtitle, const char* group) {
    g->AttachControl(new Canvas(r, [title, subtitle](IGraphics& gr, const IRECT& rr) {
      drawHardwarePanel(gr, rr, 12.f);
      gr.DrawText(txt(14.f, col::text, font::semibold, EAlign::Near), title, rr.GetPadded(-18.f, -12.f, 0.f, 0.f).GetFromTop(20.f));
      gr.DrawText(txt(11.f, col::text3, font::regular, EAlign::Near), subtitle, rr.GetPadded(-18.f, -32.f, 0.f, 0.f).GetFromTop(16.f));
    }), kNoTag, group);
  };

  panel(gateR, "Noise Gate", "Подавление шума между нотами", play);
  {
    const IRECT knobs = gateR.GetPadded(-14.f, -54.f, -14.f, -14.f);
    const float kw = knobs.W() / 6.f;
    const int ids[] = { kGateThreshold, kGateAttack, kGateHold, kGateRelease, kGateHysteresis, kGateRange };
    const char* names[] = { "Threshold", "Attack", "Hold", "Release", "Hysteresis", "Range" };
    for (int i = 0; i < 6; ++i)
    {
      const IRECT cell(knobs.L + kw * i, knobs.T, knobs.L + kw * (i + 1), knobs.T + 92.f);
      g->AttachControl(new Knob(cell.GetPadded(-4.f), ids[i], names[i], i == 0 ? KnobStyle::Steel : KnobStyle::Small), kNoTag, play);
    }
    const IRECT bottom = knobs.GetFromBottom(42.f);
    g->AttachControl(new ToggleChip(bottom.GetFromLeft(86.f).GetCentredInside(80.f, 26.f), kGateOn, "On"), kNoTag, play);
    g->AttachControl(new EnumSelector(bottom.GetReducedFromLeft(96.f).GetFromLeft(130.f).GetCentredInside(130.f, 26.f), kGateKey), kNoTag, play);
    auto* gate = static_cast<NoiseGate*>(engine.module(ModuleId::Gate));
    g->AttachControl(new ReductionMeter(bottom.GetFromRight(bottom.W() - 240.f).GetPadded(-6.f, -4.f, 0.f, -6.f),
                                        [gate]() { return gate->gainReductionDb(); }, "Подавление", 60.f), kNoTag, play);
  }

  panel(compR, "Compressor", "Выравнивание динамики и sustain", play);
  {
    const IRECT knobs = compR.GetPadded(-14.f, -54.f, -14.f, -14.f);
    const float kw = knobs.W() / 7.f;
    const int ids[] = { kCompThreshold, kCompRatio, kCompAttack, kCompRelease, kCompKnee, kCompMakeup, kCompMix };
    const char* names[] = { "Threshold", "Ratio", "Attack", "Release", "Knee", "Makeup", "Mix" };
    for (int i = 0; i < 7; ++i)
    {
      const IRECT cell(knobs.L + kw * i, knobs.T, knobs.L + kw * (i + 1), knobs.T + 92.f);
      g->AttachControl(new Knob(cell.GetPadded(-3.f), ids[i], names[i], i < 2 ? KnobStyle::Steel : KnobStyle::Small), kNoTag, play);
    }
    const IRECT bottom = knobs.GetFromBottom(42.f);
    g->AttachControl(new ToggleChip(bottom.GetFromLeft(86.f).GetCentredInside(80.f, 26.f), kCompOn, "On"), kNoTag, play);
    g->AttachControl(new Knob(bottom.GetReducedFromLeft(92.f).GetFromLeft(60.f).GetVShifted(-8.f).GetPadded(0.f, -2.f, 0.f, 6.f), kCompScHpf, "SC HPF", KnobStyle::Small), kNoTag, play);
    auto* comp = static_cast<Compressor*>(engine.module(ModuleId::Compressor));
    g->AttachControl(new ReductionMeter(bottom.GetFromRight(bottom.W() - 170.f).GetPadded(-6.f, -4.f, 0.f, -6.f),
                                        [comp]() { return comp->gainReductionDb(); }, "Gain Reduction", 24.f), kNoTag, play);
  }

  // Педали: Boost / Overdrive / Distortion / Fuzz
  const IRECT pedalRow = content.GetReducedFromTop(228.f);
  const float pw = (pedalRow.W() - 3 * 20.f) / 4.f;
  struct PedalDef
  {
    const char* name;
    const char* sub;
    PedalStyle style;
    int onParam;
    std::vector<std::pair<int, const char*>> knobs;
    int modeParam;
  };
  const PedalDef pedals[] = {
    { "Pearl Boost", "clean boost", { IColor(255, 214, 207, 203), IColor(255, 244, 240, 237), IColor(255, 120, 110, 108), IColor(255, 70, 60, 62), KnobStyle::Pearl, col::accent, 11 },
      kBoostOn, { { kBoostLevel, "Level" }, { kBoostTight, "Tight" }, { kBoostBright, "Bright" } }, -1 },
    { "Rose Drive", "overdrive", { IColor(255, 160, 74, 122), IColor(255, 206, 112, 160), IColor(255, 245, 210, 228), IColor(255, 252, 238, 244), KnobStyle::Steel, IColor(255, 255, 160, 210), 23 },
      kOdOn, { { kOdDrive, "Drive" }, { kOdTone, "Tone" }, { kOdLevel, "Level" }, { kOdMix, "Mix" } }, kOdMode },
    { "Obsidian", "distortion", { IColor(255, 24, 24, 28), IColor(255, 52, 52, 60), IColor(255, 120, 120, 132), IColor(255, 222, 222, 228), KnobStyle::Black, col::accent, 37 },
      kDistOn, { { kDistDrive, "Drive" }, { kDistTone, "Tone" }, { kDistTight, "Tight" }, { kDistLevel, "Level" } }, kDistMode },
    { "Violet Fuzz", "fuzz", { IColor(255, 92, 76, 140), IColor(255, 140, 120, 196), IColor(255, 220, 210, 250), IColor(255, 244, 240, 255), KnobStyle::Steel, IColor(255, 200, 170, 255), 53 },
      kFuzzOn, { { kFuzzFuzz, "Fuzz" }, { kFuzzTone, "Tone" }, { kFuzzBias, "Bias" }, { kFuzzLevel, "Level" } }, kFuzzMode },
  };

  for (int i = 0; i < 4; ++i)
  {
    const PedalDef& pd = pedals[i];
    const IRECT pr(pedalRow.L + i * (pw + 20.f), pedalRow.T, pedalRow.L + i * (pw + 20.f) + pw, pedalRow.B);
    const PedalStyle st = pd.style;
    const char* name = pd.name;
    const char* sub = pd.sub;
    g->AttachControl(new Canvas(pr, [st, name, sub](IGraphics& gr, const IRECT& r) { drawPedalBody(gr, r, st, name, sub); }), kNoTag, play);
    // Ручки: 2 в ряд
    const IRECT knobArea = pr.GetPadded(-14.f, -16.f, -14.f, 0.f).GetFromTop(200.f);
    const int nk = static_cast<int>(pd.knobs.size());
    const int cols = 2;
    const int rows = (nk + 1) / 2;
    for (int k = 0; k < nk; ++k)
    {
      const int r = k / cols, c = k % cols;
      const float cw = knobArea.W() / cols, ch = knobArea.H() / std::max(rows, 1);
      IRECT cell(knobArea.L + c * cw, knobArea.T + r * ch, knobArea.L + (c + 1) * cw, knobArea.T + (r + 1) * ch);
      if (nk == 3 && k == 2) cell = IRECT(knobArea.MW() - cw * 0.5f, cell.T, knobArea.MW() + cw * 0.5f, cell.B);
      auto* knob = new Knob(cell.GetCentredInside(std::min(cell.W(), 92.f), std::min(cell.H(), 96.f)), pd.knobs[static_cast<size_t>(k)].first,
                            pd.knobs[static_cast<size_t>(k)].second, st.knob, st.led);
      g->AttachControl(knob, kNoTag, play);
    }
    if (pd.modeParam >= 0)
      g->AttachControl(new EnumSelector(IRECT(pr.L + 16.f, knobArea.B + 4.f, pr.R - 16.f, knobArea.B + 30.f), pd.modeParam), kNoTag, play);
    g->AttachControl(new FootSwitch(pr.GetFromBottom(58.f).GetCentredInside(64.f, 54.f).GetVShifted(-2.f), pd.onParam, nullptr, st.led), kNoTag, play);
  }

  showPage(g, 0);
}

} // namespace tslui
