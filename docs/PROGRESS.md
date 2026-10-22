# Прогресс

Статусы: **implemented** — реализовано и проверено тестом/сборкой/валидатором; **partial** — работает, но не в полном
объёме; **planned** — не начато; **blocked** — требует внешнего действия.

## Итоговый статус (2026-10-22, версия 0.1.0)

Все функции из ТЗ реализованы, собираются и проходят автоматические проверки. Продукт готов к **ручному
тестированию**, но не к публичному релизу: звучание не прослушано на гитаре, нет ручной проверки в DAW, нет подписи
Developer ID и нотаризации.

## Артефакты

| Артефакт | Путь |
|---|---|
| Standalone | `build/release/out/TwoonemoonSoundLab.app` |
| AUv2 | `build/release/out/TwoonemoonSoundLab.component` |
| VST3 | `build/release/out/TwoonemoonSoundLab.vst3` |
| CLAP | `build/release/out/TwoonemoonSoundLab.clap` |
| Установщик | `dist/TwoonemoonSoundLab-Installer.pkg` (12 МБ) + `dist/SHA256SUMS.txt` + `dist/Uninstall TwoonemoonSoundLab.command` |

Все бандлы arm64, macOS 12.0+, версия 0.1.0. Каталоги `build/` и `dist/` не хранятся в git.

## Компоненты

| Компонент | Статус | Примечание |
|---|---|---|
| Сборка CMake/Ninja на Command Line Tools | implemented | чистая сборка 49 с |
| AUv2 / VST3 / CLAP / standalone | implemented | auval PASS, VST3 47/0, pluginval SUCCESS ×2, clap 38/0 |
| Параметры (261), автоматизация, состояние по ключам | implemented | append-only, версия формата |
| Input/Output, Bypass, Input Mode, измерители, защита от NaN/перегрузки | implemented | |
| Цепочка: перетаскивание, вкл/выкл, параллельная секция A/B | implemented | lock-free публикация |
| Noise Gate, Compressor | implemented | |
| Boost, Overdrive, Distortion, Fuzz | implemented | оверсэмплинг 2x/4x |
| 12 моделей усилителей + фасады | implemented | требуют прослушивания |
| Cab Studio: 6 кабинетов × 3 микрофона, A/B, пользовательские IR | implemented | синтезированные IR; IR-файлы по пути, без копирования в библиотеку |
| EQ 5 полос + анализатор | implemented | |
| Delay ×5, Reverb ×5 + Freeze | implemented | |
| Chorus, Phaser, Flanger, Tremolo, Vibrato, Rotary, Envelope Filter | implemented | |
| Wah, Octave, Pitch Shifter, Harmonizer (4 голоса) | implemented | голоса со сдвигом: 20–40 мс задержки (свойство алгоритма) |
| Stereo Widener, Output Limiter, Looper, метроном | implemented | |
| Тюнер | implemented | |
| 60 заводских пресетов, библиотека, избранное, поиск, импорт/экспорт, A/B, undo/redo | implemented | громкость выровнена |
| MIDI Learn, педаль экспрессии | implemented | проверено тестами, не с контроллером |
| Интерфейс: 9 страниц, тюнер, знакомство, справка, подсказки для всех параметров | implemented | |
| Масштаб 50–250%, компактный режим 50% | implemented | |
| Иконка приложения | implemented | собственная, генерируется кодом |
| Установщик .pkg, обновление, удаление, контрольные суммы | implemented | проверено в домене пользователя |
| Подпись Developer ID / нотаризация | blocked | процедура готова (`TSL_SIGN_*`, `TSL_NOTARY_PROFILE`), нужны учётные данные владельца |
| Документация | implemented | README, CLAUDE, docs/* (15 документов), CHANGELOG |

## Известные ограничения

* Звучание не подтверждено на реальной гитаре; возможна подстройка моделей после прослушивания.
* Встроенные кабинеты синтетические (не измерения). Пользовательские IR хранятся по пути — при перемещении файла
  слот покажет ошибку.
* Harmonizer — для одноголосных линий; Octave Mono — для одиночных нот.
* Limiter без lookahead: межсэмпловые пики срезаются финальным клиппером.
* Нет пресетов-«расширений» CLAP (preset-discovery), configurable audio ports — необязательные расширения.
* Перетаскивание, ввод текста и меню мышью проверены только косвенно (pluginval GUI-тесты, скриншоты).
* Протестировано только на macOS 26.6.2 / M1; минимальная заявленная версия 12.0 фактически не проверялась.
* Пост-сборочный шаг iPlug2 копирует отладочные плагины в `~/Library/Audio/Plug-Ins`; при системной установке пакета
  удалите их (`Scripts/uninstall.sh --user`), иначе DAW покажет плагин дважды.

## Список на прослушивание (audition)

1. **Усилители** — все 12 моделей при Gain 2/5/8 и Master 3/7; особенно Velvet Edge (отклик на атаку),
   Iron Veil/Catacomb (собранность низа, palm-mute), Abyssal Lead (sustain), акустические модели с пьезо-датчиком.
2. **Bias / Sag / Feedback / Presence / Depth** — заметность и музыкальность диапазонов.
3. **Кабинеты** — 6 динамиков × 3 микрофона, перемещение микрофона, сочетания A+B (фаза, Delay).
4. **Педали** — Overdrive (Transparent / Mid Focus), Distortion (Silicon / LED / Hard), Fuzz (Germanium / Silicon /
   Octave), Boost Tight перед high-gain.
5. **Гейт** на high-gain с ключом Input DI — обрывы затухающих нот.
6. **Эффекты** — Tape/Analog delay (wow/flutter), Spring и Shimmer reverb, Rotary (разгон), Envelope Filter, Phaser 12 ступеней.
7. **Pitch / Octave Poly / Harmonizer** — артефакты склеек, задержка голосов в игре.
8. **Пресеты** — все 60, в первую очередь кнопки «Быстрый старт»: Glass Morning, Iron Veil Chug, Misty Valley, Silver Stage.
9. **Переключение пресетов и модулей во время игры** — отсутствие щелчков.

## Следующие шаги

1. Ручное тестирование владельцем: прослушивание по списку выше, DAW (Logic + Reaper/Ableton), standalone с интерфейсом.
2. Подстройка моделей и пресетов по результатам прослушивания; повторная калибровка (`tsl_render --calibrate`).
3. Измерение round-trip задержки через loopback.
4. Подпись Developer ID и нотаризация, когда будут учётные данные.
