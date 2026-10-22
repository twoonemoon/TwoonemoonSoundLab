// SPDX-License-Identifier: GPL-3.0-or-later
#include "../TestHarness.h"

#include "../../Source/Audio/MidiMap.h"
#include "../../Source/Presets/UserSettings.h"

#include <filesystem>
#include <map>
#include <unistd.h>

using namespace tsl;

TEST_CASE("midi map: learn, apply, channel filter, inversion, removal")
{
  MidiMap m;
  std::map<int, float> got;
  auto apply = [&](int p, float v) { got[p] = v; };

  CHECK(!m.handleCC(0, 11, 64, apply)); // нет привязок
  m.startLearn(kWahPosition);
  CHECK(m.learnTarget() == kWahPosition);
  CHECK(m.handleCC(0, 11, 0, apply)); // обучение: привязка без применения значения
  CHECK(m.learnTarget() == -1);
  CHECK(got.empty());
  CHECK(m.mappingFor(kWahPosition).cc == 11);

  CHECK(m.handleCC(3, 11, 127, apply));
  CHECK_NEAR(got[kWahPosition], 1.0, 1e-6);
  m.handleCC(3, 11, 0, apply);
  CHECK_NEAR(got[kWahPosition], 0.0, 1e-6);

  // Инверсия и диапазон
  m.setRange(kWahPosition, 0.8f, 0.2f);
  m.handleCC(0, 11, 127, apply);
  CHECK_NEAR(got[kWahPosition], 0.2, 1e-6);

  // Фильтр канала
  m.assign(kAmpGain, 7, 2, 0.f, 1.f);
  got.clear();
  m.handleCC(0, 7, 100, apply);
  CHECK(got.count(kAmpGain) == 0);
  m.handleCC(2, 7, 127, apply);
  CHECK_NEAR(got[kAmpGain], 1.0, 1e-6);

  // Переназначение того же параметра заменяет привязку
  m.assign(kAmpGain, 8, -1, 0.f, 1.f);
  CHECK(m.mappingFor(kAmpGain).cc == 8);
  int count = 0;
  for (int i = 0; i < MidiMap::kSlots; ++i) count += m.slot(i).cc >= 0 ? 1 : 0;
  CHECK(count == 2);

  m.removeForParam(kAmpGain);
  CHECK(m.mappingFor(kAmpGain).cc == -1);
  const uint32_t v = m.version();
  m.clear();
  CHECK(m.version() != v);
  CHECK(m.mappingFor(kWahPosition).cc == -1);
  CHECK(!m.handleCC(0, 200, 10, apply)); // неверный CC игнорируется
}

TEST_CASE("user settings: save/load roundtrip, corrupted file falls back to defaults")
{
  const std::string path = std::filesystem::temp_directory_path().string() + "/tsl_settings_" + std::to_string(getpid()) + ".json";
  UserSettings s;
  s.uiScale = 1.5f;
  s.tooltips = false;
  s.reduceMotion = true;
  s.onboardingDone = true;
  CHECK(s.save(path));
  const UserSettings r = UserSettings::load(path);
  CHECK(r.uiScale == 1.5f && !r.tooltips && r.reduceMotion && r.onboardingDone);
  { FILE* f = fopen(path.c_str(), "w"); fputs("{broken", f); fclose(f); }
  const UserSettings d = UserSettings::load(path);
  CHECK(d.uiScale == 1.f && d.tooltips && !d.onboardingDone);
  std::filesystem::remove(path);
}
