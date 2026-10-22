// SPDX-License-Identifier: GPL-3.0-or-later
// Страница «Усилитель»: выбор модели, индивидуальный фасад, основные и расширенные регуляторы.
#include "AmpFaces.h"
#include "UIContext.h"

#include "TwoonemoonSoundLab.h"
#include "Amps/Amp.h"

namespace tslui {

using namespace tsl;

namespace {

/** Голова усилителя; лампа и подсветка решётки сдержанно реагируют на сигнал оконечного каскада. */
class AmpHeadView : public IControl
{
public:
  AmpHeadView(const IRECT& r, TwoonemoonSoundLab* plugin, float panelFrac)
  : IControl(r), mPlugin(plugin), mPanelFrac(panelFrac)
  {
    mIgnoreMouse = true;
  }
  bool IsDirty() override
  {
    auto* amp = static_cast<Amp*>(mPlugin->engine().module(ModuleId::Amp));
    const float target = std::min(1.f, amp ? amp->powerDrive() * 0.6f : 0.f);
    mGlowTarget = target;
    if (std::fabs(mGlow - mGlowTarget) > 0.01f) return true;
    return IControl::IsDirty();
  }
  void Draw(IGraphics& g) override
  {
    mGlow += (mGlowTarget - mGlow) * 0.25f;
    const int model = mPlugin->GetParam(kAmpModel)->Int();
    drawAmpHead(g, mRECT, model, mPanelFrac, mGlow);
  }

private:
  TwoonemoonSoundLab* mPlugin;
  float mPanelFrac;
  float mGlow = 0.f, mGlowTarget = 0.f;
};

const char* categoryName(AmpCategory c)
{
  switch (c)
  {
    case AmpCategory::Clean: return "Clean";
    case AmpCategory::Crunch: return "Crunch";
    case AmpCategory::HighGain: return "High Gain";
    case AmpCategory::Acoustic: return "Acoustic";
  }
  return "";
}

} // namespace

void buildAmpPage(UIContext& ctx, const char* page)
{
  const IRECT content = ctx.content;
  TwoonemoonSoundLab* plugin = ctx.plugin;
  const char* elecGroup = "amp.electric";
  const char* acGroup = "amp.acoustic";

  // ---- Выбор модели
  const IRECT top = content.GetFromTop(44.f);
  std::vector<ActionButton*> catButtons;
  const AmpCategory cats[] = { AmpCategory::Clean, AmpCategory::Crunch, AmpCategory::HighGain, AmpCategory::Acoustic };
  for (int i = 0; i < 4; ++i)
  {
    const AmpCategory cat = cats[i];
    const IRECT r = IRECT(top.L + i * 112.f, top.T + 4.f, top.L + i * 112.f + 104.f, top.B - 6.f);
    auto* b = new ActionButton(r, categoryName(cat), [plugin, cat, &ctx]() {
      for (int m = 0; m < kNumAmpModels; ++m)
        if (ampVoice(m).category == cat)
        {
          if (IControl* c = ctx.g->GetControlWithParamIdx(kAmpModel))
          {
            c->SetValue(plugin->GetParam(kAmpModel)->ToNormalized(m));
            c->SetDirty(true);
          }
          break;
        }
    });
    ctx.add(b, page);
    catButtons.push_back(b);
  }
  auto* modelSel = new EnumSelector(IRECT(top.L + 470.f, top.T + 4.f, top.L + 800.f, top.B - 6.f), kAmpModel);
  ctx.add(modelSel, page);
  auto* desc = new Label(IRECT(top.L + 820.f, top.T, top.R, top.B), "", txt(12.f, col::text2, font::regular, EAlign::Near));
  ctx.add(desc, page);

  // ---- Голова усилителя
  const float panelFrac = 0.52f;
  const IRECT head = IRECT(content.L + 10.f, top.B + 30.f, content.R - 10.f, top.B + 30.f + 340.f);
  ctx.add(new AmpHeadView(head, plugin, panelFrac), page);
  const IRECT inner = head.GetPadded(-16.f);
  const IRECT panel = inner.GetFromBottom(inner.H() * (1.f - panelFrac));
  const IRECT knobRow = panel.GetPadded(-30.f, -12.f, -90.f, -8.f);

  std::vector<Knob*> elecKnobs, acKnobs;
  {
    const int ids[] = { kAmpGain, kAmpBass, kAmpMid, kAmpTreble, kAmpPresence, kAmpDepth, kAmpMaster, kAmpLevel };
    const char* names[] = { "Gain", "Bass", "Middle", "Treble", "Presence", "Depth", "Master", "Volume" };
    const float w = knobRow.W() / 8.f;
    for (int i = 0; i < 8; ++i)
    {
      auto* k = new Knob(IRECT(knobRow.L + w * i, knobRow.T, knobRow.L + w * (i + 1), knobRow.B).GetCentredInside(w, std::min(knobRow.H(), 118.f)),
                         ids[i], names[i], KnobStyle::Steel, col::accent, ids[i] == kAmpLevel);
      ctx.g->AttachControl(k, kNoTag, elecGroup);
      elecKnobs.push_back(k);
    }
  }
  {
    const int ids[] = { kAmpGain, kAmpBass, kAmpMid, kAmpTreble, kAcBody, kAcPiezoTame, kAcAir, kAcRoom, kAmpLevel };
    const char* names[] = { "Gain", "Bass", "Middle", "Treble", "Body", "Piezo Tame", "Air", "Room", "Volume" };
    const float w = knobRow.W() / 9.f;
    for (int i = 0; i < 9; ++i)
    {
      auto* k = new Knob(IRECT(knobRow.L + w * i, knobRow.T, knobRow.L + w * (i + 1), knobRow.B).GetCentredInside(w, std::min(knobRow.H(), 112.f)),
                         ids[i], names[i], KnobStyle::Steel, col::accent, ids[i] == kAmpLevel);
      ctx.g->AttachControl(k, kNoTag, acGroup);
      acKnobs.push_back(k);
    }
  }

  // ---- Расширенные настройки
  const IRECT adv = IRECT(content.L, head.B + 22.f, content.R, content.B);
  ctx.add(new Canvas(adv, [](IGraphics& g, const IRECT& r) {
            drawGlass(g, r, 14.f, 0.9f);
            drawCapsLabel(g, "Advanced", r.GetPadded(-18.f, -10.f, 0.f, 0.f).GetFromTop(14.f), col::text3, 9.f, EAlign::Near);
          }),
          page);
  const IRECT advRow = adv.GetPadded(-18.f, -22.f, -18.f, -6.f);
  const float aw = 92.f;
  std::vector<IControl*> elecAdv;
  {
    const int ids[] = { kAmpSag, kAmpBias, kAmpFeedback };
    const char* names[] = { "Sag", "Bias", "Feedback" };
    for (int i = 0; i < 3; ++i)
    {
      auto* k = new Knob(IRECT(advRow.L + aw * i, advRow.T, advRow.L + aw * (i + 1), advRow.B), ids[i], names[i], KnobStyle::Small);
      ctx.g->AttachControl(k, kNoTag, elecGroup);
      elecAdv.push_back(k);
    }
    auto* bright = new ToggleChip(IRECT(advRow.L + aw * 3 + 8.f, advRow.MH() - 14.f, advRow.L + aw * 3 + 98.f, advRow.MH() + 10.f), kAmpBright, "Bright");
    ctx.g->AttachControl(bright, kNoTag, elecGroup);
  }
  ctx.add(new Knob(IRECT(advRow.L + aw * 4 + 30.f, advRow.T, advRow.L + aw * 5 + 30.f, advRow.B), kAmpLowCut, "Low Cut", KnobStyle::Small), page);
  ctx.add(new Knob(IRECT(advRow.L + aw * 5 + 30.f, advRow.T, advRow.L + aw * 6 + 30.f, advRow.B), kAmpHighCut, "High Cut", KnobStyle::Small), page);
  ctx.add(new EnumSelector(IRECT(advRow.L + aw * 6 + 60.f, advRow.MH() - 22.f, advRow.L + aw * 6 + 250.f, advRow.MH() + 18.f), kOversampling, "Oversampling"), page);
  ctx.add(new ToggleChip(IRECT(advRow.R - 120.f, advRow.MH() - 14.f, advRow.R, advRow.MH() + 10.f), kAmpOn, "Amp On"), page);

  // ---- Реакция на смену модели и переключение страниц
  auto refresh = [&ctx, plugin, elecKnobs, acKnobs, catButtons, desc, page, elecGroup, acGroup](int paramIdx) {
    if (paramIdx != kAmpModel && paramIdx != -1) return;
    const int model = plugin->GetParam(kAmpModel)->Int();
    const AmpVoice& v = ampVoice(model);
    const AmpFaceStyle& st = ampFaceStyle(model);
    const bool acoustic = v.category == AmpCategory::Acoustic;
    const bool visible = ctx.g->GetControlWithParamIdx(kAmpModel) && !ctx.g->GetControlWithParamIdx(kAmpModel)->IsHidden();
    ctx.g->ForControlInGroup(elecGroup, [&](IControl* c) { c->Hide(!visible || acoustic); });
    ctx.g->ForControlInGroup(acGroup, [&](IControl* c) { c->Hide(!visible || !acoustic); });
    for (auto* k : elecKnobs) { k->SetStyle(st.knob); k->SetAccent(st.knobAccent); k->SetLabelColor(st.label); }
    for (auto* k : acKnobs) { k->SetStyle(st.knob); k->SetAccent(st.knobAccent); k->SetLabelColor(st.label); }
    for (size_t i = 0; i < catButtons.size(); ++i) catButtons[i]->SetHighlighted(static_cast<int>(i) == static_cast<int>(v.category));
    desc->SetStr(v.description);
    (void)page;
  };
  ctx.onParam(refresh);
}

} // namespace tslui
