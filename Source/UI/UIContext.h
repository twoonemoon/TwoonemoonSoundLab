// SPDX-License-Identifier: GPL-3.0-or-later
// Общий контекст построения интерфейса: страницы, слушатели изменений параметров.
#pragma once

#include "Audio/Module.h"
#include "Presets/UserSettings.h"
#include "Controls.h"
#include "Theme.h"

#include <functional>
#include <vector>

class TwoonemoonSoundLab;

namespace tslui {

struct UIContext
{
  IGraphics* g = nullptr;
  TwoonemoonSoundLab* plugin = nullptr;
  IRECT content;
  /** Перейти к редактору модуля (страница + выбор), задаётся MainUI/страницей эффектов. */
  std::function<void(tsl::ModuleId)> openModule;
  std::function<void(tsl::ModuleId)> selectFx;
  tsl::UserSettings prefs;
  std::function<void()> showOnboarding;
  /** Переключить страницу (−1 — страница пресетов). */
  std::function<void(int)> gotoPage;
  /** Слушатели изменения параметров (UI thread). */
  std::vector<std::function<void(int)>> listeners;

  IControl* add(IControl* c, const char* page) { return g->AttachControl(c, kNoTag, page); }

  /** Установить параметр из UI (plain-значение): хост, DSP и все привязанные контролы. */
  void setParam(int idx, double plain);
  /** Жест редактирования (для автоматизации хоста) — начало/конец. */
  void beginEdit(int idx);
  void endEdit(int idx);
  void onParam(std::function<void(int)> fn) { listeners.push_back(std::move(fn)); }
  void notify(int paramIdx)
  {
    for (auto& l : listeners) l(paramIdx);
  }

  /** Фон секции с заголовком и подзаголовком. */
  void section(const IRECT& r, const char* title, const char* subtitle, const char* page)
  {
    add(new Canvas(r, [title, subtitle](IGraphics& gr, const IRECT& rr) {
          drawHardwarePanel(gr, rr, 12.f);
          gr.DrawText(txt(14.f, col::text, font::semibold, EAlign::Near), title, rr.GetPadded(-18.f, -12.f, 0.f, 0.f).GetFromTop(20.f));
          if (subtitle && *subtitle)
            gr.DrawText(txt(11.f, col::text3, font::regular, EAlign::Near), subtitle, rr.GetPadded(-18.f, -32.f, 0.f, 0.f).GetFromTop(16.f));
        }),
        page);
  }
};

// Построители страниц
void buildFxPage(UIContext& ctx, const char* page);
void buildChainPage(UIContext& ctx, const char* page);
void buildPresetsPage(UIContext& ctx, const char* page);
void buildSettingsPage(UIContext& ctx, const char* page);
void buildHelpPage(UIContext& ctx, const char* page);
IColor moduleColor(tsl::ModuleId id);
void buildEqPage(UIContext& ctx, const char* page);
void buildPlayPage(UIContext& ctx, const char* page);
void buildAmpPage(UIContext& ctx, const char* page);
void buildCabPage(UIContext& ctx, const char* page);

} // namespace tslui
