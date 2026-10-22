// SPDX-License-Identifier: GPL-3.0-or-later
// Индивидуальные фасады 12 моделей усилителей: корпус, решётка/рельеф, панель управления.
#pragma once

#include "Controls.h"
#include "Theme.h"

namespace tslui {

enum class GrilleType
{
  Relief,       // резной рельеф (завитки), как скульптурный фасад
  Cells,        // полупрозрачные светящиеся соты
  Cloth,        // тканевая решётка
  Architecture, // вертикальные «архитектурные» нити
};

struct AmpFaceStyle
{
  IColor tolex, tolexHi;   // корпус
  IColor grille, grilleHi; // основа решётки
  IColor panel, panelHi;   // панель управления
  IColor label;            // подписи на панели
  IColor glow;             // цвет свечения
  KnobStyle knob;
  IColor knobAccent;
  GrilleType grilleType;
  int seed;
};

const AmpFaceStyle& ampFaceStyle(int model);

/** Нарисовать голову усилителя. panelTop — доля высоты, с которой начинается панель управления. glow 0…1. */
void drawAmpHead(IGraphics& g, const IRECT& r, int model, float panelFrac, float glow);

/** Резной рельеф из симметричных завитков (структурированный орнамент). */
void drawScrollRelief(IGraphics& g, const IRECT& r, int seed, const IColor& base, float depth);

} // namespace tslui
