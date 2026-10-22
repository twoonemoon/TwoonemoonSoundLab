// SPDX-License-Identifier: GPL-3.0-or-later
#include "../TestHarness.h"

#include "../../Source/Presets/Preset.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <unistd.h>

using namespace tsl;
namespace fs = std::filesystem;

TEST_CASE("factory presets: 60 presets, unique names, valid keys and categories")
{
  const auto& f = factoryPresets();
  CHECK(f.size() >= 50);
  CHECK(f.size() == 60);
  std::set<std::string> names;
  std::map<std::string, int> perCat;
  for (const auto& p : f)
  {
    CHECK_MSG(names.insert(p.name).second, "duplicate %s", p.name.c_str());
    CHECK(p.factory);
    CHECK(!p.description.empty());
    CHECK(std::string(presetCategoryTitle(p.category)) != "Без категории");
    perCat[p.category]++;
    for (const auto& [k, v] : p.params)
    {
      const int id = paramIdForKey(k.c_str());
      CHECK_MSG(id >= 0, "%s: unknown key %s", p.name.c_str(), k.c_str());
      if (id >= 0) CHECK_MSG(paramClamp(id, v) == v || paramSpec(id).kind == ParamKind::Float, "%s: %s out of range", p.name.c_str(), k.c_str());
    }
    if (p.hasChain) CHECK(isValidChainConfig(p.chain));
  }
  CHECK(perCat["clean"] == 10);
  CHECK(perCat["crunch"] == 8);
  CHECK(perCat["highgain"] == 12);
  CHECK(perCat["ambient"] == 10);
  CHECK(perCat["lead"] == 6);
  CHECK(perCat["acoustic"] == 4);
  CHECK(perCat["harmony"] == 10);
}

TEST_CASE("preset json: round trip keeps params, chain and IR paths")
{
  Preset p = factoryPresets().back(); // с параллельной секцией
  p.irA = "/tmp/a.wav";
  const std::string text = presetToJson(p);
  Preset q;
  std::string err;
  CHECK(presetFromJson(text, q, err));
  CHECK(q.name == p.name && q.category == p.category && q.description == p.description);
  CHECK(q.params == p.params);
  CHECK(q.hasChain && q.chain == p.chain);
  CHECK(q.irA == "/tmp/a.wav");
  const ParamValues a = resolvePresetParams(p), b = resolvePresetParams(q);
  CHECK(a == b);

  CHECK(!presetFromJson("{ not json", q, err));
  CHECK(!err.empty());
  CHECK(!presetFromJson(R"({"format":"other","version":1,"name":"x"})", q, err));
  // Неизвестные ключи и модули игнорируются, отсутствующие модули дополняются
  CHECK(presetFromJson(R"({"format":"TwoonemoonSoundLab.preset","version":7,"name":"Future","params":{"amp.gain":7,"future.knob":3},
                           "chain":{"order":["reverb","amp","unknownfx"]}})", q, err));
  CHECK(q.params.at("amp.gain") == 7.f);
  CHECK(q.hasChain && q.chain.order[0] == ModuleId::Reverb && q.chain.order[1] == ModuleId::Amp);
  CHECK(isValidChainConfig(q.chain));
}

TEST_CASE("preset library: save, rename, favorites, import, corrupted file recovery")
{
  const std::string root = fs::temp_directory_path().string() + "/tsl_lib_" + std::to_string(getpid());
  fs::remove_all(root);
  {
    PresetLibrary lib(root);
    CHECK(lib.userPresets().empty());
    Preset p = factoryPresets()[0];
    p.name = "My: Tone/1";
    std::string err;
    CHECK(lib.save(p, false, err));
    CHECK(lib.userPresets().size() == 1);
    CHECK(lib.userPresets()[0].name == "My_ Tone_1");
    CHECK(!lib.save(p, false, err)); // без перезаписи
    CHECK(lib.save(p, true, err));
    CHECK(lib.rename("My_ Tone_1", "Lead Night", err));
    CHECK(lib.userPresets()[0].name == "Lead Night");
    lib.setFavorite("user:Lead Night", true);
    lib.pushRecent("factory:Glass Morning");
    lib.pushRecent("user:Lead Night");
    CHECK(lib.recent().front() == "user:Lead Night");

    // Повреждённый файл не ломает сканирование
    std::ofstream(lib.presetDir() + "/Broken.tslpreset") << "{ garbage";
    lib.rescan();
    CHECK(lib.userPresets().size() == 1);
    CHECK(lib.errors().size() == 1);

    // Экспорт/импорт с разрешением конфликта имён
    const std::string exported = root + "/export.tslpreset";
    CHECK(lib.exportFile(lib.userPresets()[0], exported, err));
    std::string imported;
    CHECK(lib.importFile(exported, imported, err));
    CHECK(imported == "Lead Night 2");
    CHECK(lib.remove("Lead Night 2", err));
  }
  {
    PresetLibrary lib2(root); // метаданные переживают перезапуск
    CHECK(lib2.isFavorite("user:Lead Night"));
    CHECK(lib2.recent().size() == 2);
  }
  fs::remove_all(root);
}
