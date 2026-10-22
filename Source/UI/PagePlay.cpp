// SPDX-License-Identifier: GPL-3.0-or-later
// Страница «Игра»: пресет (переключение, A/B, сохранение, undo/redo), быстрые регуляторы
// усилителя, быстрый старт и ряд педалей для игры вживую.
#include "AmpFaces.h"
#include "UIContext.h"

#include "TwoonemoonSoundLab.h"
#include "Amps/AmpVoices.h"

namespace tslui {

using namespace tsl;

namespace {

/** Название текущего пресета с категорией и признаком изменений. */
class PresetNameView : public IControl
{
public:
  PresetNameView(const IRECT& r, TwoonemoonSoundLab* plugin, std::function<void()> onClick)
  : IControl(r), mPlugin(plugin), mClick(std::move(onClick))
  {
  }
  bool IsDirty() override
  {
    const bool mod = mPlugin->presetModified();
    if (mod != mMod || mPlugin->currentPresetName() != mName || mPlugin->abSlot() != mAB) return true;
    return IControl::IsDirty();
  }
  void Draw(IGraphics& g) override
  {
    mMod = mPlugin->presetModified();
    mName = mPlugin->currentPresetName();
    mAB = mPlugin->abSlot();
    std::string cat = "Пресет";
    for (const Preset* p : mPlugin->allPresets())
      if (p->id() == mPlugin->currentPresetId()) cat = p->factory ? presetCategoryTitle(p->category) : "Мой пресет";
    if (mHover)
    {
      g.PathClear();
      g.PathRoundRect(mRECT, 10.f);
      g.PathFill(IColor(18, 255, 255, 255));
    }
    drawCapsLabel(g, cat.c_str(), IRECT(mRECT.L + 14.f, mRECT.T + 6.f, mRECT.R, mRECT.T + 18.f), col::text3, 9.f, EAlign::Near);
    g.DrawText(txt(19.f, col::text, font::semibold, EAlign::Near), mName.c_str(), IRECT(mRECT.L + 14.f, mRECT.T + 18.f, mRECT.R - 90.f, mRECT.B - 4.f));
    if (mMod)
      g.DrawText(txt(11.f, col::warn, font::medium, EAlign::Far), "● изменён", IRECT(mRECT.R - 100.f, mRECT.T + 18.f, mRECT.R - 12.f, mRECT.B - 4.f));
  }
  void OnMouseDown(float, float, const IMouseMod&) override
  {
    if (mClick) mClick();
  }
  void OnMouseOver(float, float, const IMouseMod&) override { mHover = true; SetDirty(false); }
  void OnMouseOut() override { mHover = false; SetDirty(false); }

private:
  TwoonemoonSoundLab* mPlugin;
  std::function<void()> mClick;
  bool mMod = false, mHover = false;
  std::string mName;
  int mAB = -1;
};

/** Кнопка слота A/B. */
class ABButton : public ActionButton
{
public:
  ABButton(const IRECT& r, TwoonemoonSoundLab* plugin, int slot)
  : ActionButton(r, slot == 0 ? "A" : "B", [plugin, slot]() { if (plugin->abSlot() != slot) plugin->switchAB(); }, false), mPlugin(plugin), mSlot(slot)
  {
  }
  bool IsDirty() override { return (mPlugin->abSlot() == mSlot) != mShown || IControl::IsDirty(); }
  void Draw(IGraphics& g) override
  {
    mShown = mPlugin->abSlot() == mSlot;
    SetHighlighted(mShown);
    ActionButton::Draw(g);
  }

private:
  TwoonemoonSoundLab* mPlugin;
  int mSlot;
  bool mShown = false;
};

/** «Сохранить»: перезаписать пользовательский пресет или спросить имя для заводского. */
class SaveButton : public ActionButton
{
public:
  SaveButton(const IRECT& r, TwoonemoonSoundLab* plugin) : ActionButton(r, "Сохранить", nullptr, false), mPlugin(plugin) {}
  void OnMouseDown(float, float, const IMouseMod&) override
  {
    const std::string id = mPlugin->currentPresetId();
    if (id.rfind("user:", 0) == 0)
    {
      Preset p = mPlugin->snapshot();
      p.category = "user";
      std::string err;
      if (mPlugin->library().save(p, true, err)) mPlugin->markSaved(p);
      else GetUI()->ShowMessageBox(err.c_str(), "TwoonemoonSoundLab", kMB_OK);
      return;
    }
    // Заводской пресет — сохраняем как новый пользовательский
    GetUI()->CreateTextEntry(*this, txt(12.f, col::text, font::regular), mRECT.GetHPadded(60.f), (mPlugin->currentPresetName() + " (мой)").c_str());
  }
  void OnTextEntryCompletion(const char* str, int) override
  {
    if (!str || !*str) return;
    Preset p = mPlugin->snapshot();
    p.name = str;
    p.category = "user";
    std::string err;
    if (!mPlugin->library().save(p, false, err))
    {
      GetUI()->ShowMessageBox(err.c_str(), "TwoonemoonSoundLab", kMB_OK);
      return;
    }
    p.name = PresetLibrary::sanitizeFileName(str);
    p.factory = false;
    mPlugin->markSaved(p);
  }

private:
  TwoonemoonSoundLab* mPlugin;
};

/** Панель быстрых регуляторов усилителя в стиле текущей модели. */
class AmpQuickPanel : public IControl
{
public:
  AmpQuickPanel(const IRECT& r, TwoonemoonSoundLab* plugin) : IControl(r), mPlugin(plugin) { mIgnoreMouse = true; }
  bool IsDirty() override { return mPlugin->GetParam(kAmpModel)->Int() != mModel || IControl::IsDirty(); }
  void Draw(IGraphics& g) override
  {
    mModel = mPlugin->GetParam(kAmpModel)->Int();
    const AmpFaceStyle& st = ampFaceStyle(mModel);
    g.PathClear();
    g.PathRoundRect(mRECT.GetTranslated(0.f, 4.f), 14.f);
    g.PathFill(IColor(110, 0, 0, 0));
    g.PathClear();
    g.PathRoundRect(mRECT, 14.f);
    g.PathFill(IPattern::CreateLinearGradient(mRECT.L, mRECT.T, mRECT.L, mRECT.B, { { st.tolexHi, 0.f }, { st.tolex, 1.f } }));
    const IRECT panel = mRECT.GetPadded(-10.f, -54.f, -10.f, -10.f);
    drawBrushedRect(g, panel, 9.f, st.panel, st.panelHi, st.seed);
    const bool light = (st.tolex.R + st.tolex.G + st.tolex.B) > 380;
    g.DrawText(txt(12.f, light ? col::textDark : col::text2, font::regular, EAlign::Near), ampVoice(mModel).description,
               IRECT(mRECT.L + 360.f, mRECT.T + 12.f, mRECT.R - 16.f, mRECT.T + 44.f));
  }

private:
  TwoonemoonSoundLab* mPlugin;
  int mModel = -1;
};

struct MiniPedalDef
{
  ModuleId id;
  int knobParam;
  const char* knobLabel;
};

} // namespace

void buildPlayPage(UIContext& ctx, const char* page)
{
  const IRECT content = ctx.content;
  TwoonemoonSoundLab* plugin = ctx.plugin;

  // ---- Полоса пресета
  const IRECT strip = content.GetFromTop(60.f);
  ctx.add(new Canvas(strip, [](IGraphics& g, const IRECT& r) { drawGlass(g, r, 16.f, 1.f); }), page);
  const float sy = strip.MH();
  ctx.add(new IconButton(IRECT(strip.L + 10.f, sy - 17.f, strip.L + 46.f, sy + 17.f), IconButton::Icon::Left, [plugin]() { plugin->stepPreset(-1); }), page);
  ctx.add(new PresetNameView(IRECT(strip.L + 52.f, strip.T + 4.f, strip.L + 470.f, strip.B - 4.f), plugin, [&ctx]() {
            if (ctx.gotoPage) ctx.gotoPage(-1);
          }),
          page);
  ctx.add(new IconButton(IRECT(strip.L + 476.f, sy - 17.f, strip.L + 512.f, sy + 17.f), IconButton::Icon::Right, [plugin]() { plugin->stepPreset(1); }), page);
  float x = strip.L + 540.f;
  ctx.add(new ABButton(IRECT(x, sy - 15.f, x + 40.f, sy + 15.f), plugin, 0), page);
  ctx.add(new ABButton(IRECT(x + 46.f, sy - 15.f, x + 86.f, sy + 15.f), plugin, 1), page);
  ctx.add(new ActionButton(IRECT(x + 92.f, sy - 15.f, x + 210.f, sy + 15.f), "Копия в другой", [plugin]() { plugin->copyToOtherAB(); }, false), page);
  x += 236.f;
  ctx.add(new SaveButton(IRECT(x, sy - 15.f, x + 110.f, sy + 15.f), plugin), page);
  ctx.add(new ActionButton(IRECT(x + 116.f, sy - 15.f, x + 226.f, sy + 15.f), "Все пресеты", [&ctx]() { if (ctx.gotoPage) ctx.gotoPage(-1); }, false), page);
  ctx.add(new IconButton(IRECT(strip.R - 96.f, sy - 15.f, strip.R - 54.f, sy + 15.f), IconButton::Icon::Undo, [plugin]() { plugin->undo(); },
                         [plugin]() { return plugin->canUndo(); }), page);
  ctx.add(new IconButton(IRECT(strip.R - 48.f, sy - 15.f, strip.R - 6.f, sy + 15.f), IconButton::Icon::Redo, [plugin]() { plugin->redo(); },
                         [plugin]() { return plugin->canRedo(); }), page);

  // ---- Усилитель: быстрые регуляторы
  const IRECT ampR(content.L, strip.B + 14.f, content.L + 770.f, strip.B + 214.f);
  ctx.add(new AmpQuickPanel(ampR, plugin), page);
  ctx.add(new EnumSelector(IRECT(ampR.L + 14.f, ampR.T + 10.f, ampR.L + 340.f, ampR.T + 42.f), kAmpModel), page);
  {
    const IRECT knobs = ampR.GetPadded(-24.f, -60.f, -24.f, -14.f);
    const int ids[6] = { kAmpGain, kAmpBass, kAmpMid, kAmpTreble, kAmpMaster, kAmpLevel };
    const char* names[6] = { "Gain", "Bass", "Middle", "Treble", "Master", "Volume" };
    const float w = knobs.W() / 6.f;
    std::vector<Knob*> ks;
    for (int i = 0; i < 6; ++i)
    {
      auto* k = new Knob(IRECT(knobs.L + w * i, knobs.T, knobs.L + w * (i + 1), knobs.B), ids[i], names[i], KnobStyle::Steel, col::accent, ids[i] == kAmpLevel);
      ctx.add(k, page);
      ks.push_back(k);
    }
    ctx.onParam([plugin, ks](int idx) {
      if (idx != kAmpModel && idx != -1 && idx != -2) return;
      const AmpFaceStyle& st = ampFaceStyle(plugin->GetParam(kAmpModel)->Int());
      for (auto* k : ks) { k->SetStyle(st.knob); k->SetAccent(st.knobAccent); k->SetLabelColor(st.label); }
    });
  }

  // ---- Быстрый старт
  const IRECT quick(ampR.R + 14.f, ampR.T, content.R, ampR.B);
  ctx.add(new Canvas(quick, [](IGraphics& g, const IRECT& r) {
            drawHardwarePanel(g, r, 14.f);
            g.DrawText(txt(14.f, col::text, font::semibold, EAlign::Near), "Быстрый старт", IRECT(r.L + 16.f, r.T + 10.f, r.R, r.T + 30.f));
            g.DrawMultiLineText(txt(11.f, col::text3, font::regular, EAlign::Near, EVAlign::Top),
                                "Гитара — во вход 1 аудиоинтерфейса. Сыграйте громко: индикатор In внизу слева не должен краснеть.",
                                IRECT(r.L + 16.f, r.T + 34.f, r.R - 14.f, r.T + 70.f));
          }),
          page);
  struct Quick { const char* label; const char* preset; };
  const Quick qs[4] = { { "Clean", "Glass Morning" }, { "Heavy", "Iron Veil Chug" }, { "Ambient", "Misty Valley" }, { "Acoustic", "Silver Stage" } };
  for (int i = 0; i < 4; ++i)
  {
    const IRECT b(quick.L + 14.f + (i % 2) * ((quick.W() - 36.f) / 2.f + 8.f), quick.T + 80.f + (i / 2) * 50.f,
                  quick.L + 14.f + (i % 2) * ((quick.W() - 36.f) / 2.f + 8.f) + (quick.W() - 36.f) / 2.f, quick.T + 120.f + (i / 2) * 50.f);
    const std::string presetName = qs[i].preset;
    ctx.add(new ActionButton(b, qs[i].label, [plugin, presetName]() {
              for (const auto& p : factoryPresets())
                if (p.name == presetName) plugin->loadPreset(p);
            }),
            page);
  }

  // ---- Ряд педалей
  const IRECT row(content.L, ampR.B + 16.f, content.R, content.B);
  const MiniPedalDef pedals[10] = {
    { ModuleId::Gate, kGateThreshold, "Threshold" }, { ModuleId::Compressor, kCompThreshold, "Threshold" }, { ModuleId::Wah, kWahPosition, "Position" },
    { ModuleId::Overdrive, kOdDrive, "Drive" },      { ModuleId::Distortion, kDistDrive, "Drive" },         { ModuleId::Fuzz, kFuzzFuzz, "Fuzz" },
    { ModuleId::Chorus, kChoDepth, "Depth" },        { ModuleId::Phaser, kPhaRate, "Rate" },                 { ModuleId::Delay, kDlyMix, "Mix" },
    { ModuleId::Reverb, kRvbMix, "Mix" },
  };
  const float pw = (row.W() - 9 * 10.f) / 10.f;
  for (int i = 0; i < 10; ++i)
  {
    const MiniPedalDef& d = pedals[i];
    const IRECT pr(row.L + i * (pw + 10.f), row.T, row.L + i * (pw + 10.f) + pw, row.B);
    const IColor c = moduleColor(d.id);
    const char* name = moduleDisplayName(d.id);
    ctx.add(new Canvas(pr, [c, name](IGraphics& g, const IRECT& r) {
              g.PathClear();
              g.PathRoundRect(r.GetTranslated(0.f, 4.f), 12.f);
              g.PathFill(IColor(110, 0, 0, 0));
              g.PathClear();
              g.PathRoundRect(r, 12.f);
              g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B, { { IColor(255, 40, 42, 48), 0.f }, { IColor(255, 22, 23, 27), 1.f } }));
              g.PathClear();
              g.PathRoundRect(IRECT(r.L, r.T, r.R, r.T + 5.f), 2.5f);
              g.PathFill(c);
              g.PathClear();
              g.PathRoundRect(r.GetPadded(-0.5f), 12.f);
              g.PathStroke(IColor(60, 255, 255, 255), 1.f);
              g.DrawText(txt(12.f, col::text, font::semibold), name, IRECT(r.L, r.T + 12.f, r.R, r.T + 30.f));
            }),
            page);
    ctx.add(new Knob(IRECT(pr.L + 6.f, pr.T + 36.f, pr.R - 6.f, pr.T + 150.f).GetCentredInside(std::min(pw - 8.f, 96.f), 110.f), d.knobParam, d.knobLabel,
                     KnobStyle::Small, c),
            page);
    ctx.add(new FootSwitch(IRECT(pr.L + 8.f, pr.B - 92.f, pr.R - 8.f, pr.B - 12.f), moduleEnableParam(d.id), "On", c), page);
  }
}

} // namespace tslui
