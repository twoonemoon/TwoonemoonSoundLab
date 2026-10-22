# Выпуск версии

## Версионирование

Семантические версии `MAJOR.MINOR.PATCH`. Источник истины — `Source/Plugin/config.h`:
`PLUG_VERSION_STR` и `PLUG_VERSION_HEX` (меняются вместе; из них берутся версии бандлов, AU и пакета).
До 1.0 параметры могут добавляться только в конец реестра; после 1.0 индексы и ключи параметров неизменны
(иначе ломается автоматизация в проектах пользователей). Изменения формата состояния — с повышением его версии
и миграцией.

## Чек-лист релиза

1. Обновить версию в `config.h`, раздел в `CHANGELOG.md`, `docs/PROGRESS.md`.
2. Чистая сборка: `rm -rf build && Scripts/build.sh Release`.
3. `Scripts/test.sh` — 0 ошибок.
4. `build/tests/Tests/tsl_render --calibrate` — уровни пресетов в пределах ±1 дБ, без NaN/клипа.
5. `Scripts/validate.sh` — auval, VST3 validator, pluginval (VST3/AU), clap-validator.
6. `Scripts/package.sh` → `dist/TwoonemoonSoundLab-Installer.pkg`, `SHA256SUMS.txt`.
7. Проверка установки на чистой учётной записи: установка, запуск, обнаружение в DAW, обновление поверх, удаление.
8. Тег `vX.Y.Z`, релиз на GitHub с `.pkg`, `SHA256SUMS.txt`, скриптом удаления и выдержкой из CHANGELOG.

## Сценарий 1 — локальная/тестовая сборка (по умолчанию)

```bash
Scripts/package.sh            # со сборкой
Scripts/package.sh --no-build # из уже собранных артефактов
```

Что делает скрипт:
* проверяет каждый бандл: `CFBundleIdentifier`, версию (= `config.h`), наличие arm64 (`lipo`);
* подписывает бандлы **ad-hoc** (`codesign --force --sign -`) — это полноценная подпись с запечатанными ресурсами,
  `codesign --verify --strict` проходит; но это не Developer ID;
* собирает компонентные пакеты (`pkgbuild`, неперемещаемые бандлы, скрипты pre/postinstall) и единый
  `productbuild`-пакет с выбором компонентов;
* пишет `dist/SHA256SUMS.txt` и кладёт рядом скрипт удаления.

Ограничение: такой пакет на другом Mac вызывает предупреждение Gatekeeper (см. INSTALLATION.md).

## Сценарий 2 — публичный релиз (Developer ID + нотаризация)

Нужны: членство в Apple Developer Program, сертификаты «Developer ID Application» и «Developer ID Installer» в связке
ключей, профиль `notarytool`. **Секреты не хранятся в репозитории** — только переменные окружения/секреты CI.

```bash
# один раз: сохранить учётные данные нотаризации в связке ключей
xcrun notarytool store-credentials "tsl-notary" --apple-id "<apple-id>" --team-id "<TEAMID>" --password "<app-specific-password>"

TSL_SIGN_APP="Developer ID Application: <Имя> (<TEAMID>)" \
TSL_SIGN_INSTALLER="Developer ID Installer: <Имя> (<TEAMID>)" \
TSL_NOTARY_PROFILE="tsl-notary" \
Scripts/package.sh
```

С этими переменными скрипт подписывает бандлы с hardened runtime и меткой времени, подписывает `.pkg`,
отправляет его в нотаризацию (`notarytool submit --wait`), прикрепляет тикет (`stapler staple`) и проверяет его.

Проверка результата (только по факту выполнения этих команд можно заявлять «подписано/нотаризовано»):

```bash
pkgutil --check-signature dist/TwoonemoonSoundLab-Installer.pkg
spctl --assess --type install -vv dist/TwoonemoonSoundLab-Installer.pkg
xcrun stapler validate dist/TwoonemoonSoundLab-Installer.pkg
codesign --verify --strict --deep -vv /Applications/TwoonemoonSoundLab.app   # после установки
```

Статус текущих сборок: **не подписаны Developer ID, не нотаризованы** — учётные данные разработчика не предоставлены.

## Состав релиза

| Файл | Назначение |
|---|---|
| `TwoonemoonSoundLab-Installer.pkg` | единый установщик (app, AU, VST3, CLAP) |
| `SHA256SUMS.txt` | контрольные суммы |
| `Uninstall TwoonemoonSoundLab.command` | удаление (также устанавливается пакетом) |
| исходный код (тег) | GPL-3.0-or-later требует доступности исходников |
