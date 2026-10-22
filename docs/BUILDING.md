# Сборка из исходников

## Требования

* Mac на Apple Silicon, macOS 12+ (разработка и проверка велись на macOS 26.6.2, M1).
* **Command Line Tools** (`xcode-select --install`). Полный Xcode не обязателен; если он установлен, но лицензия не
  принята, скрипты сами переключаются на CLT (`Scripts/env.sh`).
* **CMake ≥ 3.21** и **Ninja**. Без Homebrew их можно поставить из PyPI в отдельное окружение:
  ```bash
  python3 -m venv ~/.tonelab-tools && ~/.tonelab-tools/bin/pip install cmake ninja
  ```
  `Scripts/env.sh` автоматически добавляет `~/.tonelab-tools/bin` в `PATH`.
* git, доступ к github.com (загрузка закреплённых SDK).

## Чистая сборка

```bash
git clone https://github.com/twoonemoon/TwoonemoonSoundLab.git
cd TwoonemoonSoundLab
Scripts/bootstrap.sh        # iPlug2 (submodule по SHA), VST3 SDK v3.8.1_build_84, CLAP и clap-helpers по SHA
Scripts/build.sh Release    # ядро, тесты, AUv2, VST3, CLAP, standalone
Scripts/test.sh             # unit/integration тесты ядра
```

Результат: `build/release/out/TwoonemoonSoundLab.{app,component,vst3,clap}` (arm64, macOS 12.0+).
Пост-сборочный шаг iPlug2 дополнительно копирует плагины в `~/Library/Audio/Plug-Ins/{Components,VST3,CLAP}` и
приложение в `~/Applications` — удобно для отладки в DAW. Если потом устанавливаете пакет «для всех пользователей»,
удалите отладочные копии (`Scripts/uninstall.sh --user`), иначе DAW увидит плагин дважды.

Debug-сборка: `Scripts/build.sh Debug` → `build/debug/out`.

## Что делает `Scripts/env.sh`

* выбирает `DEVELOPER_DIR` (CLT, если Xcode недоступен);
* перебирает SDK от новых к старым и берёт первый, с которым реально линкуется тестовая программа
  (CLT-линкер ld-1267 не понимает tbd из SDK 27 — используется 26.5);
* задаёт deployment target 12.0 и архитектуру arm64.

## Тесты и инструменты

| Команда | Назначение |
|---|---|
| `Scripts/test.sh` | 68 тестов ядра (DSP, модули, движок, пресеты, MIDI, тюнер, кабинет) |
| `build/tests/Tests/tsl_bench` | CPU и задержка |
| `build/tests/Tests/tsl_render --calibrate` | уровни всех заводских пресетов на синтетическом сигнале |
| `build/tests/Tests/tsl_render --render <папка>` | WAV-рендеры пресетов для прослушивания |
| `Scripts/install-validators.sh` | один раз: VST3 validator (из SDK), pluginval 1.0.4, clap-validator 0.4.1 в `~/.tonelab-tools` |
| `Scripts/validate.sh` | auval -strict, VST3 validator, pluginval (VST3/AU, strictness 10), clap-validator |
| `Scripts/screenshot.sh out.png <0–8>` | скриншот страницы standalone (без аудио) |
| `Scripts/package.sh` | единый установщик `dist/TwoonemoonSoundLab-Installer.pkg` (см. RELEASE.md) |

## Структура CMake

* Корневой `CMakeLists.txt`: библиотека `TwoonemoonCore` (без iPlug2) и тесты; опция `TSL_BUILD_PLUGIN` (ON по умолчанию).
* `Source/Plugin/CMakeLists.txt`: `iplug_add_plugin` — цели `TwoonemoonSoundLab-app`, `-vst3`, `-clap`, `-au`;
  IGraphics NanoVG + Metal (предкомпилированные шейдеры, компилятор `metal` не нужен).
* Standalone собирается без `MainMenu.xib`: меню и делегат создаются в `Source/Standalone/AppBootstrap.mm`
  (`ibtool` есть только в полном Xcode).

## Частые проблемы сборки

См. [TROUBLESHOOTING.md](TROUBLESHOOTING.md#сборка).
