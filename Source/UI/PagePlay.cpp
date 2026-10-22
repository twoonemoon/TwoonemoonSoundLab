// SPDX-License-Identifier: GPL-3.0-or-later
// Страница «Игра»: динамика (Gate, Compressor) и педали перегруза.
#include "UIContext.h"

#include "TwoonemoonSoundLab.h"
#include "Effects/Compressor.h"
#include "Effects/NoiseGate.h"

namespace tslui {

using namespace tsl;

namespace {

struct PedalStyle
{
  IColor body, bodyHi, engrave, label;
  KnobStyle knob;
  IColor led;
  int seed;
};

void drawPedalBody(IGraphics& g, const IRECT& r, const PedalStyle& st, const char* name, const char* sub)
{
  g.PathClear();
  g.PathRoundRect(r.GetTranslated(0.f, 5.f), 14.f);
  g.PathFill(IColor(120, 0, 0, 0));
  g.PathClear();
  g.PathRoundRect(r, 14.f);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.R, r.B, { { st.bodyHi, 0.f }, { st.body, 0.6f }, { st.body, 1.f } }));
  g.PathClear();
  g.PathRoundRect(r.GetPadded(-0.5f), 13.5f);
  g.PathStroke(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B, { { IColor(70, 255, 255, 255), 0.f }, { IColor(90, 0, 0, 0), 1.f } }), 1.f);
  const IRECT nameR = r.GetFromBottom(112.f).GetFromTop(26.f);
  g.DrawText(txt(15.f, IColor(110, 0, 0, 0), font::bold), name, nameR.GetTranslated(0.f, 1.f));
  g.DrawText(txt(15.f, st.label, font::bold), name, nameR);
  drawCapsLabel(g, sub, nameR.GetTranslated(0.f, 18.f), st.engrave, 8.f);
}

} // namespace

void buildPlayPage(UIContext& ctx, const char* page)
{
  IGraphics* g = ctx.g;
  Engine& engine = ctx.plugin->engine();
  const IRECT content = ctx.content;

  const IRECT dynRow = content.GetFromTop(210.f);
  const IRECT gateR = dynRow.GetFromLeft(dynRow.W() * 0.5f - 8.f);
  const IRECT compR = dynRow.GetFromRight(dynRow.W() * 0.5f - 8.f);

  ctx.section(gateR, "Noise Gate", "Подавление шума между нотами", page);
  {
    const IRECT knobs = gateR.GetPadded(-14.f, -54.f, -14.f, -14.f);
    const float kw = knobs.W() / 6.f;
    const int ids[] = { kGateThreshold, kGateAttack, kGateHold, kGateRelease, kGateHysteresis, kGateRange };
    const char* names[] = { "Threshold", "Attack", "Hold", "Release", "Hysteresis", "Range" };
    for (int i = 0; i < 6; ++i)
    {
      const IRECT cell(knobs.L + kw * i, knobs.T, knobs.L + kw * (i + 1), knobs.T + 92.f);
      ctx.add(new Knob(cell.GetPadded(-4.f), ids[i], names[i], i == 0 ? KnobStyle::Steel : KnobStyle::Small), page);
    }
    const IRECT bottom = knobs.GetFromBottom(42.f);
    ctx.add(new ToggleChip(bottom.GetFromLeft(86.f).GetCentredInside(80.f, 26.f), kGateOn, "On"), page);
    ctx.add(new EnumSelector(bottom.GetReducedFromLeft(96.f).GetFromLeft(130.f).GetCentredInside(130.f, 26.f), kGateKey), page);
    auto* gate = static_cast<NoiseGate*>(engine.module(ModuleId::Gate));
    ctx.add(new ReductionMeter(bottom.GetFromRight(bottom.W() - 240.f).GetPadded(-6.f, -4.f, 0.f, -6.f),
                               [gate]() { return gate->gainReductionDb(); }, "Подавление", 60.f),
            page);
  }

  ctx.section(compR, "Compressor", "Выравнивание динамики и sustain", page);
  {
    const IRECT knobs = compR.GetPadded(-14.f, -54.f, -14.f, -14.f);
    const float kw = knobs.W() / 7.f;
    const int ids[] = { kCompThreshold, kCompRatio, kCompAttack, kCompRelease, kCompKnee, kCompMakeup, kCompMix };
    const char* names[] = { "Threshold", "Ratio", "Attack", "Release", "Knee", "Makeup", "Mix" };
    for (int i = 0; i < 7; ++i)
    {
      const IRECT cell(knobs.L + kw * i, knobs.T, knobs.L + kw * (i + 1), knobs.T + 92.f);
      ctx.add(new Knob(cell.GetPadded(-3.f), ids[i], names[i], i < 2 ? KnobStyle::Steel : KnobStyle::Small), page);
    }
    const IRECT bottom = knobs.GetFromBottom(42.f);
    ctx.add(new ToggleChip(bottom.GetFromLeft(86.f).GetCentredInside(80.f, 26.f), kCompOn, "On"), page);
    ctx.add(new Knob(IRECT(bottom.L + 92.f, bottom.T - 14.f, bottom.L + 152.f, bottom.B + 4.f), kCompScHpf, "SC HPF", KnobStyle::Small), page);
    auto* comp = static_cast<Compressor*>(engine.module(ModuleId::Compressor));
    ctx.add(new ReductionMeter(bottom.GetFromRight(bottom.W() - 170.f).GetPadded(-6.f, -4.f, 0.f, -6.f),
                               [comp]() { return comp->gainReductionDb(); }, "Gain Reduction", 24.f),
            page);
  }

  // Педали
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
    { "Rose Drive", "overdrive", { IColor(255, 150, 66, 114), IColor(255, 196, 104, 152), IColor(255, 245, 210, 228), IColor(255, 252, 238, 244), KnobStyle::Steel, IColor(255, 255, 170, 214), 23 },
      kOdOn, { { kOdDrive, "Drive" }, { kOdTone, "Tone" }, { kOdLevel, "Level" }, { kOdMix, "Mix" } }, kOdMode },
    { "Obsidian", "distortion", { IColor(255, 24, 24, 28), IColor(255, 52, 52, 60), IColor(255, 120, 120, 132), IColor(255, 222, 222, 228), KnobStyle::Black, col::accent, 37 },
      kDistOn, { { kDistDrive, "Drive" }, { kDistTone, "Tone" }, { kDistTight, "Tight" }, { kDistLevel, "Level" } }, kDistMode },
    { "Violet Fuzz", "fuzz", { IColor(255, 84, 70, 132), IColor(255, 130, 112, 188), IColor(255, 220, 210, 250), IColor(255, 244, 240, 255), KnobStyle::Steel, IColor(255, 205, 180, 255), 53 },
      kFuzzOn, { { kFuzzFuzz, "Fuzz" }, { kFuzzTone, "Tone" }, { kFuzzBias, "Bias" }, { kFuzzLevel, "Level" } }, kFuzzMode },
  };

  for (int i = 0; i < 4; ++i)
  {
    const PedalDef& pd = pedals[i];
    const IRECT pr(pedalRow.L + i * (pw + 20.f), pedalRow.T, pedalRow.L + i * (pw + 20.f) + pw, pedalRow.B);
    const PedalStyle st = pd.style;
    const char* name = pd.name;
    const char* sub = pd.sub;
    ctx.add(new Canvas(pr, [st, name, sub](IGraphics& gr, const IRECT& r) { drawPedalBody(gr, r, st, name, sub); }), page);
    const IRECT knobArea = pr.GetPadded(-14.f, -14.f, -14.f, 0.f).GetFromTop(188.f);
    const int nk = static_cast<int>(pd.knobs.size());
    const int rows = (nk + 1) / 2;
    for (int k = 0; k < nk; ++k)
    {
      const int r = k / 2, c = k % 2;
      const float cw = knobArea.W() / 2, ch = knobArea.H() / std::max(rows, 1);
      IRECT cell(knobArea.L + c * cw, knobArea.T + r * ch, knobArea.L + (c + 1) * cw, knobArea.T + (r + 1) * ch);
      if (nk == 3 && k == 2) cell = IRECT(knobArea.MW() - cw * 0.5f, cell.T, knobArea.MW() + cw * 0.5f, cell.B);
      auto* knob = new Knob(cell.GetCentredInside(std::min(cell.W(), 88.f), std::min(cell.H(), 92.f)), pd.knobs[static_cast<size_t>(k)].first,
                            pd.knobs[static_cast<size_t>(k)].second, st.knob, st.led);
      knob->SetLabelColor(st.engrave);
      ctx.add(knob, page);
    }
    if (pd.modeParam >= 0)
      ctx.add(new EnumSelector(IRECT(pr.L + 16.f, knobArea.B + 2.f, pr.R - 16.f, knobArea.B + 28.f), pd.modeParam), page);
    ctx.add(new FootSwitch(pr.GetFromBottom(60.f).GetCentredInside(64.f, 54.f).GetVShifted(-4.f), pd.onParam, nullptr, st.led), page);
  }
}

} // namespace tslui
