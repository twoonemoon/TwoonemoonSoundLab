# Установка и удаление

## Единый установщик

Артефакт: **`dist/TwoonemoonSoundLab-Installer.pkg`** (собирается `Scripts/package.sh`), рядом —
`SHA256SUMS.txt` и `Uninstall TwoonemoonSoundLab.command`. Один пакет ставит все форматы; лишние можно отключить
на шаге «Тип установки → Настроить».

| Компонент | Для всех пользователей (по умолчанию) | Только для меня |
|---|---|---|
| Приложение | `/Applications/TwoonemoonSoundLab.app` | `~/Applications/TwoonemoonSoundLab.app` |
| Audio Unit (AUv2) | `/Library/Audio/Plug-Ins/Components/TwoonemoonSoundLab.component` | `~/Library/Audio/Plug-Ins/Components/…` |
| VST3 | `/Library/Audio/Plug-Ins/VST3/TwoonemoonSoundLab.vst3` | `~/Library/Audio/Plug-Ins/VST3/…` |
| CLAP | `/Library/Audio/Plug-Ins/CLAP/TwoonemoonSoundLab.clap` | `~/Library/Audio/Plug-Ins/CLAP/…` |
| Скрипт удаления | `/Library/Application Support/TwoonemoonSoundLab/Uninstall TwoonemoonSoundLab.command` | `~/Library/Application Support/TwoonemoonSoundLab/…` |

**Схема размещения.** По умолчанию — системные стандартные папки, которые видят все DAW и все пользователи;
macOS запросит пароль администратора (об этом сказано на первом экране установщика). Вариант без пароля —
«Установить только для меня» на шаге выбора места: те же папки внутри домашней.

**Требования.** Mac с Apple Silicon (проверяется установщиком: `hostArchitectures=arm64` и проверка
`hw.optional.arm64`), macOS 12+. На неподходящей системе установка не начинается и ничего не меняет.

### Как установить
1. Откройте `TwoonemoonSoundLab-Installer.pkg` двойным щелчком и следуйте шагам.
2. Из Терминала: `Scripts/install.sh` (для всех, с `sudo`) или `Scripts/install.sh --user` (только для себя);
   скрипт сначала сверяет SHA-256 пакета.

### Обновление
Повторный запуск новой версии заменяет компоненты. Перед установкой каждого компонента скрипт `preinstall`:
* удаляет предыдущую версию **только если** по целевому пути лежит бандл TwoonemoonSoundLab (проверка
  `CFBundleIdentifier`) — так не остаётся старых файлов;
* **останавливает установку**, если там посторонний бандл с другим идентификатором: ничего не перезаписывается,
  установщик показывает ошибку, установка откатывается.

Бандлы помечены как неперемещаемые (`BundleIsRelocatable=false`): установщик не «обновляет» случайную копию с тем же
идентификатором в другом месте (например, отладочную сборку). После установки AU системный кэш Audio Units обновляется
(перезапуск служебного процесса `AudioComponentRegistrar`). Настройки DAW не изменяются.

### Пользовательские данные
Пресеты, избранное, настройки интерфейса и ссылки на IR хранятся в
`~/Library/Application Support/TwoonemoonSoundLab` (`Presets/*.tslpreset`, `library.json`, `settings.json`).
Установщик и обновление их **не трогают**. Пользовательские WAV-файлы IR остаются там, где вы их храните;
проект запоминает путь.

### Bundle ID
`com.twoonemoon.app.TwoonemoonSoundLab`, `com.twoonemoon.audiounit.TwoonemoonSoundLab`,
`com.twoonemoon.vst3.TwoonemoonSoundLab`, `com.twoonemoon.clap.TwoonemoonSoundLab`. AU: тип `aumf`, подтип `TmSL`,
производитель `Twmn`. Идентификаторы пакетов: `com.twoonemoon.TwoonemoonSoundLab.pkg.{app,au,vst3,clap,support}`.

## После установки: обнаружение в DAW

Плагин не обязан появиться мгновенно — каждая DAW сканирует плагины по-своему:

| Хост | Что сделать |
|---|---|
| Logic Pro / GarageBand / MainStage | перезапустить; AU проверяется автоматически (Logic: «Настройки → Плагины» — менеджер AU). Производитель — Twoonemoon |
| Ableton Live | «Настройки → Плагины»: включить «Use VST3 Plug-in System Folders», «Rescan» |
| Reaper | «Preferences → Plug-ins → VST» / «CLAP»: «Re-scan» |
| Bitwig Studio | VST3 и CLAP подхватываются после перезапуска |
| Cubase / Studio One | менеджер плагинов → обновить список |

## Gatekeeper и подпись

Локальные/тестовые сборки подписаны **ad-hoc** (без Developer ID) и не нотаризованы. На Mac, где пакет собран, это
не мешает. На другом Mac macOS может сообщить, что не удаётся проверить разработчика:
* для пакета — правый клик по `.pkg` → «Открыть» (или «Системные настройки → Конфиденциальность и безопасность →
  Всё равно открыть»);
* аналогично для приложения при первом запуске.

Отключать Gatekeeper целиком не нужно и не рекомендуется. Сборки, подписанные Developer ID и нотаризованные, таких
предупреждений не вызывают — процедура описана в [RELEASE.md](RELEASE.md). Текущий пакет **не** подписан Developer ID.

## Удаление

Запустите `Uninstall TwoonemoonSoundLab.command` (двойной щелчок; лежит в папке установки, см. таблицу, и рядом с
`.pkg`) или из Терминала:

```bash
Scripts/uninstall.sh            # обе схемы установки; для системных папок запросит пароль
Scripts/uninstall.sh --user     # только установка «для меня»
Scripts/uninstall.sh --system   # только системная установка
Scripts/uninstall.sh --purge    # дополнительно удалить пресеты и настройки (с подтверждением)
```

Скрипт удаляет только бандлы с идентификаторами TwoonemoonSoundLab, «забывает» квитанции установщика
(`pkgutil --forget`) и сообщает результат. Пользовательские данные сохраняются, если не указан `--purge`.
