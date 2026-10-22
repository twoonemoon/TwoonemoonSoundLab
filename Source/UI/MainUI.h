// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "IGraphics.h"

class TwoonemoonSoundLab;

namespace tslui {

/** Построить главный интерфейс. Вызывается iPlug2 при создании/пересоздании окна. */
void buildMainUI(iplug::igraphics::IGraphics* g, TwoonemoonSoundLab& plugin);

} // namespace tslui
