// SPDX-License-Identifier: GPL-3.0-or-later
// Пресеты: модель данных, JSON-формат (версионированный, мигрируемый), заводской банк,
// пользовательская библиотека (файлы, избранное, недавние). Всё — вне audio thread.
#pragma once

#include "../Audio/Engine.h"

#include <map>
#include <string>
#include <vector>

namespace tsl {

constexpr int kPresetFormatVersion = 1;
constexpr const char* kPresetFileExtension = ".tslpreset";

struct Preset
{
  std::string name;
  std::string category;    // ключ категории (см. presetCategories())
  std::string description; // краткое описание (рус.)
  std::string author;
  std::vector<std::string> tags;
  std::map<std::string, float> params; // ключ параметра → plain-значение (отсутствующие = default)
  bool hasChain = false;
  ChainConfig chain;
  std::string irA, irB; // пути пользовательских IR
  bool factory = false;
  std::string path; // файл пользовательского пресета

  /** Стабильный идентификатор для избранного/недавних. */
  std::string id() const { return (factory ? "factory:" : "user:") + name; }
};

struct PresetCategory
{
  const char* key;
  const char* title; // рус.
};
const std::vector<PresetCategory>& presetCategories();
const char* presetCategoryTitle(const std::string& key);

/** Значения всех параметров: default + переопределения пресета (с ограничением диапазонов). */
ParamValues resolvePresetParams(const Preset& p);
/** Снимок текущего состояния в пресет (все параметры). */
Preset makePresetFromState(const ParamValues& values, const ChainConfig& chain, const std::string& irA, const std::string& irB);

std::string presetToJson(const Preset& p);
/** Разбор с проверкой формата и миграцией версий; при ошибке — false и описание (рус.). */
bool presetFromJson(const std::string& text, Preset& out, std::string& error);

/** Заводской банк (встроен в бинарник — его нельзя случайно удалить или повредить). */
const std::vector<Preset>& factoryPresets();

/** Пользовательская библиотека: ~/Library/Application Support/TwoonemoonSoundLab. */
class PresetLibrary
{
public:
  explicit PresetLibrary(std::string rootDir = {});

  const std::string& presetDir() const { return mPresetDir; }
  /** Пересканировать пользовательские пресеты. Повреждённые файлы пропускаются и попадают в errors(). */
  void rescan();
  const std::vector<Preset>& userPresets() const { return mUser; }
  const std::vector<std::string>& errors() const { return mErrors; }

  /** Сохранить пользовательский пресет (имя = имя файла). overwrite=false — не перезаписывать существующий. */
  bool save(const Preset& p, bool overwrite, std::string& error);
  bool remove(const std::string& name, std::string& error);
  bool rename(const std::string& oldName, const std::string& newName, std::string& error);
  /** Импорт файла пресета (проверка формата, уникальное имя при совпадении). */
  bool importFile(const std::string& path, std::string& importedName, std::string& error);
  bool exportFile(const Preset& p, const std::string& path, std::string& error);

  // Избранное и недавние
  bool isFavorite(const std::string& id) const;
  void setFavorite(const std::string& id, bool fav);
  const std::vector<std::string>& recent() const { return mRecent; }
  void pushRecent(const std::string& id);

  /** Безопасное имя файла из имени пресета. */
  static std::string sanitizeFileName(const std::string& name);

private:
  void loadMeta();
  void saveMeta();
  std::string mRoot, mPresetDir, mMetaPath;
  std::vector<Preset> mUser;
  std::vector<std::string> mErrors;
  std::vector<std::string> mFavorites, mRecent;
};

} // namespace tsl
