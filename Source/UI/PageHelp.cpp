// SPDX-License-Identifier: GPL-3.0-or-later
// Страница «Справка»: поиск по разделам и текст выбранного раздела.
#include "HelpContent.h"
#include "UIContext.h"

#include <cctype>
#include <memory>

namespace tslui {

namespace {

struct HelpState
{
  std::string query;
  int selected = 0;
  uint32_t serial = 0;
};

std::string lowerUtf8(const std::string& s)
{
  std::string r = s;
  for (size_t i = 0; i < r.size(); ++i)
  {
    const unsigned char c = static_cast<unsigned char>(r[i]);
    if (c < 0x80) r[i] = static_cast<char>(std::tolower(c));
    else if (c == 0xD0 && i + 1 < r.size())
    {
      const unsigned char d = static_cast<unsigned char>(r[i + 1]);
      if (d >= 0x90 && d <= 0x9F) r[i + 1] = static_cast<char>(d + 0x20);
      else if (d >= 0xA0 && d <= 0xAF) { r[i] = static_cast<char>(0xD1); r[i + 1] = static_cast<char>(d - 0x20); }
      ++i;
    }
  }
  return r;
}

std::vector<int> matches(const HelpState& st)
{
  std::vector<int> out;
  const auto& topics = helpTopics();
  const std::string q = lowerUtf8(st.query);
  for (size_t i = 0; i < topics.size(); ++i)
  {
    const std::string hay = lowerUtf8(std::string(topics[i].title) + " " + topics[i].body + " " + topics[i].keywords);
    if (q.empty() || hay.find(q) != std::string::npos) out.push_back(static_cast<int>(i));
  }
  return out;
}

class HelpSearch : public IControl
{
public:
  HelpSearch(const IRECT& r, std::shared_ptr<HelpState> st) : IControl(r), mSt(std::move(st)) {}
  void Draw(IGraphics& g) override
  {
    g.PathClear();
    g.PathRoundRect(mRECT, mRECT.H() * 0.5f);
    g.PathFill(col::bg3);
    const bool empty = mSt->query.empty();
    g.DrawText(txt(12.f, empty ? col::text3 : col::text, font::regular, EAlign::Near), empty ? "Поиск по справке…" : mSt->query.c_str(), mRECT.GetReducedFromLeft(16.f));
  }
  void OnMouseDown(float, float, const IMouseMod&) override { GetUI()->CreateTextEntry(*this, txt(12.f, col::text, font::regular, EAlign::Near), mRECT.GetReducedFromLeft(12.f), mSt->query.c_str()); }
  void OnTextEntryCompletion(const char* s, int) override
  {
    mSt->query = s ? s : "";
    const auto m = matches(*mSt);
    mSt->selected = m.empty() ? -1 : m.front();
    mSt->serial++;
    GetUI()->SetAllControlsDirty();
  }

private:
  std::shared_ptr<HelpState> mSt;
};

class HelpList : public IControl
{
public:
  HelpList(const IRECT& r, std::shared_ptr<HelpState> st) : IControl(r), mSt(std::move(st)) {}
  bool IsDirty() override { return mSt->serial != mSerial || IControl::IsDirty(); }
  void Draw(IGraphics& g) override
  {
    mSerial = mSt->serial;
    drawHardwarePanel(g, mRECT, 12.f);
    mRows = matches(*mSt);
    const auto& topics = helpTopics();
    for (size_t i = 0; i < mRows.size(); ++i)
    {
      const IRECT r(mRECT.L + 8.f, mRECT.T + 8.f + i * 28.f, mRECT.R - 8.f, mRECT.T + 34.f + i * 28.f);
      if (r.B > mRECT.B) break;
      const bool sel = mRows[i] == mSt->selected;
      if (sel)
      {
        g.PathClear();
        g.PathRoundRect(r, 8.f);
        g.PathFill(IColor(55, 255, 255, 255));
      }
      g.DrawText(txt(12.5f, sel ? col::text : col::text2, sel ? font::semibold : font::medium, EAlign::Near), topics[static_cast<size_t>(mRows[i])].title,
                 r.GetReducedFromLeft(12.f));
    }
    if (mRows.empty()) g.DrawText(txt(12.f, col::text3, font::regular), "Ничего не найдено", mRECT);
  }
  void OnMouseDown(float, float y, const IMouseMod&) override
  {
    const int i = static_cast<int>((y - mRECT.T - 8.f) / 28.f);
    if (i >= 0 && i < static_cast<int>(mRows.size()))
    {
      mSt->selected = mRows[static_cast<size_t>(i)];
      mSt->serial++;
      GetUI()->SetAllControlsDirty();
    }
  }

private:
  std::shared_ptr<HelpState> mSt;
  std::vector<int> mRows;
  uint32_t mSerial = 0xFFFFFFFF;
};

class HelpBody : public IControl
{
public:
  HelpBody(const IRECT& r, std::shared_ptr<HelpState> st) : IControl(r), mSt(std::move(st)) { mIgnoreMouse = true; }
  bool IsDirty() override { return mSt->serial != mSerial || IControl::IsDirty(); }
  void Draw(IGraphics& g) override
  {
    mSerial = mSt->serial;
    drawHardwarePanel(g, mRECT, 12.f);
    const auto& topics = helpTopics();
    if (mSt->selected < 0 || mSt->selected >= static_cast<int>(topics.size())) return;
    const auto& t = topics[static_cast<size_t>(mSt->selected)];
    const IRECT in = mRECT.GetPadded(-24.f);
    g.DrawText(txt(20.f, col::text, font::semibold, EAlign::Near), t.title, in.GetFromTop(30.f));
    g.DrawMultiLineText(txt(13.f, col::text2, font::regular, EAlign::Near, EVAlign::Top), t.body, in.GetReducedFromTop(44.f));
  }

private:
  std::shared_ptr<HelpState> mSt;
  uint32_t mSerial = 0xFFFFFFFF;
};

} // namespace

void buildHelpPage(UIContext& ctx, const char* page)
{
  const IRECT content = ctx.content;
  auto st = std::make_shared<HelpState>();
  const IRECT left = content.GetFromLeft(330.f);
  ctx.add(new HelpSearch(IRECT(left.L, left.T, left.R, left.T + 34.f), st), page);
  ctx.add(new HelpList(IRECT(left.L, left.T + 44.f, left.R, left.B), st), page);
  ctx.add(new HelpBody(content.GetReducedFromLeft(346.f), st), page);
}

} // namespace tslui
