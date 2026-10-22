# Лицензии и ассеты

## Проект

TwoonemoonSoundLab распространяется под **GPL-3.0-or-later** (`LICENSE`). Весь DSP, интерфейс, модели усилителей и
кабинетов, пресеты, тексты и графика интерфейса — собственная работа проекта. Совместимость лицензий зависимостей —
ADR-001/002 в DECISIONS.md.

## Код третьих сторон

| Компонент | Версия | Лицензия | Использование | Распространяется в бинарниках |
|---|---|---|---|---|
| iPlug2 | commit 39ea73aafe6f62fa0b52b9c633d2a480f5abb353 | zlib-like | обёртки форматов, IGraphics, standalone | да |
| WDL / SWELL (Cockos) | в составе iPlug2 | zlib-like | утилиты, окно standalone | да |
| NanoVG, NanoSVG | в составе iPlug2 | zlib | векторная отрисовка UI | да |
| MetalNanoVG | в составе iPlug2 | MIT | Metal-бэкенд NanoVG | да |
| RtAudio, RtMidi | в составе iPlug2 | MIT | аудио и MIDI standalone | да |
| nlohmann/json 3.12.0 | в составе iPlug2 (Dependencies/Extras) | MIT | формат пресетов и настроек | да |
| VST3 SDK | v3.8.1_build_84 | MIT | VST3-обёртка | да |
| CLAP | commit a47f6badb49d948fd009998f28309cdab78979c9 | MIT | CLAP-обёртка | да |
| clap-helpers | commit a380c978f429103e08afc1d17e9efbf3e3c96ee2 | MIT | CLAP-обёртка | да |
| Apple Accelerate (vDSP), CoreAudio, AppKit, Metal | системные фреймворки macOS | системная лицензия Apple | FFT, аудио, окно, отрисовка | нет (динамическая связь) |

Тексты лицензий — в исходниках соответствующих компонентов (`ThirdParty/iPlug2/LICENSE.txt`, LICENSE-файлы SDK после
`Scripts/bootstrap.sh`). Краткий список — `THIRD_PARTY_NOTICES.md`.

## Шрифты

| Шрифт | Версия | Лицензия | Файлы |
|---|---|---|---|
| Inter | 4.1 (github.com/rsms/inter) | SIL OFL 1.1 | `Resources/Fonts/Inter-{Regular,Medium,SemiBold,Bold}.ttf`, `Inter-OFL.txt` |
| JetBrains Mono | 2.304 (github.com/JetBrains/JetBrainsMono) | SIL OFL 1.1 | `Resources/Fonts/JetBrainsMono-{Regular,Medium}.ttf`, `JetBrainsMono-OFL.txt` |

Шрифты встроены в бинарники как ресурсы и распространяются с текстом лицензии OFL.

## Графика интерфейса

Вся графика рисуется кодом (`Source/UI/Theme.cpp`, `Controls.cpp`, `AmpFaces.cpp`): металл, перламутр, рельеф,
фасады усилителей, педали, иконки. Иконка приложения (`Resources/Icon/`) генерируется скриптом
`Scripts/tools/make_icon.swift` (CoreGraphics). Растровых изображений, иконок и SVG сторонних авторов нет.

## Импульсные отклики и звуковые ресурсы

Файлов IR и сэмплов в поставке нет; встроенные кабинеты синтезируются кодом — см. `Resources/IR/ASSET_MANIFEST.md`.
Пресеты (`Source/Presets/FactoryPresets.cpp`) — собственные.

## Визуальные референсы

`design/references/` — 6 изображений, предоставленных владельцем проекта как mood board. Они не входят в продукт,
не встраиваются в бинарники и не распространяются как ассеты; права на них остаются у правообладателей.

## Инструменты разработки (не распространяются)

pluginval 1.0.4 (Tracktion, GPL-3.0), clap-validator 0.4.1 (free-audio, MIT), VST3 validator (из VST3 SDK, MIT),
CMake и Ninja (PyPI-колёса Kitware/Ninja, BSD/Apache-2.0) — устанавливаются в `~/.tonelab-tools` только для сборки и
проверки.

## Товарные знаки

«VST» — товарный знак Steinberg Media Technologies GmbH. «Audio Units», «Logic Pro», «GarageBand», «MainStage»,
«macOS», «Apple Silicon» — товарные знаки Apple Inc. «CLAP» — проект free-audio. Названия DAW упоминаются только для
инструкций по совместимости. Модели усилителей, педалей и кабинетов имеют оригинальные имена и не имитируют продукты
других производителей.
