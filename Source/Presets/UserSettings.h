// SPDX-License-Identifier: GPL-3.0-or-later
// Пользовательские настройки интерфейса (не часть проекта DAW): масштаб, подсказки, анимации,
// знакомство. Хранятся в ~/Library/Application Support/TwoonemoonSoundLab/settings.json.
#pragma once

#include <string>

namespace tsl {

struct UserSettings
{
  float uiScale = 1.f;
  bool tooltips = true;
  bool reduceMotion = false;
  bool onboardingDone = false;

  /** Загрузить (при ошибке — значения по умолчанию). */
  static UserSettings load(const std::string& path = {});
  bool save(const std::string& path = {}) const;
  static std::string defaultPath();
};

} // namespace tsl
