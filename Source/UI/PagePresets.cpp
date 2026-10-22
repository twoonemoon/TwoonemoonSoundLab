// SPDX-License-Identifier: GPL-3.0-or-later
// Страница «Пресеты»: категории, избранное, недавние, поиск, список и карточка пресета.
#include "UIContext.h"

#include "TwoonemoonSoundLab.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>

namespace tslui {

using namespace tsl;

namespace {

/** Общее состояние браузера (фильтр, выбор), разделяемое контролами страницы. */
struct BrowserState
{
  std::string filter = "all"; // all | fav | recent | <category key>
  std::string search;
  std::string selectedId;
  int scroll = 0;
  uint32_t serial = 0; // увеличивается при любом изменении — контролы перерисовываются
};

std::string lower(const std::string& s)
{
  std::string r = s;
  // ASCII + кириллица в UTF-8 (А–Я → а–я)
  for (size_t i = 0; i < r.size(); ++i)
  {
    unsigned char c = static_cast<unsigned char>(r[i]);
    if (c < 0x80) r[i] = static_cast<char>(std::tolower(c));
    else if (c == 0xD0 && i + 1 < r.size())
    {
      unsigned char d = static_cast<unsigned char>(r[i + 1]);
      if (d >= 0x90 && d <= 0x9F) r[i + 1] = static_cast<char>(d + 0x20);
      else if (d >= 0xA0 && d <= 0xAF) { r[i] = static_cast<char>(0xD1); r[i + 1] = static_cast<char>(d - 0x20); }
      ++i;
    }
  }
  return r;
}

std::vector<const Preset*> filtered(TwoonemoonSoundLab* plugin, const BrowserState& st)
{
  std::vector<const Preset*> out;
  auto& lib = plugin->library();
  const auto all = plugin->allPresets();
  const std::string q = lower(st.search);
  auto matches = [&](const Preset* p) {
    if (q.empty()) return true;
    if (lower(p->name).find(q) != std::string::npos) return true;
    if (lower(p->description).find(q) != std::string::npos) return true;
    for (const auto& t : p->tags)
      if (lower(t).find(q) != std::string::npos) return true;
    return false;
  };
  if (st.filter == "recent")
  {
    for (const auto& id : lib.recent())
      for (const Preset* p : all)
        if (p->id() == id && matches(p)) out.push_back(p);
    return out;
  }
  for (const Preset* p : all)
  {
    if (st.filter == "fav" && !lib.isFavorite(p->id())) continue;
    if (st.filter == "user" && p->factory) continue;
    if (st.filter != "all" && st.filter != "fav" && st.filter != "user" && p->category != st.filter) continue;
    if (matches(p)) out.push_back(p);
  }
  return out;
}

const Preset* findById(TwoonemoonSoundLab* plugin, const std::string& id)
{
  for (const Preset* p : plugin->allPresets())
    if (p->id() == id) return p;
  return nullptr;
}

class CategoryList : public IControl
{
public:
  CategoryList(const IRECT& r, std::shared_ptr<BrowserState> st) : IControl(r), mSt(std::move(st))
  {
    mItems.push_back({ "all", "Все пресеты" });
    mItems.push_back({ "fav", "★ Избранное" });
    mItems.push_back({ "recent", "Недавние" });
    for (const auto& c : presetCategories())
      if (std::string(c.key) != "user") mItems.push_back({ c.key, c.title });
    mItems.push_back({ "user", "Мои пресеты" });
  }
  bool IsDirty() override { return mSt->serial != mSerial || IControl::IsDirty(); }
  void Draw(IGraphics& g) override
  {
    mSerial = mSt->serial;
    drawHardwarePanel(g, mRECT, 12.f);
    for (size_t i = 0; i < mItems.size(); ++i)
    {
      const IRECT r = rowRect(static_cast<int>(i));
      const bool sel = mItems[i].first == mSt->filter;
      if (sel)
      {
        g.PathClear();
        g.PathRoundRect(r, 8.f);
        g.PathFill(IColor(55, 255, 255, 255));
        g.FillRect(col::accent, IRECT(r.L, r.T + 7.f, r.L + 2.f, r.B - 7.f));
      }
      g.DrawText(txt(12.5f, sel ? col::text : col::text2, sel ? font::semibold : font::medium, EAlign::Near), mItems[i].second.c_str(), r.GetReducedFromLeft(14.f));
      if (i == 2) g.FillRect(col::hairline, IRECT(r.L + 10.f, r.B + 4.f, r.R - 10.f, r.B + 5.f));
    }
  }
  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    for (size_t i = 0; i < mItems.size(); ++i)
      if (rowRect(static_cast<int>(i)).Contains(x, y))
      {
        mSt->filter = mItems[i].first;
        mSt->scroll = 0;
        mSt->serial++;
        GetUI()->SetAllControlsDirty();
      }
  }

private:
  IRECT rowRect(int i) const
  {
    const float extra = i > 2 ? 10.f : 0.f;
    return IRECT(mRECT.L + 8.f, mRECT.T + 10.f + i * 32.f + extra, mRECT.R - 8.f, mRECT.T + 38.f + i * 32.f + extra);
  }
  std::shared_ptr<BrowserState> mSt;
  std::vector<std::pair<std::string, std::string>> mItems;
  uint32_t mSerial = 0xFFFFFFFF;
};

class SearchBox : public IControl
{
public:
  SearchBox(const IRECT& r, std::shared_ptr<BrowserState> st) : IControl(r), mSt(std::move(st)) {}
  void Draw(IGraphics& g) override
  {
    g.PathClear();
    g.PathRoundRect(mRECT, mRECT.H() * 0.5f);
    g.PathFill(col::bg3);
    g.PathClear();
    g.PathRoundRect(mRECT.GetPadded(-0.5f), mRECT.H() * 0.5f);
    g.PathStroke(col::hairline, 1.f);
    // лупа
    const float cx = mRECT.L + 18.f, cy = mRECT.MH();
    g.PathClear();
    g.PathCircle(cx, cy - 1.f, 5.f);
    g.PathStroke(col::text3, 1.5f);
    g.DrawLine(col::text3, cx + 3.5f, cy + 2.5f, cx + 7.f, cy + 6.f, nullptr, 1.5f);
    const bool empty = mSt->search.empty();
    g.DrawText(txt(12.f, empty ? col::text3 : col::text, font::regular, EAlign::Near), empty ? "Поиск по имени, описанию и тегам…" : mSt->search.c_str(),
               mRECT.GetReducedFromLeft(32.f));
  }
  void OnMouseDown(float, float, const IMouseMod&) override { GetUI()->CreateTextEntry(*this, txt(12.f, col::text, font::regular, EAlign::Near), mRECT.GetReducedFromLeft(28.f), mSt->search.c_str()); }
  void OnTextEntryCompletion(const char* str, int) override
  {
    mSt->search = str ? str : "";
    mSt->scroll = 0;
    mSt->serial++;
    GetUI()->SetAllControlsDirty();
  }

private:
  std::shared_ptr<BrowserState> mSt;
};

class PresetListView : public IControl
{
public:
  PresetListView(const IRECT& r, std::shared_ptr<BrowserState> st, TwoonemoonSoundLab* plugin) : IControl(r), mSt(std::move(st)), mPlugin(plugin) {}
  bool IsDirty() override { return mSt->serial != mSerial || mPlugin->currentPresetId() != mShownCurrent || IControl::IsDirty(); }
  void Draw(IGraphics& g) override
  {
    mSerial = mSt->serial;
    mShownCurrent = mPlugin->currentPresetId();
    drawHardwarePanel(g, mRECT, 12.f);
    mRows = filtered(mPlugin, *mSt);
    const int visible = visibleRows();
    mSt->scroll = std::clamp(mSt->scroll, 0, std::max(0, static_cast<int>(mRows.size()) - visible));
    auto& lib = mPlugin->library();
    g.PathClipRegion(mRECT.GetPadded(-2.f));
    for (int i = 0; i < visible && mSt->scroll + i < static_cast<int>(mRows.size()); ++i)
    {
      const Preset* p = mRows[static_cast<size_t>(mSt->scroll + i)];
      const IRECT r = rowRect(i);
      const bool sel = p->id() == mSt->selectedId;
      const bool current = p->id() == mPlugin->currentPresetId();
      if (sel || current)
      {
        g.PathClear();
        g.PathRoundRect(r, 7.f);
        g.PathFill(sel ? IColor(60, 255, 255, 255) : IColor(24, 196, 92, 166));
      }
      if (current) g.FillCircle(col::accent, r.L + 10.f, r.MH(), 3.f);
      g.DrawText(txt(13.f, col::text, current ? font::semibold : font::medium, EAlign::Near), p->name.c_str(), IRECT(r.L + 22.f, r.T, r.L + 330.f, r.B));
      g.DrawText(txt(11.f, col::text3, font::regular, EAlign::Near), p->factory ? presetCategoryTitle(p->category) : "Мой пресет",
                 IRECT(r.L + 340.f, r.T, r.R - 40.f, r.B));
      const bool fav = lib.isFavorite(p->id());
      g.DrawText(txt(14.f, fav ? col::warn : col::text3, font::regular), fav ? "★" : "☆", IRECT(r.R - 34.f, r.T, r.R - 6.f, r.B));
    }
    g.PathClipRegion();
    if (mRows.empty())
      g.DrawText(txt(13.f, col::text3, font::regular), "Ничего не найдено", mRECT);
    // Полоса прокрутки
    if (static_cast<int>(mRows.size()) > visible)
    {
      const float h = mRECT.H() - 20.f;
      const float th = std::max(30.f, h * visible / mRows.size());
      const float ty = mRECT.T + 10.f + (h - th) * mSt->scroll / std::max(1, static_cast<int>(mRows.size()) - visible);
      g.PathClear();
      g.PathRoundRect(IRECT(mRECT.R - 7.f, ty, mRECT.R - 4.f, ty + th), 1.5f);
      g.PathFill(IColor(90, 255, 255, 255));
    }
  }
  void OnMouseWheel(float, float, const IMouseMod&, float d) override
  {
    mSt->scroll = std::max(0, mSt->scroll - static_cast<int>(d * 2.f));
    mSt->serial++;
    SetDirty(false);
  }
  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    const int i = hit(y);
    if (i < 0) return;
    const Preset* p = mRows[static_cast<size_t>(i)];
    if (x > mRECT.R - 44.f)
    {
      mPlugin->library().setFavorite(p->id(), !mPlugin->library().isFavorite(p->id()));
    }
    else
      mSt->selectedId = p->id();
    mSt->serial++;
    GetUI()->SetAllControlsDirty();
  }
  void OnMouseDblClick(float, float y, const IMouseMod&) override
  {
    const int i = hit(y);
    if (i < 0) return;
    mPlugin->loadPreset(*mRows[static_cast<size_t>(i)]);
    mSt->serial++;
  }

private:
  int visibleRows() const { return static_cast<int>((mRECT.H() - 16.f) / 30.f); }
  IRECT rowRect(int i) const { return IRECT(mRECT.L + 8.f, mRECT.T + 8.f + i * 30.f, mRECT.R - 12.f, mRECT.T + 36.f + i * 30.f); }
  int hit(float y) const
  {
    const int i = static_cast<int>((y - mRECT.T - 8.f) / 30.f) + mSt->scroll;
    return (i >= 0 && i < static_cast<int>(mRows.size())) ? i : -1;
  }
  std::shared_ptr<BrowserState> mSt;
  TwoonemoonSoundLab* mPlugin;
  std::vector<const Preset*> mRows;
  uint32_t mSerial = 0xFFFFFFFF;
  std::string mShownCurrent;
};

/** Карточка выбранного пресета. */
class PresetCard : public IControl
{
public:
  PresetCard(const IRECT& r, std::shared_ptr<BrowserState> st, TwoonemoonSoundLab* plugin) : IControl(r), mSt(std::move(st)), mPlugin(plugin) { mIgnoreMouse = true; }
  bool IsDirty() override { return mSt->serial != mSerial || IControl::IsDirty(); }
  void Draw(IGraphics& g) override
  {
    mSerial = mSt->serial;
    drawHardwarePanel(g, mRECT, 12.f);
    const Preset* p = findById(mPlugin, mSt->selectedId);
    const IRECT in = mRECT.GetPadded(-20.f);
    if (!p)
    {
      g.DrawMultiLineText(txt(12.f, col::text3, font::regular, EAlign::Near, EVAlign::Top),
                          "Выберите пресет в списке. Двойной клик — загрузить. Звезда — добавить в избранное.", in);
      return;
    }
    drawCapsLabel(g, p->factory ? presetCategoryTitle(p->category) : "Мой пресет", in.GetFromTop(14.f), col::text3, 9.f, EAlign::Near);
    g.DrawText(txt(20.f, col::text, font::semibold, EAlign::Near), p->name.c_str(), IRECT(in.L, in.T + 18.f, in.R, in.T + 46.f));
    g.DrawMultiLineText(txt(12.f, col::text2, font::regular, EAlign::Near, EVAlign::Top), p->description.empty() ? "Без описания" : p->description.c_str(),
                        IRECT(in.L, in.T + 54.f, in.R, in.T + 150.f));
    // Теги
    float x = in.L, y = in.T + 156.f;
    for (const auto& t : p->tags)
    {
      IRECT b;
      const float w = g.MeasureText(txt(10.f, col::text2, font::medium), t.c_str(), b) + 16.f;
      if (x + w > in.R) { x = in.L; y += 24.f; }
      const IRECT tr(x, y, x + w, y + 20.f);
      g.PathClear();
      g.PathRoundRect(tr, 10.f);
      g.PathFill(col::bg3);
      g.DrawText(txt(10.f, col::text2, font::medium), t.c_str(), tr);
      x += w + 6.f;
    }
  }

private:
  std::shared_ptr<BrowserState> mSt;
  TwoonemoonSoundLab* mPlugin;
  uint32_t mSerial = 0xFFFFFFFF;
};

/** Кнопка, вызывающая ввод текста (имя пресета). */
class NameEntryButton : public ActionButton
{
public:
  NameEntryButton(const IRECT& r, const char* label, std::function<std::string()> initial, std::function<void(const std::string&)> onDone)
  : ActionButton(r, label, nullptr, false), mInitial(std::move(initial)), mDone(std::move(onDone))
  {
  }
  void OnMouseDown(float, float, const IMouseMod&) override
  {
    GetUI()->CreateTextEntry(*this, txt(12.f, col::text, font::regular), mRECT, mInitial ? mInitial().c_str() : "");
  }
  void OnTextEntryCompletion(const char* str, int) override
  {
    if (str && *str && mDone) mDone(str);
  }

private:
  std::function<std::string()> mInitial;
  std::function<void(const std::string&)> mDone;
};

} // namespace

void buildPresetsPage(UIContext& ctx, const char* page)
{
  const IRECT content = ctx.content;
  TwoonemoonSoundLab* plugin = ctx.plugin;
  auto st = std::make_shared<BrowserState>();
  st->selectedId = plugin->currentPresetId();

  const IRECT cats = content.GetFromLeft(210.f);
  const IRECT card = content.GetFromRight(350.f);
  const IRECT mid = IRECT(cats.R + 14.f, content.T, card.L - 14.f, content.B);
  ctx.add(new CategoryList(cats, st), page);
  ctx.add(new SearchBox(IRECT(mid.L, mid.T, mid.R, mid.T + 34.f), st), page);
  ctx.add(new PresetListView(IRECT(mid.L, mid.T + 44.f, mid.R, mid.B), st, plugin), page);
  const IRECT cardInfo = card.GetFromTop(card.H() - 200.f);
  ctx.add(new PresetCard(cardInfo, st, plugin), page);

  // Действия
  auto msg = [&ctx](const std::string& text) { ctx.g->ShowMessageBox(text.c_str(), "TwoonemoonSoundLab", kMB_OK); };
  const IRECT act = card.GetFromBottom(188.f);
  const float bw = (act.W() - 8.f) / 2.f, bh = 34.f;
  auto cell = [&](int row, int colI, int span = 1) {
    return IRECT(act.L + colI * (bw + 8.f), act.T + row * (bh + 8.f), act.L + colI * (bw + 8.f) + bw * span + 8.f * (span - 1), act.T + row * (bh + 8.f) + bh);
  };
  auto* loadBtn = new ActionButton(cell(0, 0, 2), "Загрузить", [plugin, st]() {
    if (const Preset* p = findById(plugin, st->selectedId)) plugin->loadPreset(*p);
    st->serial++;
  }, true);
  loadBtn->SetHighlighted(true);
  ctx.add(loadBtn, page);
  ctx.add(new NameEntryButton(cell(1, 0), "Сохранить как…", [plugin]() { return plugin->currentPresetName(); },
                              [plugin, st, msg](const std::string& name) {
                                Preset p = plugin->snapshot();
                                p.name = name;
                                p.category = "user";
                                std::string err;
                                if (!plugin->library().save(p, false, err)) { msg(err); return; }
                                p.name = PresetLibrary::sanitizeFileName(name);
                                p.factory = false;
                                plugin->markSaved(p);
                                st->selectedId = p.id();
                                st->serial++;
                              }),
          page);
  ctx.add(new ActionButton(cell(1, 1), "Дублировать", [plugin, st, msg]() {
    const Preset* src = findById(plugin, st->selectedId);
    if (!src) return;
    Preset p = *src;
    p.name = src->name + " (копия)";
    p.category = "user";
    std::string err;
    if (!plugin->library().save(p, false, err)) { msg(err); return; }
    st->selectedId = "user:" + PresetLibrary::sanitizeFileName(p.name);
    st->serial++;
  }, false), page);
  ctx.add(new NameEntryButton(cell(2, 0), "Переименовать", [plugin, st]() {
                                const Preset* p = findById(plugin, st->selectedId);
                                return p ? p->name : std::string();
                              },
                              [plugin, st, msg](const std::string& name) {
                                const Preset* p = findById(plugin, st->selectedId);
                                if (!p || p->factory) { msg("Заводские пресеты нельзя переименовать — сохраните копию."); return; }
                                std::string err;
                                if (!plugin->library().rename(p->name, name, err)) { msg(err); return; }
                                st->selectedId = "user:" + PresetLibrary::sanitizeFileName(name);
                                st->serial++;
                              }),
          page);
  ctx.add(new ActionButton(cell(2, 1), "Удалить", [&ctx, plugin, st, msg]() {
    const Preset* p = findById(plugin, st->selectedId);
    if (!p) return;
    if (p->factory) { msg("Заводские пресеты встроены в программу и не удаляются."); return; }
    const std::string name = p->name;
    ctx.g->ShowMessageBox(("Удалить пресет «" + name + "»?").c_str(), "TwoonemoonSoundLab", kMB_YESNO, [plugin, st, name, msg](EMsgBoxResult r) {
      if (r != kYES) return;
      std::string err;
      if (!plugin->library().remove(name, err)) msg(err);
      st->selectedId.clear();
      st->serial++;
    });
  }, false), page);
  ctx.add(new ActionButton(cell(3, 0), "Импорт…", [&ctx, plugin, st, msg]() {
    WDL_String file, dir;
    ctx.g->PromptForFile(file, dir, EFileAction::Open, "tslpreset", [plugin, st, msg](const WDL_String& f, const WDL_String&) {
      if (f.GetLength() == 0) return;
      std::string name, err;
      if (!plugin->library().importFile(f.Get(), name, err)) { msg(err); return; }
      st->selectedId = "user:" + name;
      st->filter = "user";
      st->serial++;
    });
  }, false), page);
  ctx.add(new ActionButton(cell(3, 1), "Экспорт…", [&ctx, plugin, st, msg]() {
    const Preset* p = findById(plugin, st->selectedId);
    if (!p) { msg("Сначала выберите пресет."); return; }
    const Preset copy = *p;
    WDL_String file(PresetLibrary::sanitizeFileName(copy.name).c_str()), dir;
    file.Append(".tslpreset");
    ctx.g->PromptForFile(file, dir, EFileAction::Save, "tslpreset", [plugin, copy, msg](const WDL_String& f, const WDL_String&) {
      if (f.GetLength() == 0) return;
      std::string err;
      if (!plugin->library().exportFile(copy, f.Get(), err)) msg(err);
    });
  }, false), page);

  // Ошибки чтения пользовательских пресетов показываем один раз
  ctx.onParam([plugin, st, page, &ctx](int idx) {
    if (idx == -2) st->serial++;
    if (idx != -1) return;
    static bool reported = false;
    const auto& errs = plugin->library().errors();
    if (!reported && !errs.empty())
    {
      reported = true;
      std::string text = "Некоторые файлы пресетов повреждены и пропущены:\n";
      for (const auto& e : errs) text += "• " + e + "\n";
      ctx.g->ShowMessageBox(text.c_str(), "TwoonemoonSoundLab", kMB_OK);
    }
    (void)page;
  });
}

} // namespace tslui
