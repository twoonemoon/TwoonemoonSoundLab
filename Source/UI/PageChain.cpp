// SPDX-License-Identifier: GPL-3.0-or-later
// Страница «Цепочка»: перетаскивание модулей, включение, параллельная секция с путями A/B.
#include "UIContext.h"

#include "TwoonemoonSoundLab.h"

#include <algorithm>

namespace tslui {

using namespace tsl;

IColor moduleColor(ModuleId id)
{
  switch (id)
  {
    case ModuleId::Gate:
    case ModuleId::Compressor: return col::accent;
    case ModuleId::EnvFilter: return IColor(255, 120, 200, 170);
    case ModuleId::Boost: return IColor(255, 214, 207, 203);
    case ModuleId::Overdrive: return IColor(255, 200, 100, 160);
    case ModuleId::Distortion: return IColor(255, 160, 160, 172);
    case ModuleId::Fuzz: return IColor(255, 160, 140, 230);
    case ModuleId::Amp: return IColor(255, 227, 168, 87);
    case ModuleId::Cabinet: return IColor(255, 190, 150, 110);
    case ModuleId::Eq: return IColor(255, 120, 214, 176);
    case ModuleId::Chorus: return IColor(255, 130, 170, 240);
    case ModuleId::Phaser: return IColor(255, 220, 150, 110);
    case ModuleId::Flanger: return IColor(255, 110, 200, 220);
    case ModuleId::Vibrato: return IColor(255, 170, 130, 230);
    case ModuleId::Tremolo: return IColor(255, 230, 180, 120);
    case ModuleId::Rotary: return IColor(255, 200, 170, 120);
    case ModuleId::Delay: return IColor(255, 120, 180, 230);
    case ModuleId::Reverb: return IColor(255, 150, 140, 240);
    case ModuleId::Wah: return IColor(255, 200, 130, 90);
    case ModuleId::Octave: return IColor(255, 110, 190, 150);
    case ModuleId::Pitch: return IColor(255, 120, 220, 210);
    case ModuleId::Harmonizer: return IColor(255, 230, 140, 200);
    case ModuleId::Widener: return IColor(255, 140, 200, 230);
    case ModuleId::Looper: return IColor(255, 230, 120, 120);
    case ModuleId::Limiter: return IColor(255, 228, 86, 92);
    default: return col::steel;
  }
}

namespace {

struct Entry
{
  ModuleId id;
  bool inSection;
  bool laneB;
};

class ChainEditor : public IControl
{
public:
  ChainEditor(const IRECT& r, UIContext* ctx, std::function<void(ModuleId)> openEditor)
  : IControl(r), mCtx(ctx), mOpen(std::move(openEditor))
  {
  }

  bool IsDirty() override
  {
    uint32_t mask = 0;
    for (int i = 0; i < kNumModules; ++i)
      if (mCtx->plugin->GetParam(moduleEnableParam(static_cast<ModuleId>(i)))->Bool()) mask |= 1u << i;
    const uint32_t serial = mCtx->plugin->stateSerial();
    if (mask != mMask || serial != mSerial || mDragging) return true;
    return IControl::IsDirty();
  }

  void Draw(IGraphics& g) override
  {
    mSerial = mCtx->plugin->stateSerial();
    mMask = 0;
    for (int i = 0; i < kNumModules; ++i)
      if (mCtx->plugin->GetParam(moduleEnableParam(static_cast<ModuleId>(i)))->Bool()) mMask |= 1u << i;
    layout();
    const float cy = mRECT.MH();

    // Секция
    if (mSecL < mSecR)
    {
      const IRECT sec(mSecL - 8.f, cy - mLaneOffset - mTileH * 0.5f - 22.f, mSecR + 8.f, cy + mLaneOffset + mTileH * 0.5f + 12.f);
      drawGlass(g, sec, 16.f, 0.8f);
      drawCapsLabel(g, "A", IRECT(sec.L + 8.f, cy - mLaneOffset - 8.f, sec.L + 24.f, cy - mLaneOffset + 8.f), col::text2, 11.f, EAlign::Near);
      drawCapsLabel(g, "B", IRECT(sec.L + 8.f, cy + mLaneOffset - 8.f, sec.L + 24.f, cy + mLaneOffset + 8.f), col::text2, 11.f, EAlign::Near);
      drawCapsLabel(g, "Параллельная секция", IRECT(sec.L + 10.f, sec.T + 6.f, sec.R, sec.T + 18.f), col::text3, 9.f, EAlign::Near);
    }
    // Линии сигнала
    const IColor line(90, 255, 255, 255);
    g.DrawLine(line, mRECT.L + 10.f, cy, (mSecL < mSecR ? mSecL - 8.f : mRECT.R - 10.f), cy, nullptr, 2.f);
    if (mSecL < mSecR)
    {
      for (float ly : { cy - mLaneOffset, cy + mLaneOffset })
      {
        g.DrawLine(line, mSecL - 8.f, cy, mSecL + 4.f, ly, nullptr, 2.f);
        g.DrawLine(line, mSecL + 4.f, ly, mSecR - 4.f, ly, nullptr, 2.f);
        g.DrawLine(line, mSecR - 4.f, ly, mSecR + 8.f, cy, nullptr, 2.f);
      }
      g.DrawLine(line, mSecR + 8.f, cy, mRECT.R - 10.f, cy, nullptr, 2.f);
    }
    // Плитки
    for (size_t i = 0; i < mEntries.size(); ++i)
    {
      if (mDragging && static_cast<int>(i) == mDragIdx) continue;
      drawTile(g, mEntries[i].id, mTileRects[i], false);
    }
    // Маркер вставки при перетаскивании
    if (mDragging)
    {
      const auto drop = dropTarget(mDragX, mDragY);
      g.DrawLine(col::accent, drop.markerX, drop.markerY - mTileH * 0.6f, drop.markerX, drop.markerY + mTileH * 0.6f, nullptr, 3.f);
      const IRECT tr = IRECT(mDragX - mTileW * 0.5f, mDragY - mTileH * 0.5f, mDragX + mTileW * 0.5f, mDragY + mTileH * 0.5f);
      drawTile(g, mEntries[static_cast<size_t>(mDragIdx)].id, tr, true);
    }
    drawCapsLabel(g, "вход", IRECT(mRECT.L, cy + 8.f, mRECT.L + 60.f, cy + 22.f), col::text3, 9.f, EAlign::Near);
    drawCapsLabel(g, "выход", IRECT(mRECT.R - 60.f, cy + 8.f, mRECT.R, cy + 22.f), col::text3, 9.f, EAlign::Far);
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    layout();
    mPressIdx = hitTile(x, y);
    mPressX = x;
    mPressY = y;
    mDragging = false;
  }

  void OnMouseDrag(float x, float y, float, float, const IMouseMod&) override
  {
    if (mPressIdx < 0) return;
    if (!mDragging && std::hypot(x - mPressX, y - mPressY) > 5.f)
    {
      mDragging = true;
      mDragIdx = mPressIdx;
    }
    mDragX = x;
    mDragY = y;
    SetDirty(false);
  }

  void OnMouseUp(float x, float y, const IMouseMod&) override
  {
    if (mDragging)
    {
      applyDrop(dropTarget(x, y));
      mDragging = false;
    }
    else if (mPressIdx >= 0)
    {
      // Клик по верхней полосе плитки (LED) — включить/выключить
      const IRECT& r = mTileRects[static_cast<size_t>(mPressIdx)];
      if (y < r.T + 22.f)
      {
        const int pid = moduleEnableParam(mEntries[static_cast<size_t>(mPressIdx)].id);
        mCtx->setParam(pid, mCtx->plugin->GetParam(pid)->Bool() ? 0.0 : 1.0);
      }
    }
    mPressIdx = -1;
    SetDirty(false);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod&) override
  {
    const int i = hitTile(x, y);
    if (i >= 0 && mOpen) mOpen(mEntries[static_cast<size_t>(i)].id);
  }

private:
  struct Drop
  {
    int index;      // позиция вставки в mEntries (после удаления перетаскиваемой)
    bool inSection;
    bool laneB;
    float markerX, markerY;
  };

  void readEntries()
  {
    const ChainConfig cfg = mCtx->plugin->chainConfig();
    mEntries.clear();
    for (int i = 0; i < kNumModules; ++i)
    {
      const ModuleId id = cfg.order[static_cast<size_t>(i)];
      const bool in = cfg.parallel && i >= cfg.splitStart && i < cfg.splitEnd;
      mEntries.push_back({ id, in, in && cfg.onLaneB(id) });
    }
    mParallel = cfg.parallel;
    mSplitAt = cfg.splitStart; // для пустой секции
  }

  void layout()
  {
    readEntries();
    // Колонки: вне секции — по одной; в секции — max(A, B)
    int cols = 0, secA = 0, secB = 0;
    bool sectionSeen = false;
    for (const auto& e : mEntries)
    {
      if (e.inSection) { sectionSeen = true; (e.laneB ? secB : secA)++; }
      else cols++;
    }
    const int secCols = mParallel ? std::max({ secA, secB, 1 }) : 0;
    const int total = cols + secCols;
    const float avail = mRECT.W() - 80.f;
    mTileW = std::clamp(avail / std::max(total, 1) - 6.f, 34.f, 92.f);
    mTileH = std::min(96.f, mRECT.H() * 0.34f);
    mLaneOffset = mTileH * 0.5f + 18.f;
    const float colW = mTileW + 6.f;
    float x = mRECT.L + 40.f + (avail - colW * total) * 0.5f;
    const float cy = mRECT.MH();
    mTileRects.assign(mEntries.size(), IRECT());
    mSecL = mSecR = 0.f;
    bool sectionPlaced = false;
    for (size_t i = 0; i < mEntries.size(); ++i)
    {
      const auto& e = mEntries[i];
      if (mParallel && !sectionPlaced && (e.inSection || (!sectionSeen && static_cast<int>(i) == mSplitAt)))
      {
        // Разместить всю секцию
        mSecL = x;
        float xa = x, xb = x;
        for (size_t j = i; j < mEntries.size() && mEntries[j].inSection; ++j)
        {
          float& xx = mEntries[j].laneB ? xb : xa;
          const float ty = cy + (mEntries[j].laneB ? mLaneOffset : -mLaneOffset);
          mTileRects[j] = IRECT(xx + 3.f, ty - mTileH * 0.5f, xx + 3.f + mTileW, ty + mTileH * 0.5f);
          xx += colW;
        }
        x += colW * secCols;
        mSecR = x;
        sectionPlaced = true;
        if (e.inSection) continue;
      }
      if (e.inSection) continue;
      mTileRects[i] = IRECT(x + 3.f, cy - mTileH * 0.5f, x + 3.f + mTileW, cy + mTileH * 0.5f);
      x += colW;
    }
    if (mParallel && !sectionPlaced)
    {
      mSecL = x;
      mSecR = x + colW;
    }
  }

  void drawTile(IGraphics& g, ModuleId id, const IRECT& r, bool lifted)
  {
    const bool on = mCtx->plugin->GetParam(moduleEnableParam(id))->Bool();
    const IColor c = moduleColor(id);
    g.PathClear();
    g.PathRoundRect(r.GetTranslated(0.f, lifted ? 8.f : 3.f), 10.f);
    g.PathFill(IColor(lifted ? 150 : 100, 0, 0, 0));
    g.PathClear();
    g.PathRoundRect(r, 10.f);
    g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B, { { on ? IColor(255, 44, 46, 54) : col::bg2, 0.f }, { on ? col::bg2 : col::bg1, 1.f } }));
    g.PathClear();
    g.PathRoundRect(r.GetPadded(-0.5f), 10.f);
    g.PathStroke(on ? IColor(170, c.R, c.G, c.B) : col::hairline, on ? 1.5f : 1.f);
    // LED
    const float lx = r.MW(), ly = r.T + 12.f;
    if (on)
    {
      g.PathClear();
      g.PathCircle(lx, ly, 8.f);
      g.PathFill(IPattern::CreateRadialGradient(lx, ly, 8.f, { { IColor(120, c.R, c.G, c.B), 0.f }, { IColor(0, c.R, c.G, c.B), 1.f } }));
    }
    g.FillCircle(on ? c : col::bg3, lx, ly, 3.5f);
    g.DrawText(txt(std::min(15.f, mTileW * 0.26f), on ? col::text : col::text3, font::bold), moduleShortName(id), r.GetReducedFromTop(r.H() * 0.25f).GetFromTop(r.H() * 0.42f));
    g.DrawText(txt(std::min(9.5f, mTileW * 0.16f), on ? col::text2 : col::text3, font::medium), moduleDisplayName(id), r.GetFromBottom(r.H() * 0.3f));
  }

  int hitTile(float x, float y) const
  {
    for (size_t i = 0; i < mTileRects.size(); ++i)
      if (mTileRects[i].Contains(x, y)) return static_cast<int>(i);
    return -1;
  }

  Drop dropTarget(float x, float y)
  {
    const float cy = mRECT.MH();
    Drop d{ 0, false, false, x, cy };
    // Попадание в секцию: по горизонтали внутри секции и по вертикали — в одну из дорожек
    const bool inSecX = mParallel && x > mSecL - 4.f && x < mSecR + 4.f;
    if (inSecX && std::fabs(y - cy) > mTileH * 0.3f)
    {
      d.inSection = true;
      d.laneB = y > cy;
      d.markerY = cy + (d.laneB ? mLaneOffset : -mLaneOffset);
    }
    // Индекс вставки: среди плиток той же дорожки/основного ряда — по x
    int best = -1;
    float bestX = mRECT.R;
    for (size_t i = 0; i < mEntries.size(); ++i)
    {
      if (static_cast<int>(i) == mDragIdx) continue;
      const auto& e = mEntries[i];
      const bool sameRow = d.inSection ? (e.inSection && e.laneB == d.laneB) : !e.inSection;
      if (!sameRow) continue;
      if (mTileRects[i].MW() > x && mTileRects[i].MW() < bestX)
      {
        bestX = mTileRects[i].MW();
        best = static_cast<int>(i);
      }
    }
    if (best >= 0)
    {
      d.index = best;
      d.markerX = mTileRects[static_cast<size_t>(best)].L - 3.f;
    }
    else
    {
      // в конец ряда: после последней плитки этой дорожки
      int last = -1;
      for (size_t i = 0; i < mEntries.size(); ++i)
      {
        if (static_cast<int>(i) == mDragIdx) continue;
        const auto& e = mEntries[i];
        const bool sameRow = d.inSection ? (e.inSection && e.laneB == d.laneB) : !e.inSection;
        if (sameRow) last = static_cast<int>(i);
      }
      d.index = last + 1;
      d.markerX = last >= 0 ? mTileRects[static_cast<size_t>(last)].R + 3.f : (d.inSection ? mSecL + 4.f : mRECT.L + 40.f);
      if (d.inSection && last < 0)
      {
        // пустая дорожка: вставить в начало секции
        int first = -1;
        for (size_t i = 0; i < mEntries.size(); ++i)
          if (mEntries[i].inSection && static_cast<int>(i) != mDragIdx) { first = static_cast<int>(i); break; }
        d.index = first >= 0 ? first : mSplitAt;
      }
    }
    return d;
  }

  void applyDrop(const Drop& d)
  {
    std::vector<Entry> v = mEntries;
    Entry moved = v[static_cast<size_t>(mDragIdx)];
    int idx = d.index;
    if (idx > mDragIdx) --idx;
    v.erase(v.begin() + mDragIdx);
    moved.inSection = d.inSection;
    moved.laneB = d.inSection && d.laneB;
    idx = std::clamp(idx, 0, static_cast<int>(v.size()));
    v.insert(v.begin() + idx, moved);
    commit(v, mParallel, idx);
  }

  void commit(std::vector<Entry> v, bool parallel, int hint)
  {
    // Секция должна быть непрерывной: собрать её элементы вместе (стабильно) вокруг первой позиции
    int first = -1;
    for (size_t i = 0; i < v.size(); ++i)
      if (v[i].inSection) { first = static_cast<int>(i); break; }
    ChainConfig cfg;
    if (first >= 0)
    {
      std::vector<Entry> sec, rest;
      for (const auto& e : v) (e.inSection ? sec : rest).push_back(e);
      int insertAt = 0;
      for (int i = 0; i < first; ++i)
        if (!v[static_cast<size_t>(i)].inSection) ++insertAt;
      v.clear();
      v.insert(v.end(), rest.begin(), rest.begin() + insertAt);
      v.insert(v.end(), sec.begin(), sec.end());
      v.insert(v.end(), rest.begin() + insertAt, rest.end());
      cfg.splitStart = static_cast<uint8_t>(insertAt);
      cfg.splitEnd = static_cast<uint8_t>(insertAt + sec.size());
    }
    else
    {
      cfg.splitStart = cfg.splitEnd = static_cast<uint8_t>(std::clamp(hint, 0, kNumModules));
    }
    for (size_t i = 0; i < v.size(); ++i)
    {
      cfg.order[i] = v[i].id;
      if (v[i].laneB) cfg.laneB |= 1u << static_cast<int>(v[i].id);
    }
    cfg.parallel = parallel;
    mCtx->plugin->setChainConfig(cfg);
  }

public:
  void toggleParallel()
  {
    readEntries();
    std::vector<Entry> v = mEntries;
    if (!mParallel)
    {
      // По умолчанию: усилитель и кабинет — путь A, путь B пуст (смесь «чистый + обработанный»)
      for (auto& e : v) e.inSection = e.id == ModuleId::Amp || e.id == ModuleId::Cabinet;
      commit(v, true, 0);
    }
    else
    {
      for (auto& e : v) { e.inSection = false; e.laneB = false; }
      commit(v, false, 0);
    }
  }
  void resetDefault()
  {
    ChainConfig cfg;
    mCtx->plugin->setChainConfig(cfg);
  }
  bool parallel() const { return mCtx->plugin->chainConfig().parallel; }

private:
  UIContext* mCtx;
  std::function<void(ModuleId)> mOpen;
  std::vector<Entry> mEntries;
  std::vector<IRECT> mTileRects;
  bool mParallel = false;
  int mSplitAt = 0;
  float mSecL = 0.f, mSecR = 0.f, mTileW = 60.f, mTileH = 90.f, mLaneOffset = 70.f;
  int mPressIdx = -1, mDragIdx = -1;
  float mPressX = 0.f, mPressY = 0.f, mDragX = 0.f, mDragY = 0.f;
  bool mDragging = false;
  uint32_t mMask = 0xFFFFFFFF, mSerial = 0xFFFFFFFF;
};

} // namespace

void buildChainPage(UIContext& ctx, const char* page)
{
  auto openEditor = [&ctx](ModuleId id) {
    if (ctx.openModule) ctx.openModule(id);
  };
  const IRECT content = ctx.content;
  const IRECT top = content.GetFromTop(48.f);
  auto* editor = new ChainEditor(IRECT(content.L, top.B + 10.f, content.R, content.B - 70.f), &ctx, openEditor);
  ctx.add(new Canvas(IRECT(content.L, top.B + 10.f, content.R, content.B - 70.f), [](IGraphics& g, const IRECT& r) { drawHardwarePanel(g, r, 14.f); }), page);
  ctx.add(editor, page);
  ctx.add(new ActionButton(IRECT(top.L, top.T + 8.f, top.L + 230.f, top.B - 6.f), "Параллельная секция: вкл/выкл", [editor]() { editor->toggleParallel(); }, false), page);
  ctx.add(new ActionButton(IRECT(top.L + 240.f, top.T + 8.f, top.L + 420.f, top.B - 6.f), "Порядок по умолчанию", [editor]() { editor->resetDefault(); }, false), page);
  ctx.add(new Label(IRECT(top.L + 440.f, top.T, top.R, top.B),
                    "Перетаскивайте модули, чтобы изменить порядок. Клик по индикатору — вкл/выкл, двойной клик — открыть настройки.",
                    txt(11.f, col::text3, font::regular, EAlign::Near)),
          page);
  // Смесь путей
  const IRECT bottom = content.GetFromBottom(62.f);
  ctx.add(new Canvas(bottom, [](IGraphics& g, const IRECT& r) {
            drawGlass(g, r, 14.f, 0.9f);
            g.DrawText(txt(12.f, col::text2, font::regular, EAlign::Near),
                       "Parallel A/B Mix: 0% — только путь A, 50% — оба пути полностью, 100% — только путь B (пустой путь B = исходный сигнал).",
                       IRECT(r.L + 120.f, r.T, r.R - 20.f, r.B));
          }),
          page);
  ctx.add(new Knob(IRECT(bottom.L + 20.f, bottom.T - 6.f, bottom.L + 100.f, bottom.B + 4.f), kParallelMix, "A/B Mix", KnobStyle::Small), page);
}

} // namespace tslui
