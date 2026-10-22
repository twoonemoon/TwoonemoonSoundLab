// SPDX-License-Identifier: GPL-3.0-or-later
#include "AmpFaces.h"

#include "Amps/AmpVoices.h"

#include <cmath>

namespace tslui {

namespace {

IColor shade(const IColor& c, int d)
{
  auto cl = [](int v) { return std::max(0, std::min(255, v)); };
  return IColor(c.A, cl(c.R + d), cl(c.G + d), cl(c.B + d));
}

IColor alpha(const IColor& c, int a) { return IColor(a, c.R, c.G, c.B); }

struct Rng
{
  uint32_t s;
  explicit Rng(uint32_t seed) : s(seed * 747796405u + 2891336453u) {}
  float next()
  {
    s = s * 1664525u + 1013904223u;
    return static_cast<float>((s >> 8) & 0xFFFFFF) / 16777216.f;
  }
};

const AmpFaceStyle kStyles[tsl::kNumAmpModels] = {
  // Lumen Twin — перламутровый корпус, светлая тканевая решётка, сталь
  { IColor(255, 222, 218, 214), IColor(255, 246, 243, 240), IColor(255, 196, 192, 190), IColor(255, 226, 222, 220),
    IColor(255, 186, 190, 196), IColor(255, 232, 235, 238), IColor(255, 64, 66, 72), IColor(255, 214, 110, 186),
    KnobStyle::Steel, IColor(255, 190, 90, 160), GrilleType::Cloth, 3 },
  // Opal Bloom — тёплый опал, резной перламутровый рельеф
  { IColor(255, 214, 196, 196), IColor(255, 238, 224, 222), IColor(255, 226, 214, 210), IColor(255, 246, 238, 234),
    IColor(255, 64, 58, 62), IColor(255, 92, 86, 92), IColor(255, 236, 226, 228), IColor(255, 230, 150, 190),
    KnobStyle::Pearl, IColor(255, 214, 120, 170), GrilleType::Relief, 11 },
  // Glass Garden — розово-фиолетовые светящиеся соты, светлая панель
  { IColor(255, 196, 70, 150), IColor(255, 226, 110, 182), IColor(255, 90, 40, 96), IColor(255, 150, 70, 150),
    IColor(255, 228, 226, 230), IColor(255, 248, 247, 250), IColor(255, 70, 60, 74), IColor(255, 220, 120, 255),
    KnobStyle::Steel, IColor(255, 196, 92, 200), GrilleType::Cells, 17 },
  // Velvet Edge — сливовый бархат, тёмный рельеф
  { IColor(255, 70, 34, 60), IColor(255, 104, 54, 90), IColor(255, 84, 44, 74), IColor(255, 118, 66, 104),
    IColor(255, 40, 34, 40), IColor(255, 66, 58, 66), IColor(255, 214, 196, 210), IColor(255, 232, 120, 190),
    KnobStyle::Steel, IColor(255, 222, 120, 190), GrilleType::Relief, 23 },
  // Brass Circuit — чёрный корпус, шампанская шлифованная панель, тканевая решётка
  { IColor(255, 28, 28, 30), IColor(255, 52, 50, 52), IColor(255, 54, 50, 46), IColor(255, 82, 76, 70),
    IColor(255, 190, 170, 132), IColor(255, 232, 214, 176), IColor(255, 46, 38, 30), IColor(255, 255, 196, 140),
    KnobStyle::Black, IColor(255, 210, 150, 90), GrilleType::Cloth, 29 },
  // Rust Monarch — оксблад, медные нити
  { IColor(255, 82, 30, 28), IColor(255, 120, 50, 44), IColor(255, 40, 22, 20), IColor(255, 90, 46, 38),
    IColor(255, 52, 40, 38), IColor(255, 84, 66, 62), IColor(255, 226, 190, 170), IColor(255, 240, 150, 100),
    KnobStyle::Steel, IColor(255, 220, 130, 90), GrilleType::Architecture, 31 },
  // Thorn Drive — графит, стальные нити
  { IColor(255, 34, 36, 40), IColor(255, 60, 62, 68), IColor(255, 22, 23, 26), IColor(255, 56, 58, 64),
    IColor(255, 150, 156, 164), IColor(255, 206, 210, 216), IColor(255, 36, 38, 42), IColor(255, 196, 92, 166),
    KnobStyle::Black, IColor(255, 196, 92, 166), GrilleType::Architecture, 37 },
  // Iron Veil — матовый чёрный, чёрный рельеф
  { IColor(255, 18, 18, 20), IColor(255, 40, 40, 44), IColor(255, 26, 26, 29), IColor(255, 48, 48, 52),
    IColor(255, 24, 24, 27), IColor(255, 44, 44, 48), IColor(255, 190, 192, 198), IColor(255, 200, 100, 180),
    KnobStyle::Black, IColor(255, 190, 96, 176), GrilleType::Relief, 41 },
  // Catacomb — костяной рельеф на чёрном
  { IColor(255, 20, 20, 22), IColor(255, 44, 44, 48), IColor(255, 214, 208, 198), IColor(255, 240, 236, 228),
    IColor(255, 22, 22, 25), IColor(255, 46, 46, 50), IColor(255, 214, 208, 198), IColor(255, 255, 255, 255),
    KnobStyle::Black, IColor(255, 222, 216, 206), GrilleType::Relief, 47 },
  // Abyssal Lead — полуночный фиолетовый, светящиеся соты
  { IColor(255, 30, 26, 52), IColor(255, 54, 46, 88), IColor(255, 22, 16, 44), IColor(255, 70, 50, 130),
    IColor(255, 34, 30, 50), IColor(255, 60, 54, 84), IColor(255, 206, 196, 236), IColor(255, 150, 120, 255),
    KnobStyle::Steel, IColor(255, 150, 120, 240), GrilleType::Cells, 53 },
  // Silver Current Acoustic — шлифованное серебро, светлая ткань
  { IColor(255, 168, 174, 182), IColor(255, 214, 218, 224), IColor(255, 150, 154, 160), IColor(255, 186, 190, 196),
    IColor(255, 200, 204, 210), IColor(255, 238, 240, 244), IColor(255, 50, 54, 60), IColor(255, 160, 200, 255),
    KnobStyle::Steel, IColor(255, 120, 170, 220), GrilleType::Cloth, 59 },
  // Cedar Room Acoustic — тёплый кедр, резной рельеф
  { IColor(255, 120, 74, 48), IColor(255, 160, 106, 72), IColor(255, 138, 92, 62), IColor(255, 176, 124, 88),
    IColor(255, 52, 36, 28), IColor(255, 84, 60, 46), IColor(255, 236, 214, 190), IColor(255, 255, 196, 130),
    KnobStyle::Pearl, IColor(255, 220, 160, 100), GrilleType::Relief, 61 },
};

void drawCloth(IGraphics& g, const IRECT& r, const AmpFaceStyle& st)
{
  g.PathClear();
  g.PathRect(r);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B, { { st.grilleHi, 0.f }, { st.grille, 1.f } }));
  g.PathClipRegion(r);
  const float step = 4.f;
  const bool light = (st.grille.R + st.grille.G + st.grille.B) > 380;
  const IColor a = light ? IColor(26, 0, 0, 0) : IColor(22, 255, 255, 255);
  const IColor b = light ? IColor(18, 255, 255, 255) : IColor(30, 0, 0, 0);
  for (float x = r.L - r.H(); x < r.R; x += step)
  {
    g.DrawLine(a, x, r.B, x + r.H(), r.T, nullptr, 1.f);
    g.DrawLine(b, x + 1.5f, r.T, x + 1.5f + r.H(), r.B, nullptr, 1.f);
  }
  g.PathClipRegion();
  // Виньетка
  g.PathClear();
  g.PathRect(r);
  g.PathFill(IPattern::CreateRadialGradient(r.MW(), r.MH(), r.W() * 0.7f, { { IColor(0, 0, 0, 0), 0.5f }, { IColor(70, 0, 0, 0), 1.f } }));
}

void drawCells(IGraphics& g, const IRECT& r, const AmpFaceStyle& st, float glow)
{
  g.PathClear();
  g.PathRect(r);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B, { { st.grilleHi, 0.f }, { st.grille, 1.f } }));
  // Лампы за решёткой — мягкие пятна света (интенсивность зависит от сигнала, сдержанно)
  const float lamps = 4.f;
  for (int i = 0; i < 4; ++i)
  {
    const float cx = r.L + r.W() * (0.2f + 0.2f * i), cy = r.MH() + r.H() * 0.12f;
    const int a = static_cast<int>(70 + 90 * glow);
    g.PathClear();
    g.PathCircle(cx, cy, r.H() * 0.55f);
    g.PathFill(IPattern::CreateRadialGradient(cx, cy, r.H() * 0.55f, { { alpha(st.glow, a), 0.f }, { alpha(st.glow, 0), 1.f } }));
  }
  (void)lamps;
  // Соты
  g.PathClipRegion(r);
  const float R = 13.f;
  const float w = std::sqrt(3.f) * R, h = 1.5f * R;
  int row = 0;
  for (float y = r.T - R; y < r.B + R; y += h, ++row)
  {
    const float off = (row % 2) ? w * 0.5f : 0.f;
    for (float x = r.L - w + off; x < r.R + w; x += w)
    {
      g.PathClear();
      for (int k = 0; k < 6; ++k)
      {
        const float a = 0.5236f + k * 1.0472f;
        const float px = x + std::cos(a) * (R - 1.6f), py = y + std::sin(a) * (R - 1.6f);
        if (k == 0) g.PathMoveTo(px, py); else g.PathLineTo(px, py);
      }
      g.PathClose();
      g.PathFill(IPattern::CreateLinearGradient(x, y - R, x, y + R, { { IColor(40, 255, 255, 255), 0.f }, { IColor(60, 0, 0, 0), 1.f } }));
      g.PathStroke(IColor(70, 0, 0, 0), 1.2f);
    }
  }
  g.PathClipRegion();
  // Стеклянный блик сверху
  const IRECT hi = r.GetFromTop(r.H() * 0.35f);
  g.PathClear();
  g.PathRect(hi);
  g.PathFill(IPattern::CreateLinearGradient(hi.L, hi.T, hi.L, hi.B, { { IColor(50, 255, 255, 255), 0.f }, { IColor(0, 255, 255, 255), 1.f } }));
}

void drawArchitecture(IGraphics& g, const IRECT& r, const AmpFaceStyle& st, float glow)
{
  g.PathClear();
  g.PathRect(r);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B, { { st.grilleHi, 0.f }, { st.grille, 1.f } }));
  Rng rnd(static_cast<uint32_t>(st.seed));
  g.PathClipRegion(r);
  for (float x = r.L + 2.f; x < r.R; x += 2.5f)
  {
    const float v = rnd.next();
    if (v < 0.35f) continue;
    const float top = r.T + r.H() * (0.05f + 0.6f * rnd.next() * rnd.next());
    const float bot = r.B - r.H() * 0.05f * rnd.next();
    const int a = static_cast<int>(18 + 50 * rnd.next());
    const bool acc = rnd.next() > 0.985f;
    const IColor c = acc ? alpha(st.glow, static_cast<int>(90 + 100 * glow)) : IColor(a, 230, 232, 236);
    g.PathClear();
    g.PathMoveTo(x, top);
    g.PathLineTo(x, bot);
    g.PathStroke(IPattern::CreateLinearGradient(x, top, x, bot, { { alpha(c, 0), 0.f }, { c, 0.35f }, { alpha(c, c.A / 3), 1.f } }), acc ? 1.4f : 0.8f);
  }
  // Горизонтальные «этажи»
  for (int i = 0; i < 6; ++i)
  {
    const float y = r.T + r.H() * (0.18f + 0.13f * i);
    g.DrawLine(IColor(14, 255, 255, 255), r.L, y, r.R, y, nullptr, 1.f);
  }
  g.PathClipRegion();
  g.PathClear();
  g.PathRect(r);
  g.PathFill(IPattern::CreateRadialGradient(r.MW(), r.B, r.W() * 0.6f, { { alpha(st.glow, static_cast<int>(20 + 40 * glow)), 0.f }, { alpha(st.glow, 0), 1.f } }));
}

} // namespace

const AmpFaceStyle& ampFaceStyle(int model) { return kStyles[std::max(0, std::min(tsl::kNumAmpModels - 1, model))]; }

void drawScrollRelief(IGraphics& g, const IRECT& r, int seed, const IColor& base, float depth)
{
  g.PathClear();
  g.PathRect(r);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B, { { shade(base, 18), 0.f }, { base, 0.5f }, { shade(base, -22), 1.f } }));
  g.PathClipRegion(r);
  Rng rnd(static_cast<uint32_t>(seed));
  const float unit = r.H() * 0.95f;
  const int units = std::max(1, static_cast<int>(r.W() / unit));
  const float uw = r.W() / units;
  const bool light = (base.R + base.G + base.B) > 380;
  const IColor shadow = light ? IColor(static_cast<int>(90 * depth), 60, 50, 50) : IColor(static_cast<int>(150 * depth), 0, 0, 0);
  const IColor body = light ? shade(base, 10) : shade(base, 22);
  const IColor hilite = light ? IColor(static_cast<int>(200 * depth), 255, 255, 255) : IColor(static_cast<int>(70 * depth), 255, 255, 255);
  const float twist = 0.8f + 0.5f * rnd.next();
  const float leafiness = rnd.next();

  // Один завиток: стебель → логарифмическая спираль
  auto scroll = [&](float x0, float y0, float size, float dir, float rot) {
    std::vector<std::pair<float, float>> pts;
    const int stemN = 14;
    for (int i = 0; i <= stemN; ++i)
    {
      const float t = static_cast<float>(i) / stemN;
      const float x = x0 + dir * size * (0.15f * t + 0.35f * t * t);
      const float y = y0 - size * (0.75f * t);
      pts.emplace_back(x, y);
    }
    const float cx = pts.back().first + dir * size * 0.22f, cy = pts.back().second;
    const float a0 = dir > 0 ? tsl::kPi : 0.f;
    for (int i = 1; i <= 40; ++i)
    {
      const float th = i * 0.16f * twist;
      const float rr = size * 0.22f * std::exp(-0.32f * th);
      const float a = a0 + dir * (th + rot);
      pts.emplace_back(cx + std::cos(a) * rr, cy - std::sin(a) * rr);
    }
    for (int pass = 0; pass < 3; ++pass)
    {
      g.PathClear();
      const float dy = pass == 0 ? 2.2f : (pass == 1 ? 0.f : -0.9f);
      g.PathMoveTo(pts[0].first, pts[0].second + dy);
      for (size_t i = 1; i < pts.size(); ++i) g.PathLineTo(pts[i].first, pts[i].second + dy);
      IStrokeOptions o;
      o.mCapOption = ELineCap::Round;
      o.mJoinOption = ELineJoin::Round;
      if (pass == 0) g.PathStroke(shadow, size * 0.11f, o);
      else if (pass == 1) g.PathStroke(body, size * 0.10f, o);
      else g.PathStroke(hilite, size * 0.035f, o);
    }
    // Листья вдоль стебля
    if (leafiness > 0.3f)
      for (int l = 0; l < 2; ++l)
      {
        const auto& p = pts[static_cast<size_t>(4 + l * 5)];
        const float lx = p.first - dir * size * 0.12f, ly = p.second;
        g.PathClear();
        g.PathEllipse(lx, ly + 1.5f, size * 0.09f, size * 0.035f, dir * (35.f + l * 20.f));
        g.PathFill(shadow);
        g.PathClear();
        g.PathEllipse(lx, ly, size * 0.09f, size * 0.035f, dir * (35.f + l * 20.f));
        g.PathFill(body);
      }
  };

  for (int u = 0; u < units; ++u)
  {
    const float cx = r.L + uw * (u + 0.5f);
    const float base_y = r.B - r.H() * 0.12f;
    const float s = unit * 0.85f;
    scroll(cx, base_y, s, 1.f, 0.f);
    scroll(cx, base_y, s, -1.f, 0.f);
    // Малые завитки в промежутках
    scroll(cx + uw * 0.5f, r.T + r.H() * 0.55f, s * 0.45f, 1.f, 0.6f);
    scroll(cx + uw * 0.5f, r.T + r.H() * 0.55f, s * 0.45f, -1.f, 0.6f);
    // Центральная «почка»
    g.PathClear();
    g.PathCircle(cx, base_y - s * 0.86f, s * 0.05f);
    g.PathFill(hilite);
  }
  g.PathClipRegion();
  // Мягкое верхнее освещение
  g.PathClear();
  g.PathRect(r);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B, { { IColor(20, 255, 255, 255), 0.f }, { IColor(0, 0, 0, 0), 0.4f }, { IColor(50, 0, 0, 0), 1.f } }));
}

void drawAmpHead(IGraphics& g, const IRECT& bounds, int model, float panelFrac, float glow)
{
  // Верхние 18 px — место под ручку-ремень (рисуем внутри границ контрола)
  const IRECT r = bounds.GetReducedFromTop(18.f);
  const AmpFaceStyle& st = ampFaceStyle(model);
  const tsl::AmpVoice& v = tsl::ampVoice(model);
  const float rad = 18.f;

  // Ручка-ремень сверху
  const IRECT handle = IRECT(r.MW() - 90.f, r.T - 14.f, r.MW() + 90.f, r.T + 6.f);
  g.PathClear();
  g.PathRoundRect(handle, 8.f);
  g.PathFill(IPattern::CreateLinearGradient(handle.L, handle.T, handle.L, handle.B, { { IColor(255, 50, 48, 50), 0.f }, { IColor(255, 20, 20, 22), 1.f } }));
  for (float x : { handle.L + 10.f, handle.R - 10.f })
    drawSpunMetal(g, x, handle.MH() + 2.f, 5.f, col::steel, col::steelHi);

  // Тень корпуса
  g.PathClear();
  g.PathRoundRect(r.GetTranslated(0.f, 8.f).GetPadded(2.f), rad + 2.f);
  g.PathFill(IColor(130, 0, 0, 0));
  // Корпус
  g.PathClear();
  g.PathRoundRect(r, rad);
  g.PathFill(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B, { { st.tolexHi, 0.f }, { st.tolex, 0.45f }, { shade(st.tolex, -14), 1.f } }));
  // Текстура винила — едва заметная
  {
    Rng rnd(static_cast<uint32_t>(st.seed + 100));
    g.PathClipRegion(r);
    for (int i = 0; i < 260; ++i)
    {
      const float x = r.L + rnd.next() * r.W(), y = r.T + rnd.next() * r.H();
      g.FillCircle(IColor(10, (i & 1) ? 255 : 0, (i & 1) ? 255 : 0, (i & 1) ? 255 : 0), x, y, 0.8f + rnd.next());
    }
    g.PathClipRegion();
  }
  g.PathClear();
  g.PathRoundRect(r.GetPadded(-0.5f), rad);
  g.PathStroke(IPattern::CreateLinearGradient(r.L, r.T, r.L, r.B, { { IColor(80, 255, 255, 255), 0.f }, { IColor(120, 0, 0, 0), 1.f } }), 1.f);

  // Решётка / фасад
  const IRECT inner = r.GetPadded(-16.f);
  const IRECT grille = inner.GetFromTop(inner.H() * (panelFrac - 0.02f));
  g.PathClear();
  g.PathRoundRect(grille.GetPadded(2.f), 10.f);
  g.PathFill(IColor(120, 0, 0, 0));
  g.PathClipRegion(grille);
  switch (st.grilleType)
  {
    case GrilleType::Relief: drawScrollRelief(g, grille, st.seed, st.grille, 1.f); break;
    case GrilleType::Cells: drawCells(g, grille, st, glow); break;
    case GrilleType::Cloth: drawCloth(g, grille, st); break;
    case GrilleType::Architecture: drawArchitecture(g, grille, st, glow); break;
  }
  g.PathClipRegion();
  g.PathClear();
  g.PathRoundRect(grille, 8.f);
  g.PathStroke(IColor(140, 0, 0, 0), 2.f);

  // Панель управления: шлифованный металл
  const IRECT panel = inner.GetFromBottom(inner.H() * (1.f - panelFrac));
  drawBrushedRect(g, panel, 8.f, st.panel, st.panelHi, st.seed);
  g.PathClear();
  g.PathRoundRect(panel.GetPadded(-0.5f), 8.f);
  g.PathStroke(IColor(110, 0, 0, 0), 1.f);
  for (float x : { panel.L + 12.f, panel.R - 12.f })
    for (float y : { panel.T + 12.f, panel.B - 12.f })
    {
      drawSpunMetal(g, x, y, 4.f, col::steel, col::steelHi);
      g.DrawLine(IColor(120, 0, 0, 0), x - 2.5f, y - 2.5f, x + 2.5f, y + 2.5f, nullptr, 1.f);
    }

  // Шильдик с названием модели
  const float plateW = std::min(280.f, grille.W() * 0.34f);
  const IRECT plate(grille.R - plateW - 14.f, grille.B - 46.f, grille.R - 14.f, grille.B - 12.f);
  g.PathClear();
  g.PathRoundRect(plate.GetTranslated(0.f, 2.f), 6.f);
  g.PathFill(IColor(110, 0, 0, 0));
  drawBrushedRect(g, plate, 6.f, col::steel, col::steelHi, st.seed + 7);
  g.DrawText(txt(15.f, IColor(255, 30, 31, 36), font::bold), v.name, plate.GetTranslated(0.f, -1.f));
  // Индикатор питания (камень-линза)
  const float lx = panel.R - 46.f, ly = panel.MH();
  g.PathClear();
  g.PathCircle(lx, ly, 15.f);
  g.PathFill(IPattern::CreateRadialGradient(lx, ly, 15.f, { { alpha(st.glow, static_cast<int>(110 + 80 * glow)), 0.f }, { alpha(st.glow, 0), 1.f } }));
  g.FillCircle(IColor(255, 30, 28, 34), lx, ly, 7.f);
  g.FillCircle(alpha(st.glow, 230), lx, ly, 5.f);
  g.FillCircle(IColor(170, 255, 255, 255), lx - 1.6f, ly - 1.6f, 1.6f);
}

} // namespace tslui
