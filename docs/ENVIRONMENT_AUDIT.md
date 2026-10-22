# Аудит окружения

Дата аудита: 2026-10-22. Все версии получены командами на машине разработки, не из документации.

| Компонент | Фактическое значение | Как проверено |
|---|---|---|
| CPU | Apple M1, 8 ядер, 16 ГБ | `sysctl machdep.cpu.brand_string hw.ncpu hw.memsize` |
| Архитектура | arm64 | `uname -m` |
| macOS | 26.6.2 (25G83) | `sw_vers` |
| Xcode | 27.0 (27A266a) установлен, **лицензия не принята** | `xcodebuild -version` → требование `sudo xcodebuild -license` |
| Command Line Tools | Apple clang 21.0.0 (clang-2100.1.1.101), ld-1267 | `DEVELOPER_DIR=/Library/Developer/CommandLineTools clang --version`, `ld -v` |
| SDK | MacOSX 15.x, 26, 26.5, 27.0 в CLT | `ls /Library/Developer/CommandLineTools/SDKs` |
| Рабочий SDK | **MacOSX26.5** | линкер CLT (ld-1267) не понимает `arm64e.x1` в `libSystem.tbd` SDK 27.0 → тестовая сборка падает; 26.5 линкуется |
| CMake | 4.4.4 (PyPI-колесо Kitware, venv `~/.tonelab-tools`) | `cmake --version` |
| Ninja | 1.13.2 (PyPI) | `ninja --version` |
| Homebrew | установлен, **не работает** без принятой лицензии Xcode | `brew install` → отказ |
| GitHub CLI | 2.103.0 (официальный релиз cli/cli, arm64) | `gh --version` |
| git | 2.50.1 (Apple Git-155, через CLT) | `git --version` |
| ibtool / actool / metal | недоступны (требуют полный Xcode) | `xcode-select: tool 'ibtool' requires Xcode` |
| auval | `/usr/bin/auval` — работает | `auval -strict -v aumf TmSL Twmn` → PASS |
| VST3 validator | собран из VST3 SDK v3.8.1 (`Scripts/install-validators.sh`) | `validator` → 47 passed, 0 failed |
| clap-validator | 0.4.1 (официальный релиз free-audio, aarch64) | 38 passed, 0 failed |
| pluginval | 1.0.4 (официальный релиз Tracktion) | strictness 10: SUCCESS (VST3, AU) |
| pkgbuild / productbuild / iconutil | системные (`/usr/bin`) | сборка установщика и иконки |
| swift | Apple Swift 6.3.3 (CLT) | генерация иконки (`Scripts/tools/make_icon.swift`) |

## Фреймворк и SDK (закреплены)

| Зависимость | Версия | Лицензия |
|---|---|---|
| iPlug2 | submodule, commit `39ea73aafe6f62fa0b52b9c633d2a480f5abb353` (2026-10-18) | zlib-like (`LICENSE.txt`) |
| VST3 SDK | `v3.8.1_build_84` | MIT (с 3.8) |
| CLAP SDK | commit `a47f6badb49d948fd009998f28309cdab78979c9` (main) | MIT |
| clap-helpers | commit `a380c978f429103e08afc1d17e9efbf3e3c96ee2` | MIT |

## Последствия для сборки
- Сборка рассчитана на **Command Line Tools без полного Xcode**: `Scripts/env.sh` выбирает `DEVELOPER_DIR` и
  самый новый SDK, с которым реально линкуется тестовая программа.
- `ibtool` недоступен → standalone собирается без `MainMenu.xib` (меню создаётся в коде).
- NanoVG/Metal использует предкомпилированные шейдеры из iPlug2 → компилятор `metal` не нужен.
- Если владелец машины примет лицензию Xcode (`sudo xcodebuild -license accept`), сборка продолжит работать.

## Тестовая сборка (выполнена)
`Scripts/build.sh Release` → `build/release/out/TwoonemoonSoundLab.{app,component,vst3,clap}` (arm64).
