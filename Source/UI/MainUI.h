// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "IGraphics.h"

class TwoonemoonSoundLab;

namespace tslui {

/** Компактный режим («50%»): отдельная раскладка 600×380 с читаемыми шрифтами (не уменьшенная копия). */
constexpr int kCompactWidth = 600;
constexpr int kCompactHeight = 380;
inline bool isCompactScale(float s) { return s < 0.6f; }

/** Построить главный интерфейс. Вызывается iPlug2 при создании/пересоздании окна. */
void buildMainUI(iplug::igraphics::IGraphics* g, TwoonemoonSoundLab& plugin);

} // namespace tslui
