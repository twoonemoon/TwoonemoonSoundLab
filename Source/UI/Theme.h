// SPDX-License-Identifier: GPL-3.0-or-later
// Дизайн-система TwoonemoonSoundLab: палитра, типографика и примитивы отрисовки
// поверхностей (графит, сталь, жемчуг, сдержанное «жидкое стекло», рельеф).
// См. docs/ART_DIRECTION.md.
#pragma once

#include "IGraphics.h"

namespace tslui {

using namespace iplug;
using namespace igraphics;

namespace col {
// База
inline const IColor bg0{ 255, 9, 10, 12 };       // глубокий графит
inline const IColor bg1{ 255, 17, 18, 22 };      // панель
inline const IColor bg2{ 255, 26, 28, 33 };      // приподнятая панель
inline const IColor bg3{ 255, 36, 38, 44 };      // контрол/поле
inline const IColor hairline{ 255, 46, 49, 56 }; // тонкие линии
// Металл
inline const IColor steelHi{ 255, 222, 226, 232 };
inline const IColor steel{ 255, 168, 174, 183 };
inline const IColor steelMid{ 255, 120, 126, 136 };
inline const IColor steelLo{ 255, 62, 66, 74 };
inline const IColor pearl{ 255, 236, 231, 228 };
inline const IColor pearlShade{ 255, 196, 188, 184 };
// Текст
inline const IColor text{ 255, 233, 234, 236 };
inline const IColor text2{ 255, 154, 160, 168 };
inline const IColor text3{ 255, 104, 110, 120 };
inline const IColor textDark{ 255, 34, 34, 38 };
// Акцент — приглушённый magenta/violet, использовать выборочно
inline const IColor accent{ 255, 196, 92, 166 };
inline const IColor accentSoft{ 255, 150, 84, 140 };
inline const IColor violet{ 255, 132, 102, 204 };
inline const IColor accentGlow{ 70, 214, 110, 186 };
// Сигнальные
inline const IColor ok{ 255, 120, 214, 176 };
inline const IColor warn{ 255, 227, 168, 87 };
inline const IColor hot{ 255, 228, 86, 92 };
} // namespace col

namespace font {
inline constexpr const char* regular = "Inter-Regular";
inline constexpr const char* medium = "Inter-Medium";
inline constexpr const char* semibold = "Inter-SemiBold";
inline constexpr const char* bold = "Inter-Bold";
inline constexpr const char* mono = "JBMono-Regular";
inline constexpr const char* monoMedium = "JBMono-Medium";
} // namespace font

inline IText txt(float size, const IColor& c = col::text, const char* f = font::regular, EAlign a = EAlign::Center,
                 EVAlign v = EVAlign::Middle)
{
  return IText(size, c, f, a, v);
}

/** Загрузить шрифты дизайн-системы. */
void loadFonts(IGraphics* g);

// --- Примитивы поверхностей ---------------------------------------------------

/** Фон окна: графит с едва заметными вертикальными «архитектурными» нитями. */
void drawBackdrop(IGraphics& g, const IRECT& r);

/** Плотная панель оборудования (графит, тонкая фаска, мягкая тень). */
void drawHardwarePanel(IGraphics& g, const IRECT& r, float radius = 10.f, const IColor& tint = col::bg1);

/** Сдержанное «жидкое стекло»: полупрозрачная заливка, верхний блик, тонкая кромка. */
void drawGlass(IGraphics& g, const IRECT& r, float radius = 14.f, float strength = 1.f);

/** Шлифованный (spun) алюминий в круге — для шапок ручек. */
void drawSpunMetal(IGraphics& g, float cx, float cy, float r, const IColor& base, const IColor& hi);

/** Шлифованный алюминий в прямоугольнике (горизонтальные штрихи). */
void drawBrushedRect(IGraphics& g, const IRECT& r, float radius, const IColor& base, const IColor& hi, int seed = 1);

/** Тиснёная линия: тень + блик (для рельефа и гравировки). */
void strokeEmbossed(IGraphics& g, float thickness, const IColor& shadow, const IColor& light, float offset = 0.8f);

/**
 * Процедурный орнамент «tribal relief»: симметричные плавные завитки, сгенерированные по seed.
 * Рисуется тиснением с низким контрастом. intensity 0..1.
 */
void drawOrnament(IGraphics& g, const IRECT& r, int seed, const IColor& shadow, const IColor& light, float intensity);

/** Подпись регулятора: прописные, с трекингом. */
void drawCapsLabel(IGraphics& g, const char* str, const IRECT& r, const IColor& c = col::text2, float size = 10.f,
                   EAlign align = EAlign::Center);

} // namespace tslui
