// SPDX-License-Identifier: GPL-3.0-or-later
// Страница «Эффекты»: список модулей по категориям и детальная панель выбранного модуля.
// Описание контролов — таблица (данные), чтобы UI оставался согласованным с реестром параметров.
#include "UIContext.h"

#include "TwoonemoonSoundLab.h"
#include "Effects/Compressor.h"
#include "Effects/Delay.h"
#include "Effects/NoiseGate.h"

#include <memory>

namespace tslui {

using namespace tsl;

namespace {

enum class CtlKind { Knob, Selector, Toggle, GateMeter, CompMeter, DelayTime };

struct FxCtl
{
  CtlKind kind;
  int param;
  const char* label;
};

struct FxSpec
{
  ModuleId id;
  const char* category;
  const char* title;
  const char* description;
  IColor color;
  std::vector<FxCtl> controls;
};

const std::vector<FxSpec>& fxSpecs()
{
  using K = CtlKind;
  static const std::vector<FxSpec> specs = {
    { ModuleId::Gate, "Динамика", "Noise Gate", "Убирает шум и фон между нотами. Hysteresis и Hold не дают обрывать затухающие ноты; ключ «Input DI» управляет гейтом по чистому сигналу гитары.",
      col::accent, { { K::Selector, kGateKey, "Key" }, { K::Knob, kGateThreshold, "Threshold" }, { K::Knob, kGateAttack, "Attack" }, { K::Knob, kGateHold, "Hold" },
                     { K::Knob, kGateRelease, "Release" }, { K::Knob, kGateHysteresis, "Hysteresis" }, { K::Knob, kGateRange, "Range" }, { K::GateMeter, -1, "Подавление" } } },
    { ModuleId::Compressor, "Динамика", "Compressor", "Выравнивает громкость и добавляет sustain. Mix позволяет параллельную компрессию, SC HPF не даёт басу «качать» компрессор.",
      col::accent, { { K::Knob, kCompThreshold, "Threshold" }, { K::Knob, kCompRatio, "Ratio" }, { K::Knob, kCompAttack, "Attack" }, { K::Knob, kCompRelease, "Release" },
                     { K::Knob, kCompKnee, "Knee" }, { K::Knob, kCompMakeup, "Makeup" }, { K::Knob, kCompMix, "Mix" }, { K::Knob, kCompScHpf, "SC HPF" }, { K::CompMeter, -1, "Gain Reduction" } } },
    { ModuleId::EnvFilter, "Фильтр", "Envelope Filter", "Автовау: сила атаки управляет частотой фильтра. Sensitivity — чувствительность к уровню, Range — диапазон хода фильтра.",
      IColor(255, 120, 200, 170), { { K::Selector, kEnvMode, "Mode" }, { K::Selector, kEnvDirection, "Direction" }, { K::Knob, kEnvSensitivity, "Sensitivity" },
                                    { K::Knob, kEnvRangeLow, "Range Low" }, { K::Knob, kEnvRangeHigh, "Range High" }, { K::Knob, kEnvQ, "Q" }, { K::Knob, kEnvAttack, "Attack" },
                                    { K::Knob, kEnvRelease, "Release" }, { K::Knob, kEnvMix, "Mix" } } },
    { ModuleId::Boost, "Перегруз", "Pearl Boost", "Чистый подъём уровня перед усилителем. Tight убирает лишний низ для плотного high-gain, Bright добавляет или убирает верх.",
      IColor(255, 214, 207, 203), { { K::Knob, kBoostLevel, "Level" }, { K::Knob, kBoostTight, "Tight" }, { K::Knob, kBoostBright, "Bright" } } },
    { ModuleId::Overdrive, "Перегруз", "Rose Drive", "Овердрайв на ОУ с диодами в обратной связи. Transparent — шире полоса и больше низа, Mid Focus — характерный горб середины.",
      IColor(255, 200, 100, 160), { { K::Selector, kOdMode, "Mode" }, { K::Knob, kOdDrive, "Drive" }, { K::Knob, kOdTone, "Tone" }, { K::Knob, kOdLevel, "Level" }, { K::Knob, kOdMix, "Mix" } } },
    { ModuleId::Distortion, "Перегруз", "Obsidian", "Дисторшн с высоким усилением и диодами на землю. Tight срезает низ до клиппинга — так рифы остаются собранными.",
      IColor(255, 160, 160, 172), { { K::Selector, kDistMode, "Mode" }, { K::Knob, kDistDrive, "Drive" }, { K::Knob, kDistTone, "Tone" }, { K::Knob, kDistTight, "Tight" }, { K::Knob, kDistLevel, "Level" } } },
    { ModuleId::Fuzz, "Перегруз", "Violet Fuzz", "Двухтранзисторный фузз. Низкий Bias даёт «голодный» сплаттер, режим Octave добавляет октаву вверх.",
      IColor(255, 160, 140, 230), { { K::Selector, kFuzzMode, "Mode" }, { K::Knob, kFuzzFuzz, "Fuzz" }, { K::Knob, kFuzzTone, "Tone" }, { K::Knob, kFuzzBias, "Bias" }, { K::Knob, kFuzzLevel, "Level" } } },
    { ModuleId::Chorus, "Модуляция", "Chorus", "Две модулированные линии задержки с разнесёнными по стерео фазами. Width — ширина, Tone — яркость эффекта.",
      IColor(255, 130, 170, 240), { { K::Toggle, kChoSync, "Sync" }, { K::Selector, kChoDivision, "Division" }, { K::Knob, kChoRate, "Rate" }, { K::Knob, kChoDepth, "Depth" },
                                    { K::Knob, kChoMix, "Mix" }, { K::Knob, kChoWidth, "Width" }, { K::Knob, kChoTone, "Tone" } } },
    { ModuleId::Phaser, "Модуляция", "Phaser", "Цепочка фазовращателей с обратной связью. Больше ступеней — больше провалов в спектре; Stereo разносит фазы LFO между каналами.",
      IColor(255, 220, 150, 110), { { K::Toggle, kPhaSync, "Sync" }, { K::Selector, kPhaDivision, "Division" }, { K::Selector, kPhaStages, "Stages" }, { K::Knob, kPhaRate, "Rate" },
                                    { K::Knob, kPhaDepth, "Depth" }, { K::Knob, kPhaFeedback, "Feedback" }, { K::Knob, kPhaMix, "Mix" }, { K::Knob, kPhaStereo, "Stereo" } } },
    { ModuleId::Flanger, "Модуляция", "Flanger", "Очень короткая модулированная задержка с обратной связью. Manual задаёт центр «реактивного» свиста, отрицательный Feedback — более полый звук.",
      IColor(255, 110, 200, 220), { { K::Toggle, kFlaSync, "Sync" }, { K::Selector, kFlaDivision, "Division" }, { K::Knob, kFlaRate, "Rate" }, { K::Knob, kFlaDepth, "Depth" },
                                    { K::Knob, kFlaManual, "Manual" }, { K::Knob, kFlaFeedback, "Feedback" }, { K::Knob, kFlaMix, "Mix" } } },
    { ModuleId::Vibrato, "Модуляция", "Vibrato", "Модуляция высоты тона без прямого сигнала.",
      IColor(255, 170, 130, 230), { { K::Toggle, kVibSync, "Sync" }, { K::Selector, kVibDivision, "Division" }, { K::Knob, kVibRate, "Rate" }, { K::Knob, kVibDepth, "Depth" } } },
    { ModuleId::Tremolo, "Модуляция", "Tremolo", "Пульсация громкости. В режиме стерео-панорамы звук перекатывается между каналами.",
      IColor(255, 230, 180, 120), { { K::Toggle, kTrmSync, "Sync" }, { K::Selector, kTrmDivision, "Division" }, { K::Selector, kTrmShape, "Shape" }, { K::Toggle, kTrmStereo, "Stereo Pan" },
                                    { K::Knob, kTrmRate, "Rate" }, { K::Knob, kTrmDepth, "Depth" } } },
    { ModuleId::Rotary, "Модуляция", "Rotary", "Вращающийся динамик: рупор и барабан с разной инерцией разгона. Переключайте Slow/Fast — скорость меняется плавно.",
      IColor(255, 200, 170, 120), { { K::Selector, kRotSpeed, "Speed" }, { K::Knob, kRotDrive, "Drive" }, { K::Knob, kRotBalance, "Horn/Drum" }, { K::Knob, kRotMix, "Mix" }, { K::Knob, kRotWidth, "Width" } } },
    { ModuleId::Delay, "Пространство", "Delay", "Пять режимов задержки. Sync — по темпу хоста (или ручному темпу в настройках). Feedback устойчив даже на 100%.",
      IColor(255, 120, 180, 230), { { K::Selector, kDlyMode, "Mode" }, { K::Toggle, kDlySync, "Sync" }, { K::Selector, kDlyDivision, "Division" }, { K::DelayTime, -1, "" },
                                    { K::Knob, kDlyTime, "Time" }, { K::Knob, kDlyFeedback, "Feedback" }, { K::Knob, kDlyMix, "Mix" }, { K::Knob, kDlyLowCut, "Low Cut" },
                                    { K::Knob, kDlyHighCut, "High Cut" }, { K::Knob, kDlySaturation, "Saturation" }, { K::Knob, kDlyModDepth, "Mod Depth" }, { K::Knob, kDlyModRate, "Mod Rate" },
                                    { K::Knob, kDlyWidth, "Width" } } },
    { ModuleId::Reverb, "Пространство", "Reverb", "Алгоритмическое пространство: от маленькой комнаты до бесконечного Shimmer. Freeze замораживает хвост. Это модели, а не измеренные помещения.",
      IColor(255, 150, 140, 240), { { K::Selector, kRvbMode, "Mode" }, { K::Toggle, kRvbFreeze, "Freeze" }, { K::Knob, kRvbMix, "Mix" }, { K::Knob, kRvbDecay, "Decay" },
                                    { K::Knob, kRvbSize, "Size" }, { K::Knob, kRvbPreDelay, "Pre-delay" }, { K::Knob, kRvbEarly, "Early" }, { K::Knob, kRvbDiffusion, "Diffusion" },
                                    { K::Knob, kRvbHighDamp, "High Damp" }, { K::Knob, kRvbLowCut, "Low Cut" }, { K::Knob, kRvbModulation, "Modulation" }, { K::Knob, kRvbWidth, "Width" },
                                    { K::Knob, kRvbShimmer, "Shimmer" } } },
  };
  return specs;
}

/** Строка списка модулей: LED (клик — вкл/выкл) + название (клик — выбрать). */
class FxListItem : public IControl
{
public:
  FxListItem(const IRECT& r, const FxSpec* spec, TwoonemoonSoundLab* plugin, std::function<void()> onSelect, std::function<bool()> isSelected)
  : IControl(r, moduleEnableParam(spec->id)), mSpec(spec), mPlugin(plugin), mOnSelect(std::move(onSelect)), mIsSelected(std::move(isSelected))
  {
  }
  void Draw(IGraphics& g) override
  {
    const bool on = GetValue() > 0.5;
    const bool sel = mIsSelected();
    if (sel || mHover)
    {
      g.PathClear();
      g.PathRoundRect(mRECT.GetPadded(-2.f), 8.f);
      g.PathFill(sel ? IColor(60, 255, 255, 255) : IColor(22, 255, 255, 255));
    }
    if (sel) g.FillRect(mSpec->color, IRECT(mRECT.L + 2.f, mRECT.T + 8.f, mRECT.L + 4.f, mRECT.B - 8.f));
    const float ledX = mRECT.L + 18.f, ledY = mRECT.MH();
    if (on)
    {
      g.PathClear();
      g.PathCircle(ledX, ledY, 9.f);
      g.PathFill(IPattern::CreateRadialGradient(ledX, ledY, 9.f, { { IColor(120, mSpec->color.R, mSpec->color.G, mSpec->color.B), 0.f }, { IColor(0, mSpec->color.R, mSpec->color.G, mSpec->color.B), 1.f } }));
    }
    g.FillCircle(on ? mSpec->color : col::bg3, ledX, ledY, 4.f);
    g.DrawText(txt(13.f, sel ? col::text : (on ? IColor(255, 210, 212, 218) : col::text2), sel ? font::semibold : font::medium, EAlign::Near), mSpec->title,
               mRECT.GetReducedFromLeft(34.f));
  }
  void OnMouseDown(float x, float, const IMouseMod&) override
  {
    if (x < mRECT.L + 32.f)
    {
      SetValue(GetValue() > 0.5 ? 0.0 : 1.0);
      SetDirty(true);
    }
    else if (mOnSelect)
      mOnSelect();
  }
  void OnMouseOver(float, float, const IMouseMod&) override { mHover = true; SetDirty(false); }
  void OnMouseOut() override { mHover = false; SetDirty(false); }

private:
  const FxSpec* mSpec;
  TwoonemoonSoundLab* mPlugin;
  std::function<void()> mOnSelect;
  std::function<bool()> mIsSelected;
  bool mHover = false;
};

/** Текущее время задержки (с учётом синхронизации). */
class DelayTimeLabel : public IControl
{
public:
  DelayTimeLabel(const IRECT& r, tsl::Delay* d) : IControl(r), mDelay(d) { mIgnoreMouse = true; }
  bool IsDirty() override { return std::fabs(mDelay->currentTimeMs() - mShown) > 0.05f || IControl::IsDirty(); }
  void Draw(IGraphics& g) override
  {
    mShown = mDelay->currentTimeMs();
    char buf[48];
    snprintf(buf, sizeof(buf), "%.1f ms", mShown);
    drawCapsLabel(g, "Время", mRECT.GetFromTop(14.f), col::text3, 9.f);
    g.DrawText(txt(16.f, col::text, font::monoMedium), buf, mRECT.GetReducedFromTop(14.f));
  }

private:
  tsl::Delay* mDelay;
  float mShown = -1.f;
};

} // namespace

void buildFxPage(UIContext& ctx, const char* page)
{
  const IRECT content = ctx.content;
  TwoonemoonSoundLab* plugin = ctx.plugin;
  Engine& engine = plugin->engine();
  const auto& specs = fxSpecs();
  auto selected = std::make_shared<int>(0);

  // ---- Список
  const IRECT list = content.GetFromLeft(250.f);
  ctx.add(new Canvas(list, [](IGraphics& g, const IRECT& r) { drawHardwarePanel(g, r, 12.f); }), page);
  float y = list.T + 12.f;
  const char* lastCat = "";
  std::vector<std::string> groups;
  for (size_t i = 0; i < specs.size(); ++i)
  {
    const FxSpec& s = specs[i];
    if (std::string(lastCat) != s.category)
    {
      lastCat = s.category;
      ctx.add(new Label(IRECT(list.L + 16.f, y, list.R - 10.f, y + 18.f), s.category, txt(9.f, col::text3, font::semibold, EAlign::Near), true), page);
      y += 20.f;
    }
    const int idx = static_cast<int>(i);
    ctx.add(new FxListItem(IRECT(list.L + 6.f, y, list.R - 6.f, y + 27.f), &s, plugin,
                           [&ctx, selected, idx]() {
                             *selected = idx;
                             ctx.notify(-1);
                             ctx.g->SetAllControlsDirty();
                           },
                           [selected, idx]() { return *selected == idx; }),
            page);
    y += 28.f;
  }

  // ---- Детальные панели (по одной на модуль; видна только выбранная)
  const IRECT detail = content.GetReducedFromLeft(266.f);
  for (size_t i = 0; i < specs.size(); ++i)
  {
    const FxSpec& s = specs[i];
    groups.push_back(std::string("fx.") + moduleKey(s.id));
    const char* grp = nullptr;
    // Строка группы должна жить столько же, сколько контролы — храним в статическом пуле
    static std::vector<std::unique_ptr<std::string>> pool;
    pool.push_back(std::make_unique<std::string>(groups.back()));
    grp = pool.back()->c_str();

    const FxSpec* sp = &s;
    ctx.g->AttachControl(new Canvas(detail, [sp](IGraphics& g, const IRECT& r) {
                           drawHardwarePanel(g, r, 14.f);
                           // Цветная полоса модуля
                           g.PathClear();
                           g.PathRoundRect(IRECT(r.L, r.T, r.R, r.T + 4.f), 2.f);
                           g.PathFill(sp->color);
                           g.DrawText(txt(20.f, col::text, font::semibold, EAlign::Near), sp->title, IRECT(r.L + 24.f, r.T + 18.f, r.R - 160.f, r.T + 46.f));
                           g.DrawMultiLineText(txt(12.f, col::text2, font::regular, EAlign::Near, EVAlign::Top), sp->description,
                                               IRECT(r.L + 24.f, r.T + 52.f, r.R - 180.f, r.T + 96.f));
                         }),
                         kNoTag, grp);
    ctx.g->AttachControl(new FootSwitch(IRECT(detail.R - 110.f, detail.T + 16.f, detail.R - 30.f, detail.T + 96.f), moduleEnableParam(s.id), "On", s.color), kNoTag, grp);

    // Ряд селекторов/переключателей
    float sx = detail.L + 24.f;
    const float selY = detail.T + 108.f;
    IRECT knobsArea(detail.L + 16.f, detail.T + 168.f, detail.R - 16.f, detail.B - 16.f);
    std::vector<const FxCtl*> knobs;
    for (const FxCtl& c : s.controls)
    {
      switch (c.kind)
      {
        case CtlKind::Selector:
          ctx.g->AttachControl(new EnumSelector(IRECT(sx, selY, sx + 190.f, selY + 44.f), c.param, c.label), kNoTag, grp);
          sx += 206.f;
          break;
        case CtlKind::Toggle:
          ctx.g->AttachControl(new ToggleChip(IRECT(sx, selY + 2.f, sx + 120.f, selY + 28.f), c.param, c.label, s.color), kNoTag, grp);
          sx += 136.f;
          break;
        case CtlKind::GateMeter:
        {
          auto* gate = static_cast<NoiseGate*>(engine.module(ModuleId::Gate));
          ctx.g->AttachControl(new ReductionMeter(IRECT(detail.L + 24.f, detail.B - 54.f, detail.R - 24.f, detail.B - 24.f),
                                                  [gate]() { return gate->gainReductionDb(); }, c.label, 60.f), kNoTag, grp);
          break;
        }
        case CtlKind::CompMeter:
        {
          auto* comp = static_cast<Compressor*>(engine.module(ModuleId::Compressor));
          ctx.g->AttachControl(new ReductionMeter(IRECT(detail.L + 24.f, detail.B - 54.f, detail.R - 24.f, detail.B - 24.f),
                                                  [comp]() { return comp->gainReductionDb(); }, c.label, 24.f), kNoTag, grp);
          break;
        }
        case CtlKind::DelayTime:
          ctx.g->AttachControl(new DelayTimeLabel(IRECT(sx, selY, sx + 130.f, selY + 44.f), static_cast<tsl::Delay*>(engine.module(ModuleId::Delay))), kNoTag, grp);
          sx += 140.f;
          break;
        case CtlKind::Knob: knobs.push_back(&c); break;
      }
    }
    if (sx == detail.L + 24.f) knobsArea = knobsArea.GetReducedFromTop(-50.f); // без селекторов — выше
    const int perRow = knobs.size() > 6 ? 6 : std::max<int>(1, static_cast<int>(knobs.size()));
    const float kw = std::min(130.f, knobsArea.W() / perRow);
    const float kh = 124.f;
    for (size_t k = 0; k < knobs.size(); ++k)
    {
      const int row = static_cast<int>(k) / perRow, colI = static_cast<int>(k) % perRow;
      const IRECT cell(knobsArea.L + colI * kw, knobsArea.T + row * (kh + 8.f), knobsArea.L + (colI + 1) * kw, knobsArea.T + row * (kh + 8.f) + kh);
      const IParam* prm = plugin->GetParam(knobs[k]->param);
      const bool bipolar = prm->GetMin() < 0.0 && prm->GetMax() > 0.0;
      ctx.g->AttachControl(new Knob(cell.GetPadded(-6.f), knobs[k]->param, knobs[k]->label, row == 0 && colI < 2 ? KnobStyle::Steel : KnobStyle::Small, s.color, bipolar), kNoTag, grp);
    }
  }

  // Выбор модуля извне (двойной клик на странице «Цепочка»)
  ctx.selectFx = [&ctx, selected](ModuleId id) {
    const auto& sp = fxSpecs();
    for (size_t i = 0; i < sp.size(); ++i)
      if (sp[i].id == id) *selected = static_cast<int>(i);
    ctx.notify(-1);
  };

  // ---- Видимость выбранной панели
  ctx.onParam([&ctx, selected, groups, page](int idx) {
    if (idx != -1) return;
    IControl* probe = nullptr;
    ctx.g->ForControlInGroup(page, [&probe](IControl* c) { if (!probe) probe = c; });
    const bool pageVisible = probe && !probe->IsHidden();
    for (size_t i = 0; i < groups.size(); ++i)
    {
      const bool show = pageVisible && static_cast<int>(i) == *selected;
      ctx.g->ForControlInGroup(groups[i].c_str(), [show](IControl* c) { c->Hide(!show); });
    }
  });
}

} // namespace tslui
