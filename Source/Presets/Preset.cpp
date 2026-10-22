// SPDX-License-Identifier: GPL-3.0-or-later
#include "Preset.h"

#include "nlohmann/json.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace tsl {

const std::vector<PresetCategory>& presetCategories()
{
  static const std::vector<PresetCategory> cats = {
    { "clean", "Чистые" },     { "crunch", "Кранч" },        { "highgain", "High Gain" }, { "ambient", "Эмбиент и шугейз" },
    { "lead", "Поп, блюз, соло" }, { "acoustic", "Акустика" }, { "harmony", "Гармонии и эксперименты" }, { "user", "Мои пресеты" },
  };
  return cats;
}

const char* presetCategoryTitle(const std::string& key)
{
  for (const auto& c : presetCategories())
    if (key == c.key) return c.title;
  return "Без категории";
}

ParamValues resolvePresetParams(const Preset& p)
{
  ParamValues v{};
  for (int i = 0; i < kNumParams; ++i) v[static_cast<size_t>(i)] = paramSpec(i).defaultValue;
  for (const auto& [key, value] : p.params)
  {
    const int id = paramIdForKey(key.c_str());
    if (id >= 0) v[static_cast<size_t>(id)] = paramClamp(id, value);
  }
  return v;
}

Preset makePresetFromState(const ParamValues& values, const ChainConfig& chain, const std::string& irA, const std::string& irB)
{
  Preset p;
  for (int i = 0; i < kNumParams; ++i) p.params[paramSpec(i).key] = values[static_cast<size_t>(i)];
  p.hasChain = true;
  p.chain = chain;
  p.irA = irA;
  p.irB = irB;
  return p;
}

std::string presetToJson(const Preset& p)
{
  json j;
  j["format"] = "TwoonemoonSoundLab.preset";
  j["version"] = kPresetFormatVersion;
  j["name"] = p.name;
  j["category"] = p.category;
  j["description"] = p.description;
  j["author"] = p.author;
  j["tags"] = p.tags;
  json params = json::object();
  for (const auto& [k, v] : p.params) params[k] = v;
  j["params"] = params;
  if (p.hasChain)
  {
    json c;
    json order = json::array();
    for (ModuleId id : p.chain.order) order.push_back(moduleKey(id));
    c["order"] = order;
    c["parallel"] = p.chain.parallel;
    c["splitStart"] = p.chain.splitStart;
    c["splitEnd"] = p.chain.splitEnd;
    json lane = json::array();
    for (int i = 0; i < kNumModules; ++i)
      if (p.chain.onLaneB(static_cast<ModuleId>(i))) lane.push_back(moduleKey(static_cast<ModuleId>(i)));
    c["laneB"] = lane;
    j["chain"] = c;
  }
  if (!p.irA.empty() || !p.irB.empty()) j["userIR"] = { { "a", p.irA }, { "b", p.irB } };
  return j.dump(2);
}

bool presetFromJson(const std::string& text, Preset& out, std::string& error)
{
  json j;
  try
  {
    j = json::parse(text);
  }
  catch (const std::exception&)
  {
    error = "Файл пресета повреждён (неверный JSON)";
    return false;
  }
  if (!j.is_object() || j.value("format", "") != "TwoonemoonSoundLab.preset")
  {
    error = "Это не пресет TwoonemoonSoundLab";
    return false;
  }
  const int version = j.value("version", 0);
  if (version < 1)
  {
    error = "Неизвестная версия формата пресета";
    return false;
  }
  if (version > kPresetFormatVersion)
  {
    // Новее, чем поддерживает эта версия программы: читаем известные поля, неизвестные игнорируем
  }
  Preset p;
  p.name = j.value("name", "");
  if (p.name.empty())
  {
    error = "У пресета нет имени";
    return false;
  }
  p.category = j.value("category", "user");
  p.description = j.value("description", "");
  p.author = j.value("author", "");
  if (j.contains("tags") && j["tags"].is_array())
    for (const auto& t : j["tags"])
      if (t.is_string()) p.tags.push_back(t.get<std::string>());
  if (j.contains("params") && j["params"].is_object())
    for (auto it = j["params"].begin(); it != j["params"].end(); ++it)
      if (it.value().is_number()) p.params[it.key()] = it.value().get<float>();
  if (j.contains("chain") && j["chain"].is_object())
  {
    const auto& c = j["chain"];
    ChainConfig cfg;
    std::array<bool, kNumModules> used{};
    int count = 0;
    if (c.contains("order") && c["order"].is_array())
      for (const auto& k : c["order"])
      {
        if (!k.is_string()) continue;
        const ModuleId id = moduleIdForKey(k.get<std::string>().c_str());
        if (id == ModuleId::Count || used[static_cast<size_t>(id)] || count >= kNumModules) continue;
        cfg.order[static_cast<size_t>(count++)] = id;
        used[static_cast<size_t>(id)] = true;
      }
    // Модули, появившиеся в более новой версии, добавляются на свои места по умолчанию
    for (ModuleId id : defaultChainOrder())
      if (!used[static_cast<size_t>(id)] && count < kNumModules) cfg.order[static_cast<size_t>(count++)] = id;
    cfg.parallel = c.value("parallel", false);
    cfg.splitStart = static_cast<uint8_t>(std::clamp(c.value("splitStart", 0), 0, kNumModules));
    cfg.splitEnd = static_cast<uint8_t>(std::clamp(c.value("splitEnd", 0), static_cast<int>(cfg.splitStart), kNumModules));
    if (c.contains("laneB") && c["laneB"].is_array())
      for (const auto& k : c["laneB"])
        if (k.is_string())
        {
          const ModuleId id = moduleIdForKey(k.get<std::string>().c_str());
          if (id != ModuleId::Count) cfg.laneB |= 1u << static_cast<int>(id);
        }
    if (isValidChainConfig(cfg))
    {
      p.hasChain = true;
      p.chain = cfg;
    }
  }
  if (j.contains("userIR") && j["userIR"].is_object())
  {
    p.irA = j["userIR"].value("a", "");
    p.irB = j["userIR"].value("b", "");
  }
  out = std::move(p);
  return true;
}

// ---------------------------------------------------------------------------
// Библиотека

namespace {

std::string defaultRoot()
{
  const char* home = std::getenv("HOME");
  return std::string(home ? home : ".") + "/Library/Application Support/TwoonemoonSoundLab";
}

bool readText(const std::string& path, std::string& out)
{
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

bool writeTextAtomic(const std::string& path, const std::string& text)
{
  // Запись во временный файл и переименование — файл не останется наполовину записанным
  const std::string tmp = path + ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << text;
    if (!f) return false;
  }
  std::error_code ec;
  fs::rename(tmp, path, ec);
  return !ec;
}

} // namespace

PresetLibrary::PresetLibrary(std::string rootDir)
: mRoot(rootDir.empty() ? defaultRoot() : std::move(rootDir))
{
  mPresetDir = mRoot + "/Presets";
  mMetaPath = mRoot + "/library.json";
  std::error_code ec;
  fs::create_directories(mPresetDir, ec);
  loadMeta();
  rescan();
}

std::string PresetLibrary::sanitizeFileName(const std::string& name)
{
  std::string s;
  for (char c : name)
  {
    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') s.push_back('_');
    else s.push_back(c);
  }
  while (!s.empty() && (s.front() == ' ' || s.front() == '.')) s.erase(s.begin());
  while (!s.empty() && s.back() == ' ') s.pop_back();
  if (s.empty()) s = "Preset";
  if (s.size() > 120) s.resize(120);
  return s;
}

void PresetLibrary::rescan()
{
  mUser.clear();
  mErrors.clear();
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(mPresetDir, ec))
  {
    if (!e.is_regular_file() || e.path().extension() != kPresetFileExtension) continue;
    std::string text, err;
    Preset p;
    if (!readText(e.path().string(), text) || !presetFromJson(text, p, err))
    {
      mErrors.push_back(e.path().filename().string() + ": " + (err.empty() ? "не удалось прочитать" : err));
      continue;
    }
    p.factory = false;
    p.path = e.path().string();
    p.name = e.path().stem().string(); // имя файла — источник истины для пользовательских пресетов
    if (p.category.empty()) p.category = "user";
    mUser.push_back(std::move(p));
  }
  std::sort(mUser.begin(), mUser.end(), [](const Preset& a, const Preset& b) { return a.name < b.name; });
}

bool PresetLibrary::save(const Preset& p, bool overwrite, std::string& error)
{
  const std::string fname = sanitizeFileName(p.name);
  const std::string path = mPresetDir + "/" + fname + kPresetFileExtension;
  if (!overwrite && fs::exists(path))
  {
    error = "Пресет с таким именем уже существует";
    return false;
  }
  Preset copy = p;
  copy.name = fname;
  copy.factory = false;
  if (!writeTextAtomic(path, presetToJson(copy)))
  {
    error = "Не удалось записать файл пресета";
    return false;
  }
  rescan();
  return true;
}

bool PresetLibrary::remove(const std::string& name, std::string& error)
{
  const std::string path = mPresetDir + "/" + sanitizeFileName(name) + kPresetFileExtension;
  std::error_code ec;
  if (!fs::remove(path, ec))
  {
    error = "Не удалось удалить пресет";
    return false;
  }
  setFavorite("user:" + name, false);
  rescan();
  return true;
}

bool PresetLibrary::rename(const std::string& oldName, const std::string& newName, std::string& error)
{
  const std::string from = mPresetDir + "/" + sanitizeFileName(oldName) + kPresetFileExtension;
  const std::string to = mPresetDir + "/" + sanitizeFileName(newName) + kPresetFileExtension;
  if (fs::exists(to))
  {
    error = "Пресет с таким именем уже существует";
    return false;
  }
  std::string text, err;
  Preset p;
  if (!readText(from, text) || !presetFromJson(text, p, err))
  {
    error = err.empty() ? "Не удалось прочитать пресет" : err;
    return false;
  }
  p.name = sanitizeFileName(newName);
  if (!writeTextAtomic(to, presetToJson(p)))
  {
    error = "Не удалось записать файл пресета";
    return false;
  }
  std::error_code ec;
  fs::remove(from, ec);
  if (isFavorite("user:" + oldName))
  {
    setFavorite("user:" + oldName, false);
    setFavorite("user:" + p.name, true);
  }
  rescan();
  return true;
}

bool PresetLibrary::importFile(const std::string& path, std::string& importedName, std::string& error)
{
  std::string text;
  Preset p;
  if (!readText(path, text))
  {
    error = "Не удалось открыть файл";
    return false;
  }
  if (!presetFromJson(text, p, error)) return false;
  std::string base = sanitizeFileName(p.name);
  std::string name = base;
  for (int i = 2; fs::exists(mPresetDir + "/" + name + kPresetFileExtension); ++i) name = base + " " + std::to_string(i);
  p.name = name;
  if (!save(p, false, error)) return false;
  importedName = name;
  return true;
}

bool PresetLibrary::exportFile(const Preset& p, const std::string& path, std::string& error)
{
  Preset copy = p;
  copy.factory = false;
  if (!writeTextAtomic(path, presetToJson(copy)))
  {
    error = "Не удалось записать файл";
    return false;
  }
  return true;
}

bool PresetLibrary::isFavorite(const std::string& id) const
{
  return std::find(mFavorites.begin(), mFavorites.end(), id) != mFavorites.end();
}

void PresetLibrary::setFavorite(const std::string& id, bool fav)
{
  auto it = std::find(mFavorites.begin(), mFavorites.end(), id);
  if (fav && it == mFavorites.end()) mFavorites.push_back(id);
  if (!fav && it != mFavorites.end()) mFavorites.erase(it);
  saveMeta();
}

void PresetLibrary::pushRecent(const std::string& id)
{
  mRecent.erase(std::remove(mRecent.begin(), mRecent.end(), id), mRecent.end());
  mRecent.insert(mRecent.begin(), id);
  if (mRecent.size() > 12) mRecent.resize(12);
  saveMeta();
}

void PresetLibrary::loadMeta()
{
  std::string text;
  if (!readText(mMetaPath, text)) return;
  try
  {
    const json j = json::parse(text);
    if (j.contains("favorites") && j["favorites"].is_array())
      for (const auto& f : j["favorites"])
        if (f.is_string()) mFavorites.push_back(f.get<std::string>());
    if (j.contains("recent") && j["recent"].is_array())
      for (const auto& f : j["recent"])
        if (f.is_string()) mRecent.push_back(f.get<std::string>());
  }
  catch (const std::exception&)
  {
    // Повреждённый файл избранного — начинаем с пустого списка (файл будет перезаписан)
  }
}

void PresetLibrary::saveMeta()
{
  json j;
  j["favorites"] = mFavorites;
  j["recent"] = mRecent;
  writeTextAtomic(mMetaPath, j.dump(2));
}

} // namespace tsl
