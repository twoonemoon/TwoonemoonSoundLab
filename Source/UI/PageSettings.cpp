// SPDX-License-Identifier: GPL-3.0-or-later
// Страница «Настройки»: аудио, обработка и темп, MIDI-привязки, интерфейс, о программе.
#include "UIContext.h"

#include "TwoonemoonSoundLab.h"
#include "config.h"

#include <chrono>
#include <deque>

namespace tslui {

using namespace tsl;

namespace {

/** Tap tempo: среднее по последним ударам (сброс после паузы > 2 с). */
class TapTempoButton : public ActionButton
{
public:
  TapTempoButton(const IRECT& r, UIContext* ctx) : ActionButton(r, "Tap Tempo", nullptr, false), mCtx(ctx) {}
  void OnMouseDown(float, float, const IMouseMod&) override
  {
    const auto now = std::chrono::steady_clock::now();
    if (!mTaps.empty() && now - mTaps.back() > std::chrono::seconds(2)) mTaps.clear();
    mTaps.push_back(now);
    if (mTaps.size() > 6) mTaps.pop_front();
    if (mTaps.size() >= 2)
    {
      const double sec = std::chrono::duration<double>(mTaps.back() - mTaps.front()).count() / (mTaps.size() - 1);
      const double bpm = std::clamp(60.0 / sec, 30.0, 300.0);
      // Явное действие пользователя: ручной темп важнее темпа хоста
      mCtx->setParam(kHostTempo, 0.0);
      mCtx->setParam(kTempo, std::round(bpm * 10.0) / 10.0);
    }
    SetHighlighted(true);
    SetDirty(false);
  }
  bool IsDirty() override
  {
    if (!mTaps.empty() && std::chrono::steady_clock::now() - mTaps.back() > std::chrono::milliseconds(150)) SetHighlighted(false);
    return IControl::IsDirty();
  }

private:
  UIContext* mCtx;
  std::deque<std::chrono::steady_clock::time_point> mTaps;
};

/** Список MIDI-привязок: параметр, CC, канал, диапазон, инверсия, удаление. */
class MidiMapList : public IControl
{
public:
  MidiMapList(const IRECT& r, TwoonemoonSoundLab* plugin) : IControl(r), mPlugin(plugin) {}
  bool IsDirty() override { return mPlugin->midiMap().version() != mVersion || mPlugin->midiMap().learnTarget() != mLearn || IControl::IsDirty(); }
  void Draw(IGraphics& g) override
  {
    auto& mm = mPlugin->midiMap();
    mVersion = mm.version();
    mLearn = mm.learnTarget();
    g.PathClear();
    g.PathRoundRect(mRECT, 10.f);
    g.PathFill(IColor(255, 12, 13, 16));
    mRows.clear();
    for (int i = 0; i < MidiMap::kSlots; ++i)
      if (mm.slot(i).cc >= 0) mRows.push_back(mm.slot(i));
    if (mLearn >= 0)
    {
      char buf[160];
      snprintf(buf, sizeof(buf), "Ожидание MIDI CC для «%s»… Пошевелите контроллер или педаль.", paramSpec(mLearn).name);
      g.DrawText(txt(12.f, col::warn, font::medium, EAlign::Near), buf, IRECT(mRECT.L + 12.f, mRECT.T + 6.f, mRECT.R, mRECT.T + 26.f));
    }
    if (mRows.empty() && mLearn < 0)
    {
      g.DrawMultiLineText(txt(12.f, col::text3, font::regular, EAlign::Near, EVAlign::Top),
                          "Привязок нет. Правый клик по любой ручке → «MIDI Learn», затем пошевелите контроллер.", mRECT.GetPadded(-12.f));
      return;
    }
    for (size_t i = 0; i < mRows.size() && i < 8; ++i)
    {
      const IRECT r = row(static_cast<int>(i));
      const auto& m = mRows[i];
      char buf[64];
      g.DrawText(txt(12.f, col::text, font::medium, EAlign::Near), paramSpec(m.param).name, IRECT(r.L + 10.f, r.T, r.L + 220.f, r.B));
      snprintf(buf, sizeof(buf), "CC %d%s", m.cc, m.channel >= 0 ? (" · кан. " + std::to_string(m.channel + 1)).c_str() : "");
      g.DrawText(txt(11.f, col::text2, font::mono, EAlign::Near), buf, IRECT(r.L + 230.f, r.T, r.L + 330.f, r.B));
      snprintf(buf, sizeof(buf), "%.0f%% → %.0f%%", m.lo * 100.f, m.hi * 100.f);
      g.DrawText(txt(11.f, col::text2, font::mono, EAlign::Near), buf, IRECT(r.L + 340.f, r.T, r.L + 440.f, r.B));
      chip(g, invRect(r), "Инверсия", m.lo > m.hi);
      chip(g, delRect(r), "Удалить", false);
    }
  }
  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    auto& mm = mPlugin->midiMap();
    for (size_t i = 0; i < mRows.size() && i < 8; ++i)
    {
      const IRECT r = row(static_cast<int>(i));
      const auto& m = mRows[i];
      if (invRect(r).Contains(x, y)) mm.setRange(m.param, m.hi, m.lo);
      else if (delRect(r).Contains(x, y)) mm.removeForParam(m.param);
    }
    SetDirty(false);
  }

private:
  IRECT row(int i) const { return IRECT(mRECT.L + 4.f, mRECT.T + 30.f + i * 30.f, mRECT.R - 4.f, mRECT.T + 58.f + i * 30.f); }
  static IRECT invRect(const IRECT& r) { return IRECT(r.R - 190.f, r.T + 3.f, r.R - 100.f, r.B - 3.f); }
  static IRECT delRect(const IRECT& r) { return IRECT(r.R - 92.f, r.T + 3.f, r.R - 8.f, r.B - 3.f); }
  static void chip(IGraphics& g, const IRECT& r, const char* label, bool on)
  {
    g.PathClear();
    g.PathRoundRect(r, r.H() * 0.5f);
    g.PathFill(on ? IColor(60, 196, 92, 166) : col::bg3);
    g.DrawText(txt(10.5f, on ? col::text : col::text2, font::medium), label, r);
  }
  TwoonemoonSoundLab* mPlugin;
  std::vector<MidiMap::Mapping> mRows;
  uint32_t mVersion = 0xFFFFFFFF;
  int mLearn = -2;
};

/** Переключатель пользовательской настройки (не параметр плагина). */
class PrefToggle : public IControl
{
public:
  PrefToggle(const IRECT& r, const char* label, std::function<bool()> get, std::function<void(bool)> set)
  : IControl(r), mLabel(label), mGet(std::move(get)), mSet(std::move(set))
  {
  }
  void Draw(IGraphics& g) override
  {
    const bool on = mGet();
    const float h = mRECT.H();
    const IRECT sw(mRECT.L, mRECT.T, mRECT.L + h * 1.8f, mRECT.B);
    g.PathClear();
    g.PathRoundRect(sw, h * 0.5f);
    g.PathFill(on ? col::accent : col::bg3);
    g.FillCircle(col::pearl, on ? sw.R - h * 0.5f : sw.L + h * 0.5f, sw.MH(), h * 0.38f);
    g.DrawText(txt(12.f, col::text, font::regular, EAlign::Near), mLabel.c_str(), mRECT.GetReducedFromLeft(h * 1.8f + 10.f));
  }
  void OnMouseDown(float, float, const IMouseMod&) override
  {
    mSet(!mGet());
    SetDirty(false);
  }

private:
  std::string mLabel;
  std::function<bool()> mGet;
  std::function<void(bool)> mSet;
};

/** Текущие аудиопараметры движка. */
class AudioStatus : public IControl
{
public:
  AudioStatus(const IRECT& r, TwoonemoonSoundLab* plugin) : IControl(r), mPlugin(plugin) { mIgnoreMouse = true; }
  bool IsDirty() override { return mPlugin->engine().sampleRate() != mSr || IControl::IsDirty(); }
  void Draw(IGraphics& g) override
  {
    mSr = mPlugin->engine().sampleRate();
    char buf[160];
    if (mPlugin->engine().isPrepared())
      snprintf(buf, sizeof(buf), "Частота: %.1f кГц · блок: %d сэмплов · задержка плагина: %d сэмплов", mSr / 1000.0, mPlugin->GetBlockSize(),
               mPlugin->engine().latencySamples());
    else
      snprintf(buf, sizeof(buf), "Аудио не запущено");
    g.DrawText(txt(12.f, col::text2, font::mono, EAlign::Near), buf, mRECT);
  }

private:
  TwoonemoonSoundLab* mPlugin;
  double mSr = -1.0;
};

} // namespace

void buildSettingsPage(UIContext& ctx, const char* page)
{
  const IRECT content = ctx.content;
  TwoonemoonSoundLab* plugin = ctx.plugin;
  const float colW = (content.W() - 16.f) / 2.f;
  const IRECT left = content.GetFromLeft(colW), right = content.GetFromRight(colW);

  // ---- Аудио
  const IRECT audio = left.GetFromTop(150.f);
  ctx.section(audio, "Аудио", plugin->isStandalone() ? "Устройство, частота дискретизации и буфер" : "Аудиоустройством управляет DAW", page);
  if (plugin->isStandalone())
    ctx.add(new ActionButton(IRECT(audio.L + 18.f, audio.T + 62.f, audio.L + 290.f, audio.T + 92.f), "Аудиоустройство и буфер…",
                             [plugin]() { plugin->openAudioSettings(); }, false),
            page);
  ctx.add(new AudioStatus(IRECT(audio.L + 18.f, audio.T + 104.f, audio.R - 10.f, audio.T + 124.f), plugin), page);

  // ---- Обработка и темп
  const IRECT proc = IRECT(left.L, audio.B + 14.f, left.R, audio.B + 214.f);
  ctx.section(proc, "Обработка и темп", "Качество усилителя, темп для синхронизации, эталон тюнера", page);
  ctx.add(new EnumSelector(IRECT(proc.L + 18.f, proc.T + 64.f, proc.L + 218.f, proc.T + 108.f), kOversampling, "Oversampling"), page);
  ctx.add(new Knob(IRECT(proc.L + 234.f, proc.T + 52.f, proc.L + 314.f, proc.T + 140.f), kTempo, "Tempo", KnobStyle::Small), page);
  ctx.add(new TapTempoButton(IRECT(proc.L + 330.f, proc.T + 66.f, proc.L + 440.f, proc.T + 94.f), &ctx), page);
  ctx.add(new ToggleChip(IRECT(proc.L + 330.f, proc.T + 104.f, proc.L + 470.f, proc.T + 128.f), kHostTempo, "Темп хоста"), page);
  ctx.add(new Knob(IRECT(proc.R - 96.f, proc.T + 52.f, proc.R - 16.f, proc.T + 140.f), kTunerRef, "A4 Ref", KnobStyle::Small), page);
  ctx.add(new ToggleChip(IRECT(proc.L + 18.f, proc.T + 150.f, proc.L + 150.f, proc.T + 174.f), kMetroOn, "Метроном"), page);
  ctx.add(new ToggleChip(IRECT(proc.L + 160.f, proc.T + 150.f, proc.L + 290.f, proc.T + 174.f), kMetroAccent, "Акцент"), page);
  ctx.add(new Knob(IRECT(proc.L + 300.f, proc.T + 132.f, proc.L + 360.f, proc.T + 196.f), kMetroLevel, "Level", KnobStyle::Small), page);
  ctx.add(new Label(IRECT(proc.L + 370.f, proc.T + 146.f, proc.R - 18.f, proc.T + 180.f), "Клик 4/4 по темпу выше (или хоста)",
                    txt(10.5f, col::text3, font::regular, EAlign::Near)),
          page);

  // ---- Интерфейс
  const IRECT ui = IRECT(left.L, proc.B + 14.f, left.R, content.B);
  ctx.section(ui, "Интерфейс", "Подсказки, анимации, знакомство", page);
  ctx.add(new PrefToggle(IRECT(ui.L + 18.f, ui.T + 60.f, ui.R - 18.f, ui.T + 82.f), "Всплывающие подсказки у регуляторов",
                         [&ctx]() { return ctx.prefs.tooltips; },
                         [&ctx](bool v) {
                           ctx.prefs.tooltips = v;
                           ctx.prefs.save();
                           ctx.g->EnableTooltips(v);
                         }),
          page);
  ctx.add(new PrefToggle(IRECT(ui.L + 18.f, ui.T + 92.f, ui.R - 18.f, ui.T + 114.f), "Уменьшить анимации",
                         [&ctx]() { return ctx.prefs.reduceMotion; },
                         [&ctx](bool v) {
                           ctx.prefs.reduceMotion = v;
                           ctx.prefs.save();
                           uiFlags().reduceMotion = v;
                         }),
          page);
  ctx.add(new ActionButton(IRECT(ui.L + 18.f, ui.T + 126.f, ui.L + 260.f, ui.T + 154.f), "Показать знакомство снова", [&ctx]() {
            if (ctx.showOnboarding) ctx.showOnboarding();
          }, false),
          page);

  // ---- MIDI
  const IRECT midi = right.GetFromTop(right.H() - 150.f);
  ctx.section(midi, "MIDI Learn и педаль экспрессии", "Правый клик по ручке → MIDI Learn. Привязки сохраняются в проекте.", page);
  ctx.add(new MidiMapList(IRECT(midi.L + 14.f, midi.T + 60.f, midi.R - 14.f, midi.B - 56.f), plugin), page);
  ctx.add(new ActionButton(IRECT(midi.L + 14.f, midi.B - 46.f, midi.L + 330.f, midi.B - 16.f), "Педаль экспрессии (CC 11) → Wah Position",
                           [plugin]() { plugin->midiMap().assign(kWahPosition, 11, -1, 0.f, 1.f); }, false),
          page);
  ctx.add(new ActionButton(IRECT(midi.R - 170.f, midi.B - 46.f, midi.R - 14.f, midi.B - 16.f), "Очистить все", [plugin]() { plugin->midiMap().clear(); }, false), page);

  // ---- О программе
  const IRECT about = right.GetFromBottom(136.f);
  ctx.add(new Canvas(about, [](IGraphics& g, const IRECT& r) {
            drawHardwarePanel(g, r, 12.f);
            g.DrawText(txt(14.f, col::text, font::semibold, EAlign::Near), "TwoonemoonSoundLab " PLUG_VERSION_STR, IRECT(r.L + 18.f, r.T + 14.f, r.R, r.T + 34.f));
            g.DrawMultiLineText(txt(11.f, col::text3, font::regular, EAlign::Near, EVAlign::Top),
                                "Свободное ПО под лицензией GPL-3.0-or-later. Без телеметрии и сети. Модели усилителей, кабинетов и эффектов — собственные алгоритмы, "
                                "не копии существующих устройств. Шрифты Inter и JetBrains Mono (SIL OFL). Подробности — THIRD_PARTY_NOTICES.md.",
                                IRECT(r.L + 18.f, r.T + 40.f, r.R - 18.f, r.B - 10.f));
          }),
          page);
}

} // namespace tslui
