// SPDX-License-Identifier: GPL-3.0-or-later
#include "UserSettings.h"

#include "nlohmann/json.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace tsl {

std::string UserSettings::defaultPath()
{
  const char* home = std::getenv("HOME");
  return std::string(home ? home : ".") + "/Library/Application Support/TwoonemoonSoundLab/settings.json";
}

UserSettings UserSettings::load(const std::string& p)
{
  UserSettings s;
  std::ifstream f(p.empty() ? defaultPath() : p);
  if (!f) return s;
  try
  {
    std::stringstream ss;
    ss << f.rdbuf();
    const auto j = nlohmann::json::parse(ss.str());
    s.uiScale = j.value("uiScale", 1.f);
    if (!(s.uiScale >= 0.5f && s.uiScale <= 2.5f)) s.uiScale = 1.f;
    s.tooltips = j.value("tooltips", true);
    s.reduceMotion = j.value("reduceMotion", false);
    s.onboardingDone = j.value("onboardingDone", false);
  }
  catch (const std::exception&)
  {
    // Повреждённый файл — значения по умолчанию
  }
  return s;
}

bool UserSettings::save(const std::string& p) const
{
  const std::string path = p.empty() ? defaultPath() : p;
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
  nlohmann::json j;
  j["uiScale"] = uiScale;
  j["tooltips"] = tooltips;
  j["reduceMotion"] = reduceMotion;
  j["onboardingDone"] = onboardingDone;
  const std::string tmp = path + ".tmp";
  {
    std::ofstream f(tmp, std::ios::trunc);
    if (!f) return false;
    f << j.dump(2);
  }
  std::filesystem::rename(tmp, path, ec);
  return !ec;
}

} // namespace tsl
