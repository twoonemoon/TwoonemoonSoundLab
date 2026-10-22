// SPDX-License-Identifier: GPL-3.0-or-later
#include "Theme.h"
#include "config.h"

#include <cmath>
#include <string>

namespace tslui {

void loadFonts(IGraphics* g)
{
  g->LoadFont(font::regular, FONT_REGULAR_FN);
  g->LoadFont(font::medium, FONT_MEDIUM_FN);
  g->LoadFont(font::semibold, FONT_SEMIBOLD_FN);
  g->LoadFont(font::bold, FONT_BOLD_FN);
  g->LoadFont(font::mono, FONT_MONO_FN);
  g->LoadFont(font::monoMedium, FONT_MONO_MEDIUM_FN);
}

namespace {

// Детерминированный генератор для процедурных текстур
struct Lcg
{
  uint32_t s;
  explicit Lcg(uint32_t seed) : s(seed * 2654435761u + 12345u) {}
  float next()
  {
    s = s * 1664525u + 1013904223u;
    return static_cast<float>((s >> 8) & 0xFFFFFF) / 16777216.f;
  }
  float range(float a, float b) { return a + (b - a) * next(); }
};

IColor withAlpha(const IColor& c, int a)
{
  IColor r = c;
  r.A = a;
  return r;
}

} // namespace

void drawBackdrop(IGraphics& g, const IRECT& r)
{
  g.FillRect(col::bg0, r);
  // Мягкое верхнее освещение
  g.PathRect(r);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B,
                                            { { IColor(255, 22, 23, 28), 0.f }, { col::bg0, 0.55f }, { IColor(255, 7, 7, 9), 1.f } }));
  // Вертикальные нити (мотив монохромной архитектуры) — очень тонко и редко
  Lcg rnd(7);
  const float step = 9.f;
  for (float x = r.L + 4.f; x < r.R; x += step)
  {
    const float v = rnd.next();
    if (v < 0.78f) continue;
    const float top = r.T + r.H() * rnd.range(0.0f, 0.35f);
    const float bot = r.B - r.H() * rnd.range(0.0f, 0.4f);
    const int a = static_cast<int>(6 + 10 * rnd.next());
    g.PathClear();
    g.PathMoveTo(x, top);
    g.PathLineTo(x, bot);
    g.PathStroke(IPattern::CreateLinearGradient(x, top, x, bot,
                                                { { IColor(0, 200, 205, 215), 0.f }, { IColor(a, 200, 205, 215), 0.5f }, { IColor(0, 200, 205, 215), 1.f } }),
                 1.f);
  }
}

void drawHardwarePanel(IGraphics& g, const IRECT& r, float radius, const IColor& tint)
{
  // Тень под панелью
  g.PathClear();
  g.PathRoundRect(r.GetTranslated(0.f, 3.f).GetPadded(1.f), radius + 1.f);
  g.PathFill(IColor(110, 0, 0, 0));
  // Тело
  g.PathClear();
  g.PathRoundRect(r, radius);
  IColor top = tint, bot = tint;
  top.R = std::min(255, top.R + 10); top.G = std::min(255, top.G + 10); top.B = std::min(255, top.B + 12);
  bot.R = std::max(0, bot.R - 4); bot.G = std::max(0, bot.G - 4); bot.B = std::max(0, bot.B - 4);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B, { { top, 0.f }, { tint, 0.35f }, { bot, 1.f } }));
  // Фаска: светлая сверху, тёмная снизу
  g.PathClear();
  g.PathRoundRect(r.GetPadded(-0.5f), radius - 0.5f);
  g.PathStroke(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B,
                                              { { IColor(40, 255, 255, 255), 0.f }, { IColor(8, 255, 255, 255), 0.4f }, { IColor(90, 0, 0, 0), 1.f } }),
               1.f);
}

void drawGlass(IGraphics& g, const IRECT& r, float radius, float strength)
{
  const int a0 = static_cast<int>(28 * strength), a1 = static_cast<int>(10 * strength);
  g.PathClear();
  g.PathRoundRect(r, radius);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B,
                                            { { IColor(a0 + 8, 255, 255, 255), 0.f }, { IColor(a1, 255, 255, 255), 0.5f }, { IColor(a1 + 4, 210, 200, 230), 1.f } }));
  // Подложка, чтобы стекло читалось на тёмном фоне
  g.PathClear();
  g.PathRoundRect(r, radius);
  g.PathFill(IColor(static_cast<int>(120 * strength), 14, 15, 19));
  // Верхний блик
  const IRECT hi = r.GetFromTop(r.H() * 0.5f);
  g.PathClear();
  g.PathRoundRect(hi.GetPadded(-1.f), radius - 1.f, radius - 1.f, 0.f, 0.f);
  g.PathFill(IPattern::CreateLinearGradient(hi.L, hi.T, hi.L, hi.B,
                                            { { IColor(static_cast<int>(26 * strength), 255, 255, 255), 0.f }, { IColor(0, 255, 255, 255), 1.f } }));
  // Кромка
  g.PathClear();
  g.PathRoundRect(r.GetPadded(-0.5f), radius - 0.5f);
  g.PathStroke(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B,
                                              { { IColor(static_cast<int>(70 * strength), 255, 255, 255), 0.f }, { IColor(static_cast<int>(18 * strength), 255, 255, 255), 0.6f }, { IColor(static_cast<int>(40 * strength), 255, 255, 255), 1.f } }),
               1.f);
}

void drawSpunMetal(IGraphics& g, float cx, float cy, float r, const IColor& base, const IColor& hi)
{
  // База с радиальным градиентом (свет сверху-слева)
  g.PathClear();
  g.PathCircle(cx, cy, r);
  g.PathFill(IPattern::CreateRadialGradient(cx - r * 0.35f, cy - r * 0.45f, r * 1.6f, { { hi, 0.f }, { base, 0.75f }, { col::steelLo, 1.f } }));
  // Концентрические штрихи токарной обработки
  for (int i = 1; i < 9; ++i)
  {
    const float rr = r * (static_cast<float>(i) / 9.f);
    g.PathClear();
    g.PathCircle(cx, cy, rr);
    const int a = (i % 2) ? 16 : 9;
    g.PathStroke(IColor(a, 255, 255, 255), 0.6f);
  }
  // Блики-«лучи» анизотропного металла
  g.PathClear();
  g.PathMoveTo(cx, cy);
  g.PathArc(cx, cy, r, -60.f, -20.f);
  g.PathClose();
  g.PathFill(IColor(22, 255, 255, 255));
  g.PathClear();
  g.PathMoveTo(cx, cy);
  g.PathArc(cx, cy, r, 120.f, 160.f);
  g.PathClose();
  g.PathFill(IColor(14, 255, 255, 255));
}

void drawBrushedRect(IGraphics& g, const IRECT& r, float radius, const IColor& base, const IColor& hi, int seed)
{
  g.PathClear();
  g.PathRoundRect(r, radius);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B, { { hi, 0.f }, { base, 0.55f }, { col::steelMid, 1.f } }));
  g.PathClipRegion(r.GetPadded(-1.f));
  Lcg rnd(static_cast<uint32_t>(seed));
  for (float y = r.T + 1.f; y < r.B - 1.f; y += 1.5f)
  {
    const int a = static_cast<int>(rnd.range(3.f, 18.f));
    const bool light = rnd.next() > 0.5f;
    g.PathClear();
    g.PathMoveTo(r.L, y);
    g.PathLineTo(r.R, y);
    g.PathStroke(light ? IColor(a, 255, 255, 255) : IColor(a, 0, 0, 0), 0.7f);
  }
  g.PathClipRegion();
}

void strokeEmbossed(IGraphics& g, float thickness, const IColor& shadow, const IColor& light, float offset)
{
  // Путь уже задан: рисуем тень со сдвигом вниз, затем блик со сдвигом вверх.
  g.PathTransformSave();
  g.PathTransformTranslate(0.f, offset);
  g.PathStroke(shadow, thickness);
  g.PathTransformRestore();
  g.PathTransformSave();
  g.PathTransformTranslate(0.f, -offset * 0.6f);
  g.PathStroke(light, thickness * 0.8f);
  g.PathTransformRestore();
}

void drawOrnament(IGraphics& g, const IRECT& r, int seed, const IColor& shadow, const IColor& light, float intensity)
{
  if (intensity <= 0.f) return;
  Lcg rnd(static_cast<uint32_t>(seed) * 97u + 3u);
  const float cx = r.MW();
  const float h = r.H(), w = r.W() * 0.5f;
  const int strokes = 5 + static_cast<int>(rnd.next() * 4.f);
  const IColor sh = withAlpha(shadow, static_cast<int>(shadow.A * intensity));
  const IColor li = withAlpha(light, static_cast<int>(light.A * intensity));

  g.PathClipRegion(r);
  for (int s = 0; s < strokes; ++s)
  {
    // Каждый «завиток» — цепочка кубических кривых, завершающаяся спиралью.
    float x = rnd.range(0.05f, 0.9f) * w;
    float y = rnd.range(0.15f, 0.85f) * h;
    float ang = rnd.range(0.f, 6.283f);
    const float len = rnd.range(0.12f, 0.32f) * w;
    const float curl = rnd.range(0.6f, 1.4f) * (rnd.next() > 0.5f ? 1.f : -1.f);
    const float thick = rnd.range(1.0f, 2.2f);

    for (int mirror = 0; mirror < 2; ++mirror)
    {
      const float sx = mirror ? 1.f : -1.f;
      auto P = [&](float px, float py) { return std::pair<float, float>(cx + sx * px, r.T + py); };
      g.PathClear();
      float px = x, py = y, pa = ang, plen = len;
      auto start = P(px, py);
      g.PathMoveTo(start.first, start.second);
      for (int seg = 0; seg < 5; ++seg)
      {
        const float na = pa + curl * (0.7f + 0.25f * seg);
        const float nx = px + std::cos(na) * plen;
        const float ny = py + std::sin(na) * plen * 0.8f;
        const auto c1 = P(px + std::cos(pa) * plen * 0.55f, py + std::sin(pa) * plen * 0.45f);
        const auto c2 = P(nx - std::cos(na) * plen * 0.4f, ny - std::sin(na) * plen * 0.35f);
        const auto e = P(nx, ny);
        g.PathCubicBezierTo(c1.first, c1.second, c2.first, c2.second, e.first, e.second);
        px = nx; py = ny; pa = na; plen *= 0.62f;
      }
      strokeEmbossed(g, thick, sh, li, 0.9f);
    }
  }
  g.PathClipRegion();
}

void drawCapsLabel(IGraphics& g, const char* str, const IRECT& r, const IColor& c, float size, EAlign align)
{
  if (!str || !*str) return;
  // Верхний регистр для ASCII и кириллицы (UTF-8)
  std::string up;
  up.reserve(64);
  const unsigned char* p = reinterpret_cast<const unsigned char*>(str);
  while (*p)
  {
    unsigned char a = *p;
    if (a < 0x80) { up.push_back(static_cast<char>(std::toupper(a))); ++p; continue; }
    if (a == 0xD0 && p[1] >= 0xB0 && p[1] <= 0xBF) { up.push_back(char(0xD0)); up.push_back(char(p[1] - 0x20)); p += 2; continue; }
    if (a == 0xD1 && p[1] >= 0x80 && p[1] <= 0x8F) { up.push_back(char(0xD0)); up.push_back(char(p[1] + 0x20)); p += 2; continue; }
    if (a == 0xD1 && p[1] == 0x91) { up.push_back(char(0xD0)); up.push_back(char(0x81)); p += 2; continue; }
    up.push_back(static_cast<char>(a));
    ++p;
  }
  // Трекинг: рисуем по символам с добавочным интервалом
  const IText t = txt(size, c, font::semibold, EAlign::Near);
  const float tracking = size * 0.12f;
  std::vector<std::string> glyphs;
  for (size_t i = 0; i < up.size();)
  {
    const unsigned char b = static_cast<unsigned char>(up[i]);
    const size_t len = b < 0x80 ? 1 : (b >> 5) == 0x6 ? 2 : (b >> 4) == 0xE ? 3 : 4;
    glyphs.push_back(up.substr(i, len));
    i += len;
  }
  std::vector<float> widths(glyphs.size());
  float total = 0.f;
  for (size_t i = 0; i < glyphs.size(); ++i)
  {
    IRECT b;
    widths[i] = glyphs[i] == " " ? size * 0.32f : g.MeasureText(t, glyphs[i].c_str(), b);
    total += widths[i] + (i + 1 < glyphs.size() ? tracking : 0.f);
  }
  float x = align == EAlign::Near ? r.L : align == EAlign::Far ? r.R - total : r.MW() - total * 0.5f;
  for (size_t i = 0; i < glyphs.size(); ++i)
  {
    if (glyphs[i] != " ") g.DrawText(t, glyphs[i].c_str(), IRECT(x, r.T, x + widths[i] + 2.f, r.B));
    x += widths[i] + tracking;
  }
}

} // namespace tslui
